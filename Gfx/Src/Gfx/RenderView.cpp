#include "Gfx/RenderView.h"
#include "Gfx/RenderStage.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Core/Log.h"
#include "Interop/RenderResources.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/MeshInstance.h"

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
	for (auto& rp : renderStages)
		m_RenderStages.push_back(RenderStageDeps{ {}, rp });

	for (auto rp : m_RenderStages)
	{
		rp.renderStage->Attach(this);
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

bool st::gfx::RenderView::CreateColorTarget(const char* id, int width, int height, rhi::Format format)
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
		.format = format,
		.shaderUsage = rhi::TextureShaderUsage::ShaderResource | rhi::TextureShaderUsage::RenderTarget };

	rhi::TextureOwner texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::RENDERTARGET, id);
	m_DeclaredTextures.insert({ id, std::make_unique<DeclaredTexture>(std::move(texture), id, false, width, height) });

	return true;
}

bool st::gfx::RenderView::CreateDepthTarget(const char* id, int width, int height, rhi::Format format)
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
	auto rp_it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rp](const RenderStageDeps& entry) -> bool
		{
			return entry.renderStage.get() == rp;
		});
	if (rp_it == m_RenderStages.end())
	{
		LOG_ERROR("Render pass with debug name {} not registered", rp->GetDebugName());
		return false;
	}

	rp_it->textureDeps.emplace_back(texture_it->second.get(), accessMode, inputState, outputState);

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
			commandList->BeginMarker(renderStage.renderStage->GetDebugName());

			// Entry barriers
			{
				std::vector<rhi::Barrier> barriers;
				for (const auto& dep : renderStage.textureDeps)
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
				for (const auto& dep : renderStage.textureDeps)
				{
					auto state_it = resourcesStates.find(dep.declTex->id);
					assert(state_it != resourcesStates.end());
					if (state_it->second != dep.outputState)
					{
						state_it->second = dep.outputState;
					}
				}
			}
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
	sceneData->viewProjectionMatrix = m_Camera ? m_Camera->GetViewProjectionMatrix() : float4x4{ 1.f };
	sceneData->sunDirection = glm::normalize(float3(1.f, 1.f, 1.f));
	sceneData->sunIntensity = 1.f;
	sceneData->sunColor = float3(1.f);
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