#pragma once
#include <memory>
#include <nvrhi/nvrhi.h>

namespace st::gfx
{
	class Camera;
	class RenderPass;
	class DeviceManager;
}

namespace st::gfx
{

class RenderView
{
public:

	RenderView(DeviceManager* deviceManager);

	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(nvrhi::FramebufferHandle frameBuffer);

	void SetRenderPasses(std::vector<RenderPass*>&& renderPasses);

	void Render();

private:

	std::shared_ptr<Camera> m_Camera;
	nvrhi::FramebufferHandle m_OffscreenFramebuffer;	

	std::vector<RenderPass*> m_RenderPasses;

	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx