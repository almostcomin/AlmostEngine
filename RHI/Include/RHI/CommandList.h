#pragma once

#include "RHI/Resource.h"
#include "RHI/Barriers.h"
#include "RHI/Texture.h"
#include "Core/Memory.h"
#include <span>

namespace alm::rhi
{

class IGraphicsPipelineState;
class IFramebuffer;
class ViewportState;

enum class QueueType : uint8_t
{
	Graphics = 0,
	Compute,
	Copy,

	_Count
};

struct CommandListParams
{
	QueueType queueType = QueueType::Graphics;
};

union ClearValue
{
	float4 color;
	struct DepthStencil
	{
		float depth;
		uint32_t stencil;
	} depthStencil;

	static constexpr ClearValue ColorBlack() { return ClearValue{ float4{ 0.f } }; }
	static constexpr ClearValue DepthZero() { return ClearValue{ .depthStencil{ 0.f, 0u } }; }
	static constexpr ClearValue DepthOne() { return ClearValue{ .depthStencil{ 1.f, 0u } }; }

	static constexpr ClearValue Color(const float4& c) { return ClearValue{ c }; }
};

struct RenderPassOp
{
	enum class LoadOp
	{
		NoAccess,
		Load,
		Clear,
		Discard
	};
	enum class StoreOp
	{
		NoAccess,
		Store,
		Discard
	};

	LoadOp loadOp = LoadOp::NoAccess;
	StoreOp storeOp = StoreOp::NoAccess;
	ClearValue clearValue = {};
};

enum class RenderPassFlags : uint8_t
{
	None				= 0,
	AllowUAVWrites		= 0x01
};
ENUM_CLASS_FLAG_OPERATORS(RenderPassFlags)

class ICommandList : public IResource
{
public:

	virtual void Open() = 0;
	virtual void Close() = 0;

	virtual void WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size) = 0;
	virtual void WriteTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset) = 0;

	virtual void CopyTextureToTexture(ITexture* dstTexture, ITexture* srcTexture) = 0;
	virtual void CopyTextureToTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& dstSubresources,
		ITexture* srcTexture, const rhi::TextureSubresourceSet& srcSubresources) = 0;

	virtual void CopyBufferToBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, uint64_t size) = 0;

	virtual void PushBarriers(std::span<const Barrier> barriers) = 0;
	void PushBarriers(std::initializer_list<Barrier> barriers)
	{
		PushBarriers(std::span<const Barrier>(barriers.begin(), barriers.size()));
	}
	virtual void PushBarrier(const Barrier& barrier)
	{
		PushBarriers(std::span{ &barrier, 1 });
	}

	virtual void SetPipelineState(IGraphicsPipelineState* pso) = 0;
	virtual void SetPipelineState(IComputePipelineState* pso) = 0;

	virtual void SetViewport(const rhi::ViewportState& vp) = 0;

	virtual void SetStencilRef(uint8_t value) = 0;

	virtual void SetBlendFactor(const float4& value) = 0;

	virtual void PushConstants(uint32_t slot, const void* data, size_t sizeBytes, size_t offsetBytes, bool isCompute) = 0;
	template<class T>
	void PushGraphicsConstants(uint32_t slot, const T& data) { PushConstants(slot, (const void*)&data, sizeof(T), 0, false); }
	template<class T>
	void PushComputeConstants(uint32_t slot, const T& data) { PushConstants(slot, (const void*)&data, sizeof(T), 0, true); }

	virtual void BeginRenderPass(rhi::IFramebuffer* fb, const std::vector<RenderPassOp>& renderPassOp, 
		const RenderPassOp& depthRenderPassOp, const RenderPassOp& stencilRenderPassOp, RenderPassFlags flags) = 0;
	virtual void EndRenderPass() = 0;

	virtual void Draw(uint32_t vertexCount) = 0;
	virtual void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertex) = 0;

	virtual void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) = 0;

	virtual void Discard(IBuffer* buffer) = 0;
	virtual void Discard(ITexture* texture, int mipLevel = -1, int arraySlice = -1) = 0;

	virtual void BeginMarker(const char* str) = 0;
	virtual void EndMarker() = 0;

	virtual void BeginTimerQuery(ITimerQuery* query) = 0;
	virtual void EndTimerQuery(ITimerQuery* query) = 0;

	virtual QueueType GetType() const = 0;

protected:
	
	ICommandList(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

} // namespace st::rhi