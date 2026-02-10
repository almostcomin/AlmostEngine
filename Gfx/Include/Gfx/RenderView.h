#pragma once

#include "Core/Common.h"
#include "Core/Memory.h"
#include "Core/Math/plane.h"
#include "RHI/Framebuffer.h"
#include "RHI/CommandList.h"
#include "RHI/Buffer.h"
#include "RHI/TimerQuery.h"
#include "Gfx/Scene.h"

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
	using BufferViewTicket = void*;

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
		TextureResourceType type;
		int requestedWidth;
		int requestedHeight;
	};

	struct DeclaredBuffer
	{
		rhi::BufferOwner buffer;
	};

	struct RenderStageResourceDep
	{
		std::string id;
		rhi::ResourceState inputState;
		rhi::ResourceState outputState;
	};

	struct RenderStageData
	{
		std::shared_ptr<RenderStage> renderStage;
		std::vector<RenderStageResourceDep> textureReads;
		std::vector<RenderStageResourceDep> textureWrites;
		std::vector<RenderStageResourceDep> bufferReads;
		std::vector<RenderStageResourceDep> bufferWrites;
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
	
	const std::vector<const st::gfx::MeshInstance*>& GetCameraVisibleSet() const { return m_CameraVisibleSet; }
	const std::vector<const st::gfx::MeshInstance*>& GetSunVisibleSet() const { return m_SunVisibleSet; }

	// Texture creation / release
	bool CreateColorTarget(const char* id, int width, int height, int arraySize, rhi::Format format);
	bool CreateDepthTarget(const char* id, int width, int height, int arraySize, rhi::Format format);
	bool CreateTexture(const char* id, TextureResourceType type, int width, int height, int arraySize, rhi::Format format, bool needUAV);
	bool RecreateTexture(const char* id, int width, int height, int arraySize, rhi::Format format);
	bool ReleaseTexture(const char* id);

	// Buffer creation / release
	bool CreateBuffer(const std::string& id, const rhi::BufferDesc& desc);
	bool RecreateBuffer(const std::string& id, const rhi::BufferDesc& desc);
	bool ReleaseBuffer(const std::string& id);

	// Request access
	bool RequestTextureAccess(RenderStage* rs, AccessMode accessMode, const std::string& id, rhi::ResourceState inputState, rhi::ResourceState outputState);
	bool RequestBufferAccess(RenderStage* rs, AccessMode accessMode, const std::string& id, rhi::ResourceState inputState, rhi::ResourceState outputState);
	
	// Access
	rhi::TextureHandle GetTexture(const std::string& id) const;
	rhi::BufferHandle GetBuffer(const std::string& id) const;

	rhi::TextureSampledView GetSampledView(const std::string& id);
	rhi::TextureStorageView GetStorageView(const std::string& id);

	rhi::DescriptorIndex GetShaderViewIndex(const std::string& id, rhi::BufferShaderView view);

	void OnWindowSizeChanged();

	void Render(float timeDeltaSec);

	size_t GetNumRenderStages() const { return m_RenderStages.size(); }
	const RenderStageData* GetRenderStage(uint32_t idx) const { return m_RenderStages[idx]; }

	TextureViewTicket RequestTextureView(RenderStage* rs, AccessMode accessMode, const std::string& id);
	BufferViewTicket RequestBufferView(RenderStage* rs, AccessMode accessMode, const std::string& id);
	void ReleaseTextureView(TextureViewTicket ticket);
	void ReleaseBufferView(BufferViewTicket ticket);
	rhi::TextureHandle GetTextureView(TextureViewTicket ticket);
	rhi::BufferHandle GetBufferView(BufferViewTicket ticket);

	float GetTimeDelta() const { return m_TimeDeltaSec; }

	std::string GetName() const { return m_DebugName; }
	DeviceManager* GetDeviceManager() const { return m_DeviceManager; }

private:

	void ClearRenderStages();
	void UpdateSceneConstantBuffer();
	void UpdateCameraVisibleSet();
	void UpdateShadowmapData();

	std::vector<const st::gfx::MeshInstance*> GetVisibleSet(const std::span<const math::plane3f>& planes, math::aabox3f* opt_outBounds = nullptr) const;

private:

	struct TextureViewRequest
	{
		RenderStage* rs;
		AccessMode accessMode;
		std::string id;
		int refCount;
		rhi::TextureOwner tex;
	};

	struct BufferViewRequest
	{
		RenderStage* rs;
		AccessMode accessMode;
		std::string id;
		int refCount;
		rhi::BufferOwner buffer;
	};

private:

	void Refresh();

	std::vector<TextureViewRequest*> GetTexViewRequests(RenderStage* rs, AccessMode accessMode);
	void UpdateRequestedTextureViews(st::rhi::CommandListHandle commandList, RenderStage* rs, AccessMode accessMode,
		const std::map<std::string, rhi::ResourceState> resourceStates);

	std::vector<BufferViewRequest*> GetBufferViewRequests(RenderStage* rs, AccessMode accessMode);
	void UpdateRequestedBufferViews(st::rhi::CommandListHandle commandList, RenderStage* rs, AccessMode accessMode,
		const std::map<std::string, rhi::ResourceState> resourceStates);

private:

	st::weak<Scene> m_Scene;
	std::shared_ptr<Camera> m_Camera;

	st::rhi::FramebufferHandle m_OffscreenFramebuffer;
	std::vector<st::rhi::CommandListOwner> m_CommandLists;

	std::vector<RenderStageData*> m_RenderStages;

	std::map<std::string, std::unique_ptr<DeclaredTexture>> m_DeclaredTextures;
	std::map<std::string, std::unique_ptr<DeclaredBuffer>> m_DeclaredBuffers;

	// Visible set for the current camera
	std::vector<const st::gfx::MeshInstance*> m_CameraVisibleSet;
	math::aabox3f m_CameraVisibleBounds;

	// Visible set for the sun (for shadowmapping)
	std::vector<const st::gfx::MeshInstance*> m_SunVisibleSet;
	float4x4 m_SunWoldToClipMatrix;

	// Scene constant buffer, set at begin frame, no changes during frame render
	st::rhi::BufferOwner m_SceneCB;

	// Living requests for visualizing resources
	std::vector<TextureViewRequest*> m_TexViewRequests;
	std::vector<BufferViewRequest*> m_BufferViewRequests;

	float m_TimeDeltaSec = 0.f;

	// True if a renderstage has added or removed
	bool m_IsDirty;

	std::string m_DebugName;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx