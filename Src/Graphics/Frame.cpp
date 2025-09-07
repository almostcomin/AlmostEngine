#include "Graphics/Frame.h"
#include "Graphics/RenderPass.h"

void st::gfx::Frame::Init(DeviceManager* deviceManager, std::vector<RenderPass*>&& renderPasses)
{
	m_DeviceManager = deviceManager;
	m_RenderPasses = std::move(renderPasses);
}