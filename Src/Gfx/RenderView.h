#pragma once
#include <memory>
#include "Core/Common.h"
#include "Core/Memory.h"
#include "RHI/Framebuffer.h"
#include "RHI/CommandList.h"
#include "RHI/Buffer.h"
#include "Gfx/Scene.h"
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

	RenderView(DeviceManager* deviceManager, const char* debugName);
	~RenderView();

	void SetScene(st::weak<Scene> scene) { m_Scene = scene; }
	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(st::rhi::FramebufferHandle frameBuffer);

	void SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages);

	st::weak<Scene> GetScene() { return m_Scene; }
	std::shared_ptr<Camera> GetCamera() { return m_Camera; }
	st::rhi::FramebufferHandle GetFramebuffer();
	st::rhi::FramebufferHandle GetOffscreenFramebuffer() { return m_OffscreenFramebuffer; }
	st::rhi::TextureHandle GetBackBuffer(int idx = 0);
	st::rhi::CommandListHandle GetCommandList();
	st::rhi::DescriptorIndex GetSceneBufferDI();

	bool CreateColorTarget(const char* id, int width, int height, rhi::Format format);
	bool CreateDepthTarget(const char* id, int width, int height, rhi::Format format);

	bool RequestTextureAccess(RenderStage* rp, AccessMode accessMode, const char* id, rhi::ResourceState inputState, rhi::ResourceState outputState);

	rhi::TextureHandle GetTexture(const char* id) const;

	DeviceManager* GetDeviceManager() const { return m_DeviceManager; }

	void Render();

private:

	void CleanRenderPasses();
	void UpdateSceneBuffer();

private:

	struct DeclaredTexture
	{
		rhi::TextureHandle texture;
		std::string id;
		bool isDepthStencil;
	};

	struct RenderPassTextureDep
	{
		DeclaredTexture declTex;
		AccessMode accessMode;
		rhi::ResourceState inputState;
		rhi::ResourceState outputState;
	};

	struct RenderPassDeps
	{
		std::vector<RenderPassTextureDep> textureDeps;
		std::shared_ptr<RenderStage> renderPass;
	};

	void Refresh();

	st::weak<Scene> m_Scene;
	std::shared_ptr<Camera> m_Camera;

	st::rhi::FramebufferHandle m_OffscreenFramebuffer;
	std::vector<st::rhi::CommandListHandle> m_CommandLists;

	std::map<std::string, DeclaredTexture> m_DeclaredTextures;

	st::rhi::BufferHandle m_SceneCB;

	std::vector<RenderPassDeps> m_RenderStages;

	bool m_IsDirty;

	std::string m_DebugName;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx