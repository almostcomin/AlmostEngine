#pragma once
#include <memory>
#include <nvrhi/nvrhi.h>
#include "Core/Util.h"
#include "Core/Memory.h"

namespace st::gfx
{
	class Camera;
	class RenderPass;
	class DeviceManager;
}

namespace st::gfx
{

class RenderView : public st::enable_weak_from_this<RenderView>, private st::noncopyable_nonmovable
{
public:

	RenderView(DeviceManager* deviceManager);

	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(nvrhi::FramebufferHandle frameBuffer);

	void SetRenderPasses(std::vector<std::shared_ptr<RenderPass>>&& renderPasses);

	void Render();

private:

	std::shared_ptr<Camera> m_Camera;
	nvrhi::FramebufferHandle m_OffscreenFramebuffer;	

	std::vector<std::shared_ptr<RenderPass>> m_RenderPasses;

	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx