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
	for (auto rp : m_RenderStages)
	{
		rp.renderStage->OnSceneChanged();
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
		m_RenderStages.push_back(RenderStageData{ {}, {}, rs });

	for (auto rs : m_RenderStages)
	{
		rs.renderStage->Attach(this);
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

st::rhi::DescriptorIndex st::gfx::RenderView::GetSceneBufferDI()
{
	return m_SceneCB ? m_SceneCB->GetShaderViewIndex(rhi::BufferShaderView::ConstantBuffer) : rhi::c_InvalidDescriptorIndex;
}

bool st::gfx::RenderView::CreateColorTarget(const char* id, int width, int height, int arraySize, rhi::Format format)
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
		.shaderUsage = rhi::TextureShaderUsage::ShaderResource | rhi::TextureShaderUsage::RenderTarget };

	rhi::TextureOwner texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::RENDERTARGET, id);
	m_DeclaredTextures.insert({ id, std::make_unique<DeclaredTexture>(std::move(texture), id, false, width, height) });

	return true;
}

bool st::gfx::RenderView::CreateDepthTarget(const char* id, int width, int height, int arraySize, rhi::Format format)
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
		.shaderUsage = rhi::TextureShaderUsage::ShaderResource | rhi::TextureShaderUsage::DepthStencil };

	// Created in common state
	rhi::TextureOwner texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::DEPTHSTENCIL, id);
	m_DeclaredTextures.insert({ id, std::make_unique<DeclaredTexture>(std::move(texture), id, true, width, height) });

	return true;
}

bool st::gfx::RenderView::RequestTextureAccess(RenderStage* rp, AccessMode accessMode, const char* id, rhi::ResourceState inputState, rhi::ResourceState outputState)
{
	// Check that texture is create
	auto texture_it = m_DeclaredTextures.find(id);
	if (texture_it == m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} not created", id);
		return false;
	}

	// Find render pass deps
	auto rp_it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rp](const RenderStageData& entry) -> bool
		{
			return entry.renderStage.get() == rp;
		});
	if (rp_it == m_RenderStages.end())
	{
		LOG_ERROR("Render pass with debug name {} not registered", rp->GetDebugName());
		return false;
	}

	if (accessMode == AccessMode::Read)
	{
		rp_it->reads.emplace_back(texture_it->second.get(), inputState, outputState);
	}
	else
	{
		rp_it->writes.emplace_back(texture_it->second.get(), inputState, outputState);
	}

	return true;
}

st::rhi::TextureHandle st::gfx::RenderView::GetTexture(const char* id) const
{
	auto texture_it = m_DeclaredTextures.find(id);
	if (texture_it == m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} not created", id);
		return nullptr;
	}
	return texture_it->second->texture.get_weak();
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
			if (declTex->originalWidth <= 0 || declTex->originalHeight <= 0)
			{
				rhi::TextureDesc newDesc = declTex->texture->GetDesc();
				newDesc.width = GetActualSize(declTex->originalWidth, newSize.x);
				newDesc.height = GetActualSize(declTex->originalWidth, newSize.y);
				
				// Lets queue the release just in case, but should not be neccessary because we are waiting for the GPU
				// At least in the case of swapchan BB.
				m_DeviceManager->GetDevice()->ReleaseQueued(declTex->texture);
				declTex->texture = m_DeviceManager->GetDevice()->CreateTexture(newDesc, 
					declTex->isDepthStencil ? rhi::ResourceState::DEPTHSTENCIL : rhi::ResourceState::RENDERTARGET, declTex->id);
			}
		}

		// Inform render stages
		for (auto rs : m_RenderStages)
		{
			rs.renderStage->OnBackbufferResize();
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

	UpdateSceneBuffer();
	UpdateVisibleSet();

	st::rhi::FramebufferHandle frameBuffer = GetFramebuffer();
	if (frameBuffer)
	{
		auto commandList = GetCommandList();
		commandList->Open();
		commandList->BeginMarker(m_DebugName.c_str());

		// Back buffer is in COMMON state and need to be transitioned to RT
		commandList->PushBarrier(rhi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture, rhi::ResourceState::PRESENT, rhi::ResourceState::RENDERTARGET));

		// Set initial states info
		std::map<std::string, rhi::ResourceState> resourcesStates;
		for (auto& entry : m_DeclaredTextures)
		{
			resourcesStates.emplace(entry.first, entry.second->isDepthStencil ? 
				rhi::ResourceState::DEPTHSTENCIL : rhi::ResourceState::RENDERTARGET);
		}

		for (auto& renderStage : m_RenderStages)
		{
			UpdateTextureViews(commandList, renderStage.renderStage.get(), AccessMode::Read, resourcesStates);

			commandList->BeginMarker(renderStage.renderStage->GetDebugName());

			// Entry barriers
			{
				std::vector<rhi::Barrier> barriers;
				auto getBarriers = [&resourcesStates, &barriers](const std::vector<RenderStageTextureDep>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = resourcesStates.find(dep.declTex->id);
						assert(state_it != resourcesStates.end());
						if (state_it->second != dep.inputState)
						{
							barriers.push_back(rhi::Barrier::Texture(
								dep.declTex->texture.get(), state_it->second, dep.inputState));
							state_it->second = dep.inputState;
						}
					}
				};
				getBarriers(renderStage.reads);
				getBarriers(renderStage.writes);

				if (!barriers.empty())
				{
					std::string markerName = renderStage.renderStage->GetDebugName();
					markerName.append(" - Entry barriers");
					commandList->BeginMarker(markerName.c_str());
					commandList->PushBarriers(barriers);
					commandList->EndMarker();
				}
			}

			renderStage.renderStage->Render();
			commandList->EndMarker();

			// Update the resource states with the state left by the stage
			{
				auto updateStates = [&resourcesStates](const std::vector<RenderStageTextureDep>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = resourcesStates.find(dep.declTex->id);
						assert(state_it != resourcesStates.end());
						if (state_it->second != dep.outputState)
						{
							state_it->second = dep.outputState;
						}
					}
				};
				updateStates(renderStage.reads);
				updateStates(renderStage.writes);
			}

			UpdateTextureViews(commandList, renderStage.renderStage.get(), AccessMode::Write, resourcesStates);
		}

		// All the resources need to go back to it initial state
		for (auto& tex : m_DeclaredTextures)
		{
			rhi::ResourceState initialState = tex.second->isDepthStencil ? rhi::ResourceState::DEPTHSTENCIL : rhi::ResourceState::RENDERTARGET;
			rhi::ResourceState currentState = resourcesStates.find(tex.first)->second;
			if (initialState != currentState)
			{
				commandList->PushBarrier(rhi::Barrier::Texture(tex.second->texture.get(), currentState, initialState));
			}
			
		}

		// Back buffer to common so it can be presented
		commandList->PushBarrier(rhi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture, rhi::ResourceState::RENDERTARGET, rhi::ResourceState::PRESENT));

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
	for (auto rp : m_RenderStages)
	{
		rp.renderStage->Detach();
	}
	m_RenderStages.clear();

	for (auto& dt : m_DeclaredTextures)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(dt.second->texture);
	}
	m_DeclaredTextures.clear();
}

void st::gfx::RenderView::UpdateSceneBuffer()
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

	interop::Scene* sceneData = (interop::Scene*)m_SceneCB->Map();

	sceneData->camViewProjMatrix = m_Camera ? m_Camera->GetViewProjectionMatrix() : float4x4{ 1.f };
	sceneData->invCamViewProjMatrix = glm::inverse(sceneData->camViewProjMatrix);

	sceneData->sunDirection = glm::normalize(float3(1.f, -1.f, 0.f)); // default;
	sceneData->sunIntensity = 1.f;
	sceneData->sunColor = float3(1.f);
	if (m_Camera && m_Scene)
	{
		st::math::aabox3f sceneBounds = m_Scene->GetSceneGraph()->GetRoot()->GetWorldBounds();
		float3 sceneExtents = sceneBounds.max - sceneBounds.min;
		float3 sceneCenter = sceneBounds.min + (sceneExtents / 2.f);
		float sceneExtentMag = glm::length(sceneExtents);
		float3 sunPosition = m_Camera->GetPosition();
		sceneData->sunDirection = m_Camera->GetForward();
		sceneData->sunDirection = glm::normalize(glm::normalize(sceneCenter - sunPosition) + float3{ 0.f, -1.f, 0.f });
		
		//sceneData->sunDirection = glm::normalize(float3{ 1.f, -1.f, -1.f });
		//sunPosition = float3{ -5.f, 5.f, 5.f };

		sceneData->sunDirection.x = -sceneData->sunDirection.x;
		sceneData->sunDirection.z = -sceneData->sunDirection.z;
		sunPosition = -sceneData->sunDirection * 100.f;

		float3 sunUp = fabs(glm::dot(sceneData->sunDirection, { 0, 1, 0 })) > 0.99f ? float3(0, 0, 1) : float3(0, 1, 0);
		float4x4 sunViewMatrix = glm::lookAtRH(sunPosition, sunPosition + sceneData->sunDirection, sunUp);

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
		float zNear = std::max(-max_v.z, 0.f);
		float zFar = std::max(-min_v.z, 0.f);
		assert(zNear >= 0.0f);
		assert(zFar >= zNear);

		float4x4 sunProjMatrix = BuildOrthoInvZ(min_v.x, max_v.x, min_v.y, max_v.y, zNear, zFar);
		//float4x4 sunProjMatrix = BuildPersInvZInfFar(PI / 2.f, 1.f, 0.1f);
		//float4x4 sunProjMatrix = BuildPersInvZ(PI / 2.f, 1.f, zNear, zFar);
		sceneData->sunWorldToClipMatrix = sunProjMatrix * sunViewMatrix;

		//*** TEST
#ifdef _DEBUG
		{
			//float4 pNear = sunProjMatrix * float4(min_v, 1.f);
			//float4 pFar = sunProjMatrix * float4(max_v, 1.f);
			float4 pNear = sunProjMatrix * float4{ 0.f, 0.f, -zNear, 1.f };
			float4 pFar = sunProjMatrix * float4{ 0.f, 0.f, -zFar, 1.f };
			float zn = pNear.z / pNear.w;
			float zf = pFar.z / pFar.w;
			puts("gola");
			//assert(zn > zf);
			// Ortho proj, W is 1, so p.w should be 1
			//assert(AlmostEqual(zn, 1.f) && AlmostEqual(pFar.z, 0.f));
			//assert(AlmostEqual(zf, 1.f) && AlmostEqual(pFar.w, 1.f));
		}
#endif
	}
	else
	{
		sceneData->sunWorldToClipMatrix = float4x4{ 1.f };
	}

	if (m_Scene)
	{
		sceneData->instanceBufferDI = m_Scene->GetInstancesBufferDI();
		sceneData->meshesBufferDI = m_Scene->GetMeshesBufferDI();
		sceneData->materialsBufferDI = m_Scene->GetMaterialsBufferDI();
	}
	else
	{
		sceneData->instanceBufferDI = rhi::c_InvalidDescriptorIndex;
		sceneData->meshesBufferDI = rhi::c_InvalidDescriptorIndex;
		sceneData->materialsBufferDI = rhi::c_InvalidDescriptorIndex;
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
		if (has_flag(node->GetContentFlags(), SceneContentFlags::OpaqueMeshes) && node->HasBounds() /*&& frustum.check(node->GetWorldBounds())*/)
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