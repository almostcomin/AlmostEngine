#pragma once
#include <memory>
#include "Core/Util.h"
#include "Core/Memory.h"
#include "RenderAPI/Framebuffer.h"
#include "RenderAPI/CommandList.h"
#include <map>

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

	enum AccessMode
	{
		Read,
		Write
	};

	RenderView(DeviceManager* deviceManager);
	~RenderView();

	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(st::rapi::FramebufferHandle frameBuffer);

	void SetRenderPasses(const std::vector<std::shared_ptr<RenderPass>>& renderPasses);

	std::shared_ptr<Camera> GetCamera() { return m_Camera; }
	st::rapi::FramebufferHandle GetFramebuffer();
	st::rapi::FramebufferHandle GetOffscreenFramebuffer() { return m_OffscreenFramebuffer; }
	st::rapi::CommandListHandle GetCommandList();

	bool CreateTexture(const rapi::TextureDesc& desc, const char* id);
	bool RequestTextureAccess(RenderPass* rp, AccessMode accessMode, const char* id, rapi::ResourceState inputState, rapi::ResourceState outputState);
	rapi::TextureHandle GetTexture(const char* id) const;

	DeviceManager* GetDeviceManager() const { return m_DeviceManager; }

	void Render();

private:

	void CleanRenderPasses();

private:

	struct DeclaredTexture
	{
		rapi::TextureHandle texture;
		std::string id;
	};

	struct RenderPassTextureDep
	{
		DeclaredTexture declTex;
		AccessMode accessMode;
		rapi::ResourceState inputState;
		rapi::ResourceState outputState;
	};

	struct RenderPassDeps
	{
		std::vector<RenderPassTextureDep> textureDeps;
		std::shared_ptr<RenderPass> renderPass;
	};

	void Refresh();

	std::shared_ptr<Camera> m_Camera;
	st::rapi::FramebufferHandle m_OffscreenFramebuffer;
	std::vector<st::rapi::CommandListHandle> m_CommandLists;

	std::map<std::string, DeclaredTexture> m_DeclaredTextures;

	std::vector<RenderPassDeps> m_RenderPasses;

	bool m_IsDirty;

	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx