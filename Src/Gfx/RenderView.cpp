#include "Gfx/RenderView.h"
#include "Gfx/RenderStage.h"
#include "Gfx/DeviceManager.h"
#include "RenderAPI/Device.h"
#include "Core/Log.h"

st::gfx::RenderView::RenderView(DeviceManager* deviceManager) :
	m_IsDirty{ false }, m_DeviceManager{ deviceManager }
{
	rapi::CommandListParams params{
		.queueType = rapi::QueueType::Graphics
	};
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
	{
		m_CommandLists.push_back(m_DeviceManager->GetDevice()->CreateCommandList(params));
	}
}

st::gfx::RenderView::~RenderView()
{
	CleanRenderPasses();

	for (int i = 0; i < m_CommandLists.size(); ++i)
	{
		m_DeviceManager->GetDevice()->ReleaseImmediately(m_CommandLists[i]);
	}
}

void st::gfx::RenderView::SetCamera(std::shared_ptr<st::gfx::Camera> camera)
{
	m_Camera = camera;
}

void st::gfx::RenderView::SetOffscreenFrameBuffer(st::rapi::FramebufferHandle frameBuffer)
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

st::rapi::FramebufferHandle st::gfx::RenderView::GetFramebuffer()
{
	return m_OffscreenFramebuffer ? m_OffscreenFramebuffer : m_DeviceManager->GetCurrentFramebuffer();
}

st::rapi::CommandListHandle st::gfx::RenderView::GetCommandList()
{
	return m_CommandLists[m_DeviceManager->GetFrameModuleIndex()];
}

bool st::gfx::RenderView::CreateColorTarget(const char* id, int width, int height, rapi::Format format)
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

	rapi::TextureDesc desc{
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.format = format,
		.shaderUsage = rapi::TextureShaderUsage::ShaderResource | rapi::TextureShaderUsage::RenderTarget,
		.debugName = id };

	rapi::TextureHandle texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rapi::ResourceState::RENDERTARGET);
	m_DeclaredTextures.insert({ id, { texture, id, false } });
}

bool st::gfx::RenderView::CreateDepthTarget(const char* id, int width, int height, rapi::Format format)
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

	rapi::TextureDesc desc{
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.format = format,
		.shaderUsage = rapi::TextureShaderUsage::ShaderResource | rapi::TextureShaderUsage::DepthStencil,
		.debugName = id };

	// Created in common state
	rapi::TextureHandle texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rapi::ResourceState::DEPTHSTENCIL);
	m_DeclaredTextures.insert({ id, { texture, id, true } });
}

bool st::gfx::RenderView::RequestTextureAccess(RenderStage* rp, AccessMode accessMode, const char* id, rapi::ResourceState inputState, rapi::ResourceState outputState)
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

st::rapi::TextureHandle st::gfx::RenderView::GetTexture(const char* id) const
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

	st::rapi::FramebufferHandle frameBuffer = GetFramebuffer();
	if (frameBuffer)
	{
		auto commandList = GetCommandList();
		commandList->Open();

		// Back buffer is in COMMON state and need to be transitioned to RT
		commandList->PushBarrier(rapi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture, rapi::ResourceState::PRESENT, rapi::ResourceState::RENDERTARGET));

		// Set initial states info
		std::map<std::string, rapi::ResourceState> resourcesStates;
		for (auto& entry : m_DeclaredTextures)
		{
			resourcesStates.emplace(entry.first, entry.second.isDepthStencil ? 
				rapi::ResourceState::DEPTHSTENCIL : rapi::ResourceState::RENDERTARGET);
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
					commandList->PushBarrier(rapi::Barrier::Texture(
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
			rapi::ResourceState initialState = tex.second.isDepthStencil ? rapi::ResourceState::DEPTHSTENCIL : rapi::ResourceState::RENDERTARGET;
			rapi::ResourceState currentState = resourcesStates.find(tex.first)->second;
			if (initialState != currentState)
			{
				commandList->PushBarrier(rapi::Barrier::Texture(tex.second.texture.get(), currentState, initialState));
			}
			
		}

		// Back buffer to common so it can be presented
		commandList->PushBarrier(rapi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture, rapi::ResourceState::RENDERTARGET, rapi::ResourceState::PRESENT));
		commandList->Close();

		m_DeviceManager->GetDevice()->ExecuteCommandList(commandList.get(), st::rapi::QueueType::Graphics);
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