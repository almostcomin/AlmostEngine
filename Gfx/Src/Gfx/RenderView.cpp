#include "Gfx/RenderView.h"
#include "Gfx/RenderStage.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Core/Log.h"
#include "Interop/RenderResources.h"

st::gfx::RenderView::RenderView(DeviceManager* deviceManager, const char* debugName) :
	m_IsDirty{ false }, m_DebugName{ debugName }, m_DeviceManager { deviceManager }
{
	rhi::CommandListParams params{
		.queueType = rhi::QueueType::Graphics
	};
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
	{
		m_CommandLists.push_back(m_DeviceManager->GetDevice()->CreateCommandList(params));
	}
}

st::gfx::RenderView::~RenderView()
{
	CleanRenderPasses();
	m_DeviceManager->GetDevice()->ReleaseQueued(m_SceneCB);

	for (int i = 0; i < m_CommandLists.size(); ++i)
	{
		m_DeviceManager->GetDevice()->ReleaseImmediately(m_CommandLists[i]);
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
	CleanRenderPasses();

	m_RenderStages.reserve(renderStages.size());
	for (auto& rp : renderStages)
		m_RenderStages.push_back(RenderPassDeps{ {}, rp });

	for (auto rp : m_RenderStages)
	{
		rp.renderPass->Attach(this);
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
	return m_CommandLists[m_DeviceManager->GetFrameModuleIndex()];
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

	if (width == c_BBSize)
		width = GetFramebuffer()->GetFramebufferInfo().width;
	if (height == c_BBSize)
		height = GetFramebuffer()->GetFramebufferInfo().height;

	rhi::TextureDesc desc{
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.format = format,
		.shaderUsage = rhi::TextureShaderUsage::ShaderResource | rhi::TextureShaderUsage::RenderTarget,
		.debugName = id };

	rhi::TextureHandle texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::RENDERTARGET);
	m_DeclaredTextures.insert({ id, { texture, id, false } });

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

	if (width == c_BBSize)
		width = GetFramebuffer()->GetFramebufferInfo().width;
	if (height == c_BBSize)
		height = GetFramebuffer()->GetFramebufferInfo().height;

	rhi::TextureDesc desc{
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.format = format,
		.shaderUsage = rhi::TextureShaderUsage::ShaderResource | rhi::TextureShaderUsage::DepthStencil,
		.debugName = id };

	// Created in common state
	rhi::TextureHandle texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::DEPTHSTENCIL);
	m_DeclaredTextures.insert({ id, { texture, id, true } });

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
	auto rp_it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rp](const RenderPassDeps& entry) -> bool
		{
			return entry.renderPass.get() == rp;
		});
	if (rp_it == m_RenderStages.end())
	{
		LOG_ERROR("Render pass with debug name {} not registered", rp->GetDebugName());
		return false;
	}

	rp_it->textureDeps.emplace_back(texture_it->second, accessMode, inputState, outputState);

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
	return texture_it->second.texture;
}

void st::gfx::RenderView::Render()
{
	if (m_IsDirty)
	{
		Refresh();
	}

	UpdateSceneBuffer();

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
			resourcesStates.emplace(entry.first, entry.second.isDepthStencil ? 
				rhi::ResourceState::DEPTHSTENCIL : rhi::ResourceState::RENDERTARGET);
		}

		for (auto& renderPass : m_RenderStages)
		{
			std::string markerName = renderPass.renderPass->GetDebugName();
			markerName.append(" - Entry barriers");
			commandList->BeginMarker(markerName.c_str());

			for (const auto& dep : renderPass.textureDeps)
			{
				auto state_it = resourcesStates.find(dep.declTex.id);
				assert(state_it != resourcesStates.end());
				if (state_it->second != dep.inputState)
				{
					commandList->PushBarrier(rhi::Barrier::Texture(
						dep.declTex.texture.get(), state_it->second, dep.inputState));
					state_it->second = dep.inputState;
				}
			}

			commandList->EndMarker();

			commandList->BeginMarker(renderPass.renderPass->GetDebugName());
			renderPass.renderPass->Render();
			commandList->EndMarker();

			// Update the resource states with the state left by the stages
			for (const auto& dep : renderPass.textureDeps)
			{
				auto state_it = resourcesStates.find(dep.declTex.id);
				assert(state_it != resourcesStates.end());
				if (state_it->second != dep.outputState)
				{
					state_it->second = dep.outputState;
				}
			}
		}

		// All the resources need to go back to it initial state
		for (auto& tex : m_DeclaredTextures)
		{
			rhi::ResourceState initialState = tex.second.isDepthStencil ? rhi::ResourceState::DEPTHSTENCIL : rhi::ResourceState::RENDERTARGET;
			rhi::ResourceState currentState = resourcesStates.find(tex.first)->second;
			if (initialState != currentState)
			{
				commandList->PushBarrier(rhi::Barrier::Texture(tex.second.texture.get(), currentState, initialState));
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

void st::gfx::RenderView::CleanRenderPasses()
{
	for (auto rp : m_RenderStages)
	{
		rp.renderPass->Detach();
	}
	m_RenderStages.clear();

	for (auto dt : m_DeclaredTextures)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(dt.second.texture);
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

		m_SceneCB = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE);
	}

	interop::Scene* sceneData = (interop::Scene*)m_SceneCB->Map();
	sceneData->viewProjectionMatrix = m_Camera ? m_Camera->GetViewProjectionMatrix() : float4x4{ 1.f };
	m_SceneCB->Unmap();
}