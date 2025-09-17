#include "Gfx/Frame.h"
#include "Gfx/RenderPass.h"

void st::gfx::Frame::Init(DeviceManager* deviceManager, std::vector<RenderPass*>&& renderPasses)
{
	m_DeviceManager = deviceManager;
	m_RenderPasses = std::move(renderPasses);
}

void st::gfx::Frame::Render(nvrhi::IFramebuffer* frameBuffer)
{
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->Render(frameBuffer);
	}
}

void st::gfx::Frame::OnBackbufferResize(const glm::ivec2& size)
{
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->OnBackbufferResize(size);
	}
}