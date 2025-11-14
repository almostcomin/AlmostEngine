#include "Gfx/RenderView.h"
#include "Gfx/RenderPass.h"
#include "Gfx/DeviceManager.h"
#include "Core/Log.h"

st::gfx::RenderView::RenderView(DeviceManager* deviceManager) :
	m_DeviceManager{ deviceManager }
{}

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

void st::gfx::RenderView::Render()
{
	st::rapi::IFramebuffer* frameBuffer = m_OffscreenFramebuffer ?
		m_OffscreenFramebuffer.get() : m_DeviceManager->GetCurrentFramebuffer();

	if (frameBuffer)
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->Render(frameBuffer);
		}
	}
	else
	{
		LOG_ERROR("No frame buffer specified. Nothing to render");
	}
}