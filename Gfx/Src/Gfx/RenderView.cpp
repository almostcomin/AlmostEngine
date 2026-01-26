#include "Gfx/RenderView.h"
#include "Gfx/RenderStage.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Core/Log.h"
#include "Interop/RenderResources.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Util.h"

namespace
{

int GetActualSize(int desired, int ref)
{
	if (desired > 0)
	{
		return desired;
	}

	return ref / (-desired + 1);
}

st::rhi::ResourceState GetInitialState(st::gfx::RenderView::TextureResourceType type)
{
	switch (type)
	{
	case st::gfx::RenderView::TextureResourceType::RenderTarget:
		return st::rhi::ResourceState::RENDERTARGET;
	case st::gfx::RenderView::TextureResourceType::DepthStencil:
		return st::rhi::ResourceState::DEPTHSTENCIL;
	case st::gfx::RenderView::TextureResourceType::ShaderResource:
		return st::rhi::ResourceState::SHADER_RESOURCE;
	default:
		assert(0);
		return st::rhi::ResourceState::SHADER_RESOURCE;
	}
}

st::rhi::TextureShaderUsage GetTextureShaderUsage(st::gfx::RenderView::TextureResourceType type, bool needUAV)
{
	st::rhi::TextureShaderUsage usage;
	switch (type)
	{
	case st::gfx::RenderView::TextureResourceType::RenderTarget:
		usage = st::rhi::TextureShaderUsage::ShaderResource | st::rhi::TextureShaderUsage::RenderTarget;
		break;
	case st::gfx::RenderView::TextureResourceType::DepthStencil:
		usage = st::rhi::TextureShaderUsage::ShaderResource | st::rhi::TextureShaderUsage::DepthStencil;
		break;
	case st::gfx::RenderView::TextureResourceType::ShaderResource:
		usage = st::rhi::TextureShaderUsage::ShaderResource;
		break;
	default:		
		assert(0);
		usage = st::rhi::TextureShaderUsage::ShaderResource;
	}
	if (needUAV)
		usage |= st::rhi::TextureShaderUsage::UnorderedAccess;
	
	return usage;
}

} // anonymous namespace

st::gfx::RenderView::RenderView(DeviceManager* deviceManager, const char* debugName) :
	m_IsDirty{ false }, m_DebugName{ debugName }, m_DeviceManager { deviceManager }
{
	rhi::CommandListParams params{
		.queueType = rhi::QueueType::Graphics
	};
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
	{
		m_CommandLists.push_back(m_DeviceManager->GetDevice()->CreateCommandList(params, m_DebugName));
	}
}

st::gfx::RenderView::~RenderView()
{
	ClearRenderStages();
	m_DeviceManager->GetDevice()->ReleaseQueued(m_SceneCB);

	for (int i = 0; i < m_CommandLists.size(); ++i)
	{
		m_DeviceManager->GetDevice()->ReleaseImmediately(m_CommandLists[i]);
	}
}

void st::gfx::RenderView::SetScene(st::weak<Scene> scene)
{ 
	m_Scene = scene;
	for (auto rs : m_RenderStages)
	{
		rs->renderStage->OnSceneChanged();
	}
}

void st::gfx::RenderView::SetCamera(std::shared_ptr<st::gfx::Camera> camera)
{
	m_Camera = camera;
}

void st::gfx::RenderView::SetOffscreenFrameBuffer(st::rhi::FramebufferHandle frameBuffer)
{
	m_OffscreenFramebuffer = frameBuffer;
}

void st::gfx::RenderView::SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages)
{
	ClearRenderStages();

	m_RenderStages.reserve(renderStages.size());
	for (auto& rs : renderStages)
	{
		auto rsd = new RenderStageData{ {}, {}, rs, {} };
		rsd->timerQueries.reserve(m_DeviceManager->GetSwapchainBufferCount());
		for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
		{
			rsd->timerQueries.push_back(m_DeviceManager->GetDevice()->CreateTimerQuery(
				std::format("{} - TimerQuery", rs->GetDebugName())));
		}

		m_RenderStages.push_back(rsd);
	}

	for (auto rs : m_RenderStages)
	{
		rs->renderStage->Attach(this);
	}

	m_IsDirty = true;
}

st::rhi::FramebufferHandle st::gfx::RenderView::GetFramebuffer()
{
	return m_OffscreenFramebuffer ? m_OffscreenFramebuffer : m_DeviceManager->GetCurrentFramebuffer();
}

st::rhi::TextureHandle st::gfx::RenderView::GetBackBuffer(int idx)
{
	return GetFramebuffer()->GetBackBuffer(idx);
}

st::rhi::CommandListHandle st::gfx::RenderView::GetCommandList()
{
	return m_CommandLists[m_DeviceManager->GetFrameModuleIndex()].get_weak();
}

st::rhi::DescriptorIndex st::gfx::RenderView::GetSceneConstantBufferDI()
{
	return m_SceneCB ? m_SceneCB->GetShaderViewIndex(rhi::BufferShaderView::ConstantBuffer) : rhi::c_InvalidDescriptorIndex;
}

bool st::gfx::RenderView::CreateColorTarget(const char* id, int width, int height, int arraySize, rhi::Format format)
{
	return CreateTexture(id, TextureResourceType::RenderTarget, width, height, arraySize, format, false);
}

bool st::gfx::RenderView::CreateDepthTarget(const char* id, int width, int height, int arraySize, rhi::Format format)
{
	return CreateTexture(id, TextureResourceType::DepthStencil, width, height, arraySize, format, false);
}

bool st::gfx::RenderView::CreateTexture(const char* id, TextureResourceType type, int width, int height, int arraySize, rhi::Format format, bool needUAV)
{
	// Check that texture has not been already created
	auto it = m_DeclaredTextures.find(id);
	if (it != m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} already created", id);
		return false;
	}

	rhi::TextureDesc desc{
		.width = (uint32_t)GetActualSize(width, GetFramebuffer()->GetFramebufferInfo().width),
		.height = (uint32_t)GetActualSize(height, GetFramebuffer()->GetFramebufferInfo().height),
		.arraySize = (uint32_t)arraySize,
		.format = format,
		.shaderUsage = GetTextureShaderUsage(type, needUAV) };

	// Created in common state
	rhi::TextureOwner texture = m_DeviceManager->GetDevice()->CreateTexture(desc, GetInitialState(type), id);
	m_DeclaredTextures.insert({ id, std::make_unique<DeclaredTexture>(std::move(texture), id, type, width, height) });

	return true;
}

bool st::gfx::RenderView::RecreateTexture(const char* id, int width, int height, int arraySize, rhi::Format format)
{
	// Check that texture has been already created
	auto it = m_DeclaredTextures.find(id);
	if (it == m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} does not exist", id);
		return false;
	}

	rhi::TextureDesc desc = it->second->texture->GetDesc();
	desc.width = width;
	desc.height = height;
	desc.arraySize = arraySize;
	desc.format = format;

	rhi::TextureOwner newTexture = m_DeviceManager->GetDevice()->CreateTexture(desc, GetInitialState(it->second->type), id);
	rhi::TextureOwner& oldTexture = it->second->texture;

	// Swap
	oldTexture->Swap(*newTexture.get());

	// newTexture (actually the old old since it has been swap-ed) would be released when the owner pointer gets out of scope
	// but lets do it explicitly
	m_DeviceManager->GetDevice()->ReleaseQueued(newTexture);

	it->second->requestedWidth = width;
	it->second->requestedHeight = height;

	return true;
}

bool st::gfx::RenderView::ReleaseTexture(const char* id)
{
	// Check that the texture exists
	auto it = m_DeclaredTextures.find(id);
	if (it == m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} does not exist", id);
		return false;
	}

	m_DeclaredTextures.erase(it);
	return true;
}

bool st::gfx::RenderView::RequestTextureAccess(RenderStage* rs, AccessMode accessMode, const std::string& id, rhi::ResourceState inputState, rhi::ResourceState outputState)
{
	// Find render pass deps
	auto rs_it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rs](const RenderStageData* entry) -> bool
		{
			return entry->renderStage.get() == rs;
		});
	if (rs_it == m_RenderStages.end())
	{
		LOG_ERROR("Render pass with debug name {} not registered", rs->GetDebugName());
		return false;
	}

	// Remove prev dependencies
	auto it = std::find_if((*rs_it)->reads.begin(), (*rs_it)->reads.end(), [&id](const RenderStageTextureDep& dep)
		{ return dep.id == id; });
	if (it != (*rs_it)->reads.end())
	{
		(*rs_it)->reads.erase(it);
	}

	it = std::find_if((*rs_it)->writes.begin(), (*rs_it)->writes.end(), [&id](const RenderStageTextureDep& dep)
		{ return dep.id == id; });
	if (it != (*rs_it)->writes.end())
	{
		(*rs_it)->writes.erase(it);
	}

	if (accessMode == AccessMode::Read)
	{
		(*rs_it)->reads.emplace_back(id, inputState, outputState);
	}
	else
	{
		(*rs_it)->writes.emplace_back(id, inputState, outputState);
	}

	return true;
}

st::rhi::TextureHandle st::gfx::RenderView::GetTexture(const std::string& id) const
{
	auto texture_it = m_DeclaredTextures.find(id);
	if (texture_it != m_DeclaredTextures.end())
	{
		return texture_it->second->texture.get_weak();
	}
	return nullptr;
}

st::rhi::DescriptorIndex st::gfx::RenderView::GetShaderViewIndex(const std::string& id, rhi::TextureShaderView view)
{
	auto tex = GetTexture(id);
	if (tex)
	{
		return tex->GetShaderViewIndex(view);
	}
	return rhi::c_InvalidDescriptorIndex;
}

void st::gfx::RenderView::OnWindowSizeChanged()
{
	// 
	// Actually we are only interested in this if we are rendering to the swap chain BB
	if (!m_OffscreenFramebuffer)
	{
		const auto newSize = m_DeviceManager->GetWindowDimensions();
		// Update all the textures whose size is dependant on BB size
		for (auto& it : m_DeclaredTextures)
		{
			auto& declTex = it.second;
			if (declTex->requestedWidth <= 0 || declTex->requestedHeight <= 0)
			{
				rhi::TextureDesc newDesc = declTex->texture->GetDesc();
				newDesc.width = GetActualSize(declTex->requestedWidth, newSize.x);
				newDesc.height = GetActualSize(declTex->requestedHeight, newSize.y);
				
				// Lets queue the release just in case, but should not be neccessary because we are waiting for the GPU
				// At least in the case of swapchan BB.
				m_DeviceManager->GetDevice()->ReleaseQueued(declTex->texture);
				declTex->texture = m_DeviceManager->GetDevice()->CreateTexture(newDesc, 
					GetInitialState(declTex->type), declTex->id);
			}
		}

		// Inform render stages
		for (auto rs : m_RenderStages)
		{
			rs->renderStage->OnBackbufferResize();
		}
	}
	// TODO: Check if we are resized (or changed) the offscreen BB
}

void st::gfx::RenderView::Render()
{
	if (m_IsDirty)
	{
		Refresh();
	}

	UpdateSceneConstantBuffer();
	UpdateVisibleSet();

	st::rhi::FramebufferHandle frameBuffer = GetFramebuffer();
	if (frameBuffer)
	{
		auto commandList = GetCommandList();
		commandList->Open();
		commandList->BeginMarker(m_DebugName.c_str());

		// Back buffer is in COMMON state and need to be transitioned to RT
		commandList->PushBarrier(rhi::Barrier::Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture.get(), rhi::ResourceState::PRESENT, rhi::ResourceState::RENDERTARGET));

		// Set initial states info
		std::map<std::string, rhi::ResourceState> resourcesStates;
		for (auto& entry : m_DeclaredTextures)
		{
			resourcesStates.emplace(entry.first, GetInitialState(entry.second->type));
		}

		for (auto& rs : m_RenderStages)
		{
			UpdateTextureViews(commandList, rs->renderStage.get(), AccessMode::Read, resourcesStates);

			if (rs->renderStage->IsEnabled())
			{
				commandList->BeginMarker(rs->renderStage->GetDebugName());

				// Entry barriers
				{
					std::vector<rhi::Barrier> barriers;
					auto getBarriers = [&resourcesStates, &barriers, this](const std::vector<RenderStageTextureDep>& deps)
						{
							for (const auto& dep : deps)
							{
								auto state_it = resourcesStates.find(dep.id);
								if (state_it != resourcesStates.end() && state_it->second != dep.inputState)
								{
									barriers.push_back(rhi::Barrier::Texture(
										GetTexture(dep.id).get(), state_it->second, dep.inputState));
									state_it->second = dep.inputState;
								}
							}
						};
					getBarriers(rs->reads);
					getBarriers(rs->writes);

					if (!barriers.empty())
					{
						std::string markerName = rs->renderStage->GetDebugName();
						markerName.append(" - Entry barriers");
						commandList->BeginMarker(markerName.c_str());
						commandList->PushBarriers(barriers);
						commandList->EndMarker();
					}
				}

				rs->renderStage->Render();
				commandList->EndMarker();

				// Update the resource states with the state left by the stage
				{
					auto updateStates = [&resourcesStates, this](const std::vector<RenderStageTextureDep>& deps)
						{
							for (const auto& dep : deps)
							{
								auto state_it = resourcesStates.find(dep.id);
								if (state_it != resourcesStates.end() && state_it->second != dep.outputState)
								{
									state_it->second = dep.outputState;
								}
							}
						};
					updateStates(rs->reads);
					updateStates(rs->writes);
				}
			} // if (rs->renderStage->IsEnabled())

			UpdateTextureViews(commandList, rs->renderStage.get(), AccessMode::Write, resourcesStates);
		}

		// All the resources need to go back to it initial state
		for (auto& tex : m_DeclaredTextures)
		{
			rhi::ResourceState initialState = GetInitialState(tex.second->type);
			rhi::ResourceState currentState = resourcesStates.find(tex.first)->second;
			if (initialState != currentState)
			{
				commandList->PushBarrier(rhi::Barrier::Texture(tex.second->texture.get(), currentState, initialState));
			}
			
		}

		// Back buffer to common so it can be presented
		commandList->PushBarrier(rhi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture.get(), rhi::ResourceState::RENDERTARGET, rhi::ResourceState::PRESENT));

		commandList->EndMarker();
		commandList->Close();

		m_DeviceManager->GetDevice()->ExecuteCommandList(commandList.get(), st::rhi::QueueType::Graphics);
	}
	else
	{
		LOG_ERROR("No frame buffer specified. Nothing to render");
	}
}

st::gfx::RenderView::TextureViewTicket st::gfx::RenderView::RequestTextureView(RenderStage* rs, AccessMode accessMode, const std::string& id)
{
	for (auto& entry : m_TexViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode && entry->id == id)
		{
			++entry->refCount;
			return entry;
		}
	}

	auto* ticket = new TextureViewRequest{ rs, accessMode, id, 1 };
	m_TexViewRequests.push_back(ticket);
	return ticket;
}

void st::gfx::RenderView::ReleaseTextureView(TextureViewTicket ticket)
{
	auto it = std::find(m_TexViewRequests.begin(), m_TexViewRequests.end(), ticket);
	if (it != m_TexViewRequests.end())
	{
		if (--((*it)->refCount) == 0)
		{
			delete *it;
			m_TexViewRequests.erase(it);
		}
	}
}

st::rhi::TextureHandle st::gfx::RenderView::GetTextureView(TextureViewTicket ticket)
{
	auto it = std::find(m_TexViewRequests.begin(), m_TexViewRequests.end(), ticket);
	if (it == m_TexViewRequests.end())
		return nullptr;

	return (*it)->tex.get_weak();
}

void st::gfx::RenderView::Refresh()
{
	m_IsDirty = false;
}

void st::gfx::RenderView::ClearRenderStages()
{
	for (auto rs : m_RenderStages)
	{
		rs->renderStage->Detach();
		delete rs;
	}
	m_RenderStages.clear();

	for (auto& dt : m_DeclaredTextures)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(dt.second->texture);
	}
	m_DeclaredTextures.clear();
}

void st::gfx::RenderView::UpdateSceneConstantBuffer()
{
	if (!m_SceneCB)
	{
		rhi::BufferDesc desc{
			.memoryAccess = rhi::MemoryAccess::Upload,
			.shaderUsage = rhi::BufferShaderUsage::ConstantBuffer,
			.sizeBytes = sizeof(interop::Scene),
			.allowUAV = false,
			.stride = 0 };

		m_SceneCB = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE, "Scene CB");
	}

	interop::Scene* sceneShaderConstant = (interop::Scene*)m_SceneCB->Map();

	// Camera
	if (m_Camera)
	{
		sceneShaderConstant->camViewProjMatrix = m_Camera->GetViewProjectionMatrix();
		sceneShaderConstant->camWorldPos = float4{ m_Camera->GetPosition(), 0.f };
	}
	else
	{
		sceneShaderConstant->camViewProjMatrix = float4x4{ 1.f };
		sceneShaderConstant->camWorldPos = float4{ 0.f };
	}
	sceneShaderConstant->invCamViewProjMatrix = glm::inverse(sceneShaderConstant->camViewProjMatrix);

	// Scene data
	if (m_Scene)
	{
		// Sun directional light
		const Scene::SunParams& sunParams = m_Scene->GetSunParams();
		const float3 sunDir = st::ElevationAzimuthRadToDir(
			glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg));

		sceneShaderConstant->sunDirection = sunDir;
		sceneShaderConstant->sunIrradiance = sunParams.Irradiance;
		sceneShaderConstant->sunColor = float4{ sunParams.Color, 0.f };
		sceneShaderConstant->sunAngularSizeRad = glm::radians(sunParams.AngularSizeDeg);
		sceneShaderConstant->sunWorldToClipMatrix = GetSunWoldToClipMatrix(sunDir);

		// Ambient 
		const Scene::AmbientParams& ambientParams = m_Scene->GetAmbientParams();
		sceneShaderConstant->ambientTop = float4{ ambientParams.SkyColor * ambientParams.Intensity, 0.f };
		sceneShaderConstant->ambientBottom = float4{ ambientParams.GroundColor * ambientParams.Intensity, 0.f };

		// Data buffers
		sceneShaderConstant->instanceBufferDI = m_Scene->GetInstancesBufferDI();
		sceneShaderConstant->meshesBufferDI = m_Scene->GetMeshesBufferDI();
		sceneShaderConstant->materialsBufferDI = m_Scene->GetMaterialsBufferDI();
	}
	else
	{
		sceneShaderConstant->instanceBufferDI = rhi::c_InvalidDescriptorIndex;
		sceneShaderConstant->meshesBufferDI = rhi::c_InvalidDescriptorIndex;
		sceneShaderConstant->materialsBufferDI = rhi::c_InvalidDescriptorIndex;
	}

	m_SceneCB->Unmap();
}

void st::gfx::RenderView::UpdateVisibleSet()
{
	m_VisibleSet.clear();
	if (!m_Camera || !m_Scene || !m_Scene->GetSceneGraph())
		return;

	const auto& frustum = m_Camera->GetFrustum();
	st::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (has_flag(node->GetContentFlags(), SceneContentFlags::OpaqueMeshes) && node->HasBounds() && frustum.check(node->GetWorldBounds()))
		{
			auto leaf = node->GetLeaf();
			if (leaf)
			{
				const auto* meshInstance = st::checked_cast<const st::gfx::MeshInstance*>(leaf.get());
				assert(meshInstance);
				m_VisibleSet.push_back(meshInstance);
			}
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	}
}

std::vector<st::gfx::RenderView::TextureViewRequest*> st::gfx::RenderView::GetTexViewRequests(RenderStage* rs, AccessMode accessMode)
{
	std::vector<st::gfx::RenderView::TextureViewRequest*> ret;
	for (auto& entry : m_TexViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode)
		{
			ret.push_back(entry);
		}
	}
	return ret;
}

void st::gfx::RenderView::UpdateTextureViews(st::rhi::CommandListHandle commandList, RenderStage* rs, AccessMode accessMode, const std::map<std::string, rhi::ResourceState> resourcesStates)
{
	auto requests = GetTexViewRequests(rs, accessMode);
	for (auto req : requests)
	{
		auto it = m_DeclaredTextures.find(req->id);
		if (it == m_DeclaredTextures.end())
			continue;

		rhi::TextureHandle sourceTex = it->second->texture.get_weak();
		if (!sourceTex)
			continue;

		rhi::TextureDesc sourceTexDesc = sourceTex->GetDesc();

		// Create the target texture if does not exists
		// TODO: If the source texture changed (resized for example) we need to re-create de texture
		if (!req->tex)
		{
			rhi::TextureDesc desc{
				.width = sourceTexDesc.width,
				.height = sourceTexDesc.height,
				.depth = sourceTexDesc.depth,
				.arraySize = sourceTexDesc.arraySize,
				.mipLevels = sourceTexDesc.mipLevels,
				.format = sourceTexDesc.format,//rhi::Format::RGBA8_UNORM,
				.shaderUsage = rhi::TextureShaderUsage::ShaderResource
			};

			std::stringstream debugName;
			debugName << rs->GetDebugName() << " - ";
			if (accessMode == st::gfx::RenderView::AccessMode::Read)
				debugName << "Read";
			else
				debugName << "Write";
			debugName << " - " << sourceTex->GetDebugName();

			req->tex = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::SHADER_RESOURCE, debugName.str().c_str());
			assert(req->tex);
		}

		rhi::ResourceState srcTexState = resourcesStates.find(req->id)->second;

		rhi::Barrier entryBarriers[] = {
			rhi::Barrier::Texture(sourceTex.get(), srcTexState, rhi::ResourceState::COPY_SRC),
			rhi::Barrier::Texture(req->tex.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST) };
		commandList->PushBarriers(entryBarriers);

		commandList->CopyTextureToTexture(req->tex.get(), rhi::AllSubresources, sourceTex.get(), rhi::AllSubresources);
		
		rhi::Barrier exitBarriers[] = {
			rhi::Barrier::Texture(sourceTex.get(), rhi::ResourceState::COPY_SRC, srcTexState),
			rhi::Barrier::Texture(req->tex.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE) };
		commandList->PushBarriers(exitBarriers);
	}
}

float4x4 st::gfx::RenderView::GetSunWoldToClipMatrix(const float3& sunDir)
{
	const Scene::SunParams& sunParams = m_Scene->GetSunParams();

	const st::math::aabox3f sceneBounds = m_Scene->GetSceneGraph()->GetRoot()->GetWorldBounds();
	const float3 sceneCenter = sceneBounds.center();
	const float3 sceneExtents = sceneBounds.extents();
	const float3 sunPos = sceneCenter - (sunDir * glm::length(sceneExtents) / 2.f);

	const float3 sunUp = fabs(glm::dot(sunDir, { 0, 1, 0 })) > 0.99f ? float3(0, 0, 1) : float3(0, 1, 0);
	const float4x4 sunViewMatrix = glm::lookAtRH(sunPos, sunPos + sunDir, sunUp);

	float3 corners[8] = {
		{sceneBounds.min.x, sceneBounds.min.y, sceneBounds.min.z},
		{sceneBounds.max.x, sceneBounds.min.y, sceneBounds.min.z},
		{sceneBounds.min.x, sceneBounds.max.y, sceneBounds.min.z},
		{sceneBounds.max.x, sceneBounds.max.y, sceneBounds.min.z},
		{sceneBounds.min.x, sceneBounds.min.y, sceneBounds.max.z},
		{sceneBounds.max.x, sceneBounds.min.y, sceneBounds.max.z},
		{sceneBounds.min.x, sceneBounds.max.y, sceneBounds.max.z},
		{sceneBounds.max.x, sceneBounds.max.y, sceneBounds.max.z},
	};
	float3 min_v{ FLT_MAX };
	float3 max_v{ -FLT_MAX };
	for (int i = 0; i < 8; ++i)
	{
		float4 v = sunViewMatrix * float4(corners[i], 1.0f);
		min_v.x = std::min(min_v.x, v.x);
		min_v.y = std::min(min_v.y, v.y);
		min_v.z = std::min(min_v.z, v.z);
		max_v.x = std::max(max_v.x, v.x);
		max_v.y = std::max(max_v.y, v.y);
		max_v.z = std::max(max_v.z, v.z);
	}
	assert(max_v.z > min_v.z);
	float zNear = -max_v.z;
	float zFar = -min_v.z;
	assert(zNear >= 0.0f);
	assert(zFar >= zNear);

	const float4x4 sunProjMatrix = BuildOrthoInvZ(min_v.x, max_v.x, min_v.y, max_v.y, zNear, zFar);

#ifdef _DEBUG
	//*** TEST
	{
		float4 pNear = sunProjMatrix * float4{ 0.f, 0.f, -zNear, 1.f };
		float4 pFar = sunProjMatrix * float4{ 0.f, 0.f, -zFar, 1.f };
		float zn = pNear.z / pNear.w;
		float zf = pFar.z / pFar.w;
		assert(zn > zf);
		// Ortho proj, W is 1, so p.w should be 1
		assert(AlmostEqual(zn, 1.f) && AlmostEqual(pFar.z, 0.f));
		assert(AlmostEqual(zf, 0.f) && AlmostEqual(pFar.w, 1.f));
	}
#endif

	return sunProjMatrix * sunViewMatrix;
}