#include "Gfx/RenderView.h"
#include "Gfx/RenderPass.h"
#include "Gfx/DeviceManager.h"
#include "RenderAPI/Device.h"
#include "Core/Log.h"

st::gfx::RenderView::RenderView(DeviceManager* deviceManager) :
	m_DeviceManager{ deviceManager }
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
	for (auto rp : m_RenderPasses)
	{
		rp->Detach();
	}

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

void st::gfx::RenderView::SetRenderPasses(std::vector<std::shared_ptr<RenderPass>>&& renderPasses)
{
	for (auto rp : m_RenderPasses)
	{
		rp->Detach();
	}

	m_RenderPasses = std::move(renderPasses);

	for (auto rp : m_RenderPasses)
	{
		rp->Attach(this);
	}
}

st::rapi::FramebufferHandle st::gfx::RenderView::GetFramebuffer()
{
	return m_OffscreenFramebuffer ? m_OffscreenFramebuffer : m_DeviceManager->GetCurrentFramebuffer();
}

st::rapi::CommandListHandle st::gfx::RenderView::GetCommandList()
{
	return m_CommandLists[m_DeviceManager->GetFrameModuleIndex()];
}

void st::gfx::RenderView::Render()
{
	st::rapi::FramebufferHandle frameBuffer = GetFramebuffer();
	if (frameBuffer)
	{
		auto commandList = GetCommandList();
		commandList->Open();
		commandList->PushBarrier(rapi::Barrier().Texture(
			frameBuffer->GetDesc().ColorAttachments[0].texture, rapi::ResourceState::COMMON, rapi::ResourceState::RENDERTARGET));

		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->Render();
		}

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