#pragma once

#include "RHI/Format.h"
#include "RHI/Buffer.h"
#include "Gfx/ViewportSwapChain.h"
#include "RHI/ResourceState.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageType.h"

namespace alm::gfx
{
	class RenderStage;
	class RenderView;
	class DeviceManager;
}

namespace alm::gfx
{

class RenderGraph : public alm::enable_weak_from_this<RenderGraph>, private alm::noncopyable_nonmovable
{
public:

	static constexpr int c_BBSize = 0;
	static constexpr int c_HalfBBSize = -1;

	enum TextureResourceType
	{
		RenderTarget,
		DepthStencil,
		ShaderResource
	};

	enum AccessMode
	{
		Read,
		Write
	};

	struct TextureDependency
	{
		RGTextureHandle handle;
		rhi::ResourceState inputState;
		rhi::ResourceState outputState;
	};

	struct BufferDependency
	{
		RGBufferHandle handle;
		rhi::ResourceState inputState;
		rhi::ResourceState outputState;
	};

	struct StageData
	{
		std::shared_ptr<RenderStage> renderStage;
		std::vector<TextureDependency> textureReads;
		std::vector<TextureDependency> textureWrites;
		std::vector<BufferDependency> bufferReads;
		std::vector<BufferDependency> bufferWrites;
		std::vector<rhi::TimerQueryOwner> timerQueries;
		std::vector<float> cpuElapsed;
	};

public:

	RenderGraph(RenderView* renderView, const char* debugName);
	~RenderGraph();

	void Reset();

	void SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages);
	void SetRenderMode(const std::string& name, const std::vector<RenderStage*>& renderStages);
	void SetActiveRenderMode(const std::string& name);
	const std::string& GetCurrentRenderMode() const { return m_CurrentRenderMode; }
	std::vector<std::string> GetRenderModes() const;

	void Compile();

	bool BeginRender(rhi::ICommandList* commandList);
	bool EndRender(rhi::ICommandList* commandList);
	void Render(alm::rhi::FramebufferHandle frameBuffer);

	void OnSceneChanged();
	void OnRenderTargetChanged(const int2& newSize);

	RGTextureHandle CreateTexture(RenderStage* renderStage, const std::string& id, TextureResourceType type, int width, int height, int arraySize,
								rhi::Format format, bool needsUAV);
	RGBufferHandle CreateBuffer(RenderStage* renderStage, const std::string& id, const rhi::BufferDesc& desc);

	bool RecreateTexture(RGTextureHandle handle, int width, int height, int arraySize, rhi::Format format);
	bool RecreateBuffer(RGBufferHandle handle, const rhi::BufferDesc& desc);

	void EnableTexture(RGTextureHandle handle);
	void DisableTexture(RGTextureHandle handle);

	RGTextureHandle GetTextureHandle(const std::string& id);
	RGBufferHandle GetBufferHandle(const std::string& id);

	rhi::TextureHandle GetTexture(RGTextureHandle handle);
	rhi::BufferHandle GetBuffer(RGBufferHandle handle);

	rhi::TextureHandle GetTexture(const std::string& id);
	rhi::BufferHandle GetBuffer(const std::string& id);

	const std::string& GetId(RGTextureHandle handle);
	const std::string& GetId(RGBufferHandle handle);

	RGFramebufferHandle RequestFramebuffer(const std::vector<RGTextureHandle>& colorTargets, RGTextureHandle depthTarget);
	rhi::FramebufferHandle GetFrameBuffer(RGFramebufferHandle handle);

	rhi::TextureSampledView GetTextureSampledView(RGTextureHandle handle);
	rhi::TextureStorageView GetTextureStorageView(RGTextureHandle handle);

	rhi::BufferUniformView GetBufferUniformView(RGBufferHandle handle);
	rhi::BufferReadOnlyView GetBufferReadOnlyView(RGBufferHandle handle);
	rhi::BufferReadWriteView GetBufferReadWriteView(RGBufferHandle handle);

	alm::rhi::CommandListHandle GetCommandList();

	size_t GetNumRenderStages(const std::string& mode = {}) const;

	const StageData* GetRenderStageData(RenderStageTypeID id);
	const StageData* GetRenderStageData(uint32_t idx, const std::string& mode = {}) const;
	
	std::shared_ptr<RenderStage> GetRenderStage(RenderStageTypeID id);
	template<class T> 
	std::shared_ptr<T> GetRenderStage()
	{
		auto ptr = GetRenderStage(T::StaticType());
		return ptr ? std::dynamic_pointer_cast<T>(ptr) : nullptr;
	}

	RGTextureViewTicket RequestTextureView(RenderStage* rs, AccessMode accessMode, RGTextureHandle handle);
	RGBufferViewTicket RequestBufferView(RenderStage* rs, AccessMode accessMode, RGBufferHandle handle);
	void ReleaseTextureView(RGTextureViewTicket ticket);
	void ReleaseBufferView(RGBufferViewTicket ticket);
	rhi::TextureHandle GetTextureView(RGTextureViewTicket ticket);
	rhi::BufferHandle GetBufferView(RGBufferViewTicket ticket);

	alm::rhi::FramebufferHandle GetFramebuffer();
	RenderView* GetRenderView() { return m_RenderView; }
	DeviceManager* GetDeviceManager() { return m_DeviceManager; }

private:

	struct DeclaredTexture
	{
		std::string id;
		rhi::TextureOwner texture;
		TextureResourceType type;
		int requestedWidth;
		int requestedHeight;
		int arraySize;
		rhi::Format format;
		bool needsUAV;
		RenderStage* owner;
		std::vector<RGFramebufferHandle> framebuffers;
	};

	struct DeclaredBuffer
	{
		std::string id;
		rhi::BufferOwner buffer;
		RenderStage* owner;
	};

	struct RequestedFramebufferData
	{
		std::vector<RGTextureHandle> colorTargets;
		RGTextureHandle depthTarget;
		rhi::FramebufferOwner fb;
		int refCount = 0;
	};

	struct TextureViewRequest
	{
		RenderStage* rs;
		AccessMode accessMode;
		RGTextureHandle handle;
		int refCount;
		rhi::TextureOwner tex;
	};

	struct BufferViewRequest
	{
		RenderStage* rs;
		AccessMode accessMode;
		RGBufferHandle handle;
		int refCount;
		rhi::BufferOwner buffer;
	};

private:

	DeclaredTexture* GetDeclTex(RGTextureHandle handle);
	DeclaredBuffer* GetDeclBuffer(RGBufferHandle handle);
	RequestedFramebufferData* GetReqFb(RGFramebufferHandle handle);

	RGTextureHandle GetHandle(DeclaredTexture* declTex);
	RGBufferHandle GetHandle(DeclaredBuffer* declBuffer);

	std::vector<TextureViewRequest*> GetTexViewRequests(RenderStage* rs, AccessMode accessMode);
	void UpdateRequestedTextureViews(alm::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
		const std::map<RGTextureHandle, rhi::ResourceState> resourceStates);

	std::vector<BufferViewRequest*> GetBufferViewRequests(RenderStage* rs, AccessMode accessMode);
	void UpdateRequestedBufferViews(alm::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
		const std::map<RGBufferHandle, rhi::ResourceState> resourceStates);

private:

	std::map<std::string, std::unique_ptr<DeclaredTexture>> m_Textures;
	std::map<std::string, std::unique_ptr<DeclaredBuffer>> m_Buffers;
	std::set<std::unique_ptr<RequestedFramebufferData>> m_Framebuffers;

	std::vector<std::unique_ptr<StageData>> m_RenderStages;
	std::unordered_map<std::string, std::vector<StageData*>> m_RenderModes;
	std::string m_CurrentRenderMode;

	// In-flight render texture & buffer states
	std::map<RGTextureHandle, rhi::ResourceState> m_TexturesState;
	std::map<RGBufferHandle, rhi::ResourceState> m_BuffersState;

	// Living requests for visualizing resources
	std::vector<TextureViewRequest*> m_TexViewRequests;
	std::vector<BufferViewRequest*> m_BufferViewRequests;

	std::vector<alm::rhi::CommandListOwner> m_CommandLists;

	std::string m_DebugName;
	RenderView* m_RenderView;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx