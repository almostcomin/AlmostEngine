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
	class RenderStage;
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

	static constexpr int c_BBSize = 0;

	RenderView(DeviceManager* deviceManager);
	~RenderView();

	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(st::rapi::FramebufferHandle frameBuffer);

	void SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages);

	std::shared_ptr<Camera> GetCamera() { return m_Camera; }
	st::rapi::FramebufferHandle GetFramebuffer();
	st::rapi::FramebufferHandle GetOffscreenFramebuffer() { return m_OffscreenFramebuffer; }
	st::rapi::CommandListHandle GetCommandList();

	bool CreateColorTarget(const char* id, int width, int height, rapi::Format format);
	bool CreateDepthTarget(const char* id, int width, int height, rapi::Format format);

	bool RequestTextureAccess(RenderStage* rp, AccessMode accessMode, const char* id, rapi::ResourceState inputState, rapi::ResourceState outputState);

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
		bool isDepthStencil;
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
		std::shared_ptr<RenderStage> renderPass;
	};

	void Refresh();

	std::shared_ptr<Camera> m_Camera;
	st::rapi::FramebufferHandle m_OffscreenFramebuffer;
	std::vector<st::rapi::CommandListHandle> m_CommandLists;

	std::map<std::string, DeclaredTexture> m_DeclaredTextures;

	std::vector<RenderPassDeps> m_RenderStages;

	bool m_IsDirty;

	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx