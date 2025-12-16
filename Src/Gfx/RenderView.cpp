#include "Gfx/RenderView.h"
#include "Gfx/RenderPass.h"
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

void st::gfx::RenderView::SetRenderPasses(const std::vector<std::shared_ptr<RenderPass>>& renderPasses)
{
	CleanRenderPasses();

	m_RenderPasses.reserve(renderPasses.size());
	for (auto& rp : renderPasses)
		m_RenderPasses.push_back(RenderPassDeps{ {}, rp });

	for (auto rp : m_RenderPasses)
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

bool st::gfx::RenderView::CreateTexture(const rapi::TextureDesc& desc, const char* id)
{
	// Check that texture has not been declared
	auto it = m_DeclaredTextures.find(id);
	if (it != m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} already created", id);
		return false;
	}

	// Created in common state
	rapi::TextureHandle texture = m_DeviceManager->GetDevice()->CreateTexture(desc, rapi::ResourceState::COMMON);
	m_DeclaredTextures.insert({ id, { texture, id } });

	return true;
}

bool st::gfx::RenderView::RequestTextureAccess(RenderPass* rp, AccessMode accessMode, const char* id, rapi::ResourceState inputState, rapi::ResourceState outputState)
{
	// Check that texture is create
	auto texture_it = m_DeclaredTextures.find(id);
	if (texture_it == m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} not created", id);
		return false;
	}

	// Find render pass deps
	auto rp_it = std::find_if(m_RenderPasses.begin(), m_RenderPasses.end(), [rp](const RenderPassDeps& entry) -> bool
		{
			return entry.renderPass.get() == rp;
		});
	if (rp_it == m_RenderPasses.end())
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

		// Render targets always are in COMMON state and need to be transitioned to RT
		commandList->PushBarrier(rapi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture, rapi::ResourceState::COMMON, rapi::ResourceState::RENDERTARGET));

		std::map<std::string, rapi::ResourceState> resourcesStates;
		for (auto& entry : m_DeclaredTextures)
		{
			resourcesStates.emplace(entry.first, rapi::ResourceState::COMMON);
		}

		for (auto& renderPass : m_RenderPasses)
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

		// All the resources need to go back to common
		for (auto& entry : resourcesStates)
		{
			if (entry.second != rapi::ResourceState::COMMON)
			{
				auto it = m_DeclaredTextures.find(entry.first);
				commandList->PushBarrier(rapi::Barrier::Texture(it->second.texture.get(), entry.second, rapi::ResourceState::COMMON));
			}
		}

		// Render attachment to commoin to it can be presented
		commandList->PushBarrier(rapi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture, rapi::ResourceState::RENDERTARGET, rapi::ResourceState::COMMON));
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
	for (auto rp : m_RenderPasses)
	{
		rp.renderPass->Detach();
	}
	m_RenderPasses.clear();

	for (auto dt : m_DeclaredTextures)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(dt.second.texture);
	}
	m_DeclaredTextures.clear();
}