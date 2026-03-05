#pragma once

#include "Core/Common.h"
#include "Core/Memory.h"
#include "Core/Math/plane.h"
#include "RHI/Framebuffer.h"
#include "RHI/CommandList.h"
#include "RHI/Buffer.h"
#include "RHI/TimerQuery.h"
#include "RHI/RasterizerState.h"
#include "Gfx/FrameUniformBuffer.h"
#include "Gfx/Scene.h"
#include "Gfx/ViewportSwapChain.h"

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
		std::vector<float> cpuElapsed;
	};

	using RenderSet = std::vector<std::pair<rhi::CullMode, std::vector<const st::gfx::MeshInstance*>>>;

	static constexpr int c_BBSize = 0;
	static constexpr int c_HalfBBSize = -1;

	RenderView(DeviceManager* deviceManager, const char* debugName);
	RenderView(ViewportSwapChainId, DeviceManager* deviceManager, const char* debugName);
	~RenderView();

	void SetScene(st::weak<Scene> scene);
	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(st::rhi::FramebufferHandle frameBuffer);

	void SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages);
	void SetRenderMode(const std::string& name, const std::vector<RenderStage*>& renderStages);
	void SetCurrentRenderMode(const std::string& name);
	const std::string& GetCurrentRenderMode() const { return m_CurrentRenderMode; }
	std::vector<std::string> GetRenderModes() const;

	st::weak<Scene> GetScene() { return m_Scene; }
	std::shared_ptr<Camera> GetCamera() { return m_Camera; }
	st::rhi::FramebufferHandle GetFramebuffer();
	st::rhi::FramebufferHandle GetOffscreenFramebuffer() { return m_OffscreenFramebuffer; }
	st::rhi::TextureHandle GetBackBuffer(int idx = 0);
	st::rhi::CommandListHandle GetCommandList();

	st::rhi::BufferUniformView GetSceneBufferUniformView();
	st::rhi::BufferReadOnlyView GetCameraVisiblityBufferROView();
	st::rhi::BufferReadOnlyView GetSunVisibilityBufferROView();
	
	const RenderSet& GetCameraVisibleSet() const { return m_CameraVisibleSet; }
	const RenderSet& GetSunVisibleSet() const { return m_SunVisibleSet; }

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

	rhi::TextureSampledView GetTextureSampledView(const std::string& id);
	rhi::TextureStorageView GetTextureStorageView(const std::string& id);
	
	rhi::BufferUniformView GetBufferUniformView(const std::string& id);
	rhi::BufferReadOnlyView GetBufferReadOnlyView(const std::string& id);
	rhi::BufferReadWriteView GetBufferReadWriteView(const std::string& id);

	void OnWindowSizeChanged();

	void Render(float timeDeltaSec);

	size_t GetNumRenderStages(const std::string& mode = {}) const;
	const RenderStageData* GetRenderStage(uint32_t idx, const std::string& mode = {}) const;

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
	void UpdateCameraVisibleSet(rhi::ICommandList* commandList);
	void UpdateShadowmapData(rhi::ICommandList* commandList);

	RenderSet GetVisibleSet(const std::span<const math::plane3f>& planes, math::aabox3f* opt_outBounds = nullptr) const;

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
	void UpdateRequestedTextureViews(st::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
		const std::map<std::string, rhi::ResourceState> resourceStates);

	std::vector<BufferViewRequest*> GetBufferViewRequests(RenderStage* rs, AccessMode accessMode);
	void UpdateRequestedBufferViews(st::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
		const std::map<std::string, rhi::ResourceState> resourceStates);

	void UpdateVisibilityShaderBuffer(const RenderSet& renderSet, rhi::BufferOwner& buffer, rhi::ICommandList* commandList);

private:

	ViewportSwapChainId m_ViewportSwapChainId;

	st::weak<Scene> m_Scene;
	std::shared_ptr<Camera> m_Camera;

	st::rhi::FramebufferHandle m_OffscreenFramebuffer;
	std::vector<st::rhi::CommandListOwner> m_CommandLists;

	std::vector<std::unique_ptr<RenderStageData>> m_RenderStages;

	std::string m_CurrentRenderMode;
	std::unordered_map<std::string, std::vector<RenderStageData*>> m_RenderModes;

	std::map<std::string, std::unique_ptr<DeclaredTexture>> m_DeclaredTextures;
	std::map<std::string, std::unique_ptr<DeclaredBuffer>> m_DeclaredBuffers;

	// Visible set for the current camera
	RenderSet m_CameraVisibleSet;
	math::aabox3f m_CameraVisibleBounds;
	std::vector<rhi::BufferOwner> m_CameraVisibleBuffer;
	std::vector<rhi::BufferOwner> m_SunVisibleBuffer;

	// Visible set for the sun (for shadowmapping)
	RenderSet m_SunVisibleSet;
	float4x4 m_SunWoldToClipMatrix;
	float4x4 m_ViewToSunClipMatrix;

	// Scene constant buffer, set at begin frame, no changes during frame render
	FrameUniformBufferRaw m_SceneCB;

	// Living requests for visualizing resources
	std::vector<TextureViewRequest*> m_TexViewRequests;
	std::vector<BufferViewRequest*> m_BufferViewRequests;

	// Begin & End command lists
	std::vector<rhi::CommandListOwner> m_BeginCommandLists;
	std::vector<rhi::CommandListOwner> m_EndCommandLists;
	// Submit sync fence
	std::vector<rhi::FenceOwner> m_SubmitFences;

	float m_TimeDeltaSec = 0.f;

	// True if a renderstage has added or removed
	bool m_IsDirty;

	std::string m_DebugName;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx