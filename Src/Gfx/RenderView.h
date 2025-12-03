#pragma once
#include <memory>
#include "Core/Util.h"
#include "Core/Memory.h"
#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/CommandList.h"

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
	~RenderView();

	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(st::rapi::FramebufferHandle frameBuffer);

	void SetRenderPasses(std::vector<std::shared_ptr<RenderPass>>&& renderPasses);

	std::shared_ptr<Camera> GetCamera() { return m_Camera; }
	st::rapi::FramebufferHandle GetFramebuffer();
	st::rapi::FramebufferHandle GetOffscreenFramebuffer() { return m_OffscreenFramebuffer; }
	st::rapi::CommandListHandle GetCommandList();

	DeviceManager* GetDeviceManager() const { return m_DeviceManager; }

	void Render();

private:

	std::shared_ptr<Camera> m_Camera;
	st::rapi::FramebufferHandle m_OffscreenFramebuffer;
	std::vector<st::rapi::CommandListHandle> m_CommandLists;

	std::vector<std::shared_ptr<RenderPass>> m_RenderPasses;

	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx