#pragma once
#include <memory>
#include "Core/Common.h"
#include "Core/Memory.h"
#include "RHI/Framebuffer.h"
#include "RHI/CommandList.h"
#include "RHI/Buffer.h"
#include "RHI/TimerQuery.h"
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

	using TextureViewTicket = void*;

	enum AccessMode
	{
		Read,
		Write
	};

	enum TextureResourceType
	{
		RenderTarget,
		DepthStencil,
		ShaderResource
	};

	struct DeclaredTexture
	{
		rhi::TextureOwner texture;
		std::string id;
		TextureResourceType type;
		int requestedWidth;
		int requestedHeight;
	};

	struct RenderStageTextureDep
	{
		std::string id;
		rhi::ResourceState inputState;
		rhi::ResourceState outputState;
	};

	struct RenderStageData
	{
		std::vector<RenderStageTextureDep> reads;
		std::vector<RenderStageTextureDep> writes;
		std::shared_ptr<RenderStage> renderStage;
		std::vector<rhi::TimerQueryOwner> timerQueries;
	};

	static constexpr int c_BBSize = 0;
	static constexpr int c_HalfBBSize = -1;

	RenderView(DeviceManager* deviceManager, const char* debugName);
	~RenderView();

	void SetScene(st::weak<Scene> scene);
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
	st::rhi::DescriptorIndex GetSceneConstantBufferDI();
	const std::vector<const st::gfx::MeshInstance*>& GetVisibleSet() const { return m_VisibleSet; }

	bool CreateColorTarget(const char* id, int width, int height, int arraySize, rhi::Format format);
	bool CreateDepthTarget(const char* id, int width, int height, int arraySize, rhi::Format format);
	bool CreateTexture(const char* id, TextureResourceType type, int width, int height, int arraySize, rhi::Format format, bool needUAV);

	bool RecreateTexture(const char* id, int width, int height, int arraySize, rhi::Format format);

	bool ReleaseTexture(const char* id);

	bool RequestTextureAccess(RenderStage* rs, AccessMode accessMode, const std::string& id, rhi::ResourceState inputState, rhi::ResourceState outputState);
	
	rhi::TextureHandle GetTexture(const std::string& id) const;
	rhi::DescriptorIndex GetShaderViewIndex(const std::string& id, rhi::TextureShaderView view);

	void OnWindowSizeChanged();

	void Render();

	size_t GetNumRenderStages() const { return m_RenderStages.size(); }
	const RenderStageData* GetRenderStage(uint32_t idx) const { return m_RenderStages[idx]; }

	TextureViewTicket RequestTextureView(RenderStage* rs, AccessMode accessMode, const std::string& id);
	void ReleaseTextureView(TextureViewTicket ticket);
	rhi::TextureHandle GetTextureView(TextureViewTicket ticket);

	std::string GetName() const { return m_DebugName; }
	DeviceManager* GetDeviceManager() const { return m_DeviceManager; }

private:

	void ClearRenderStages();
	void UpdateSceneConstantBuffer();
	void UpdateVisibleSet();
	float4x4 GetSunWoldToClipMatrix(const float3& sunDir);

private:

	struct TextureViewRequest
	{
		RenderStage* rs;
		AccessMode accessMode;
		std::string id;
		int refCount;
		rhi::TextureOwner tex;
	};

	struct RenderStageInFrameData
	{
		
	};

private:

	void Refresh();

	std::vector<TextureViewRequest*> GetTexViewRequests(RenderStage* rs, AccessMode accessMode);
	void UpdateTextureViews(st::rhi::CommandListHandle commandList, RenderStage* rs, AccessMode accessMode, const std::map<std::string, rhi::ResourceState> resourcesStates);

private:

	st::weak<Scene> m_Scene;
	std::shared_ptr<Camera> m_Camera;

	st::rhi::FramebufferHandle m_OffscreenFramebuffer;
	std::vector<st::rhi::CommandListOwner> m_CommandLists;

	std::vector<RenderStageData*> m_RenderStages;
	std::map<std::string, std::unique_ptr<DeclaredTexture>> m_DeclaredTextures;

	// Visible set for the current camera
	std::vector<const st::gfx::MeshInstance*> m_VisibleSet;

	// Scene constant buffer, set at begin frame, no changes during frame render
	st::rhi::BufferOwner m_SceneCB;

	// Request for visualizing render targets
	std::vector<TextureViewRequest*> m_TexViewRequests;

	int m_NextTimerToUse = 0;

	// True if a renderstage has added or removed
	bool m_IsDirty;

	std::string m_DebugName;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx