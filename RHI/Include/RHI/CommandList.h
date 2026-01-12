#pragma once

#include "RHI/Resource.h"
#include "RHI/Barriers.h"
#include "RHI/Texture.h"
#include "Core/Memory.h"
#include <span>

namespace st::rhi
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
};

struct RenderPassOp
{
	enum class LoadOp
	{
		Load,
		Clear,
		Discard
	};
	enum class StoreOp
	{
		Store,
		Discard
	};

	LoadOp loadOp = LoadOp::Load;
	StoreOp storeOp = StoreOp::Store;
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

	virtual void CopyTextureToTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& dstSubresources,
		ITexture* srcTexture, const rhi::TextureSubresourceSet& srcSubresources) = 0;

	virtual void PushBarriers(std::span<const Barrier> barriers) = 0;
	virtual void PushBarrier(const Barrier& barrier) = 0;

	virtual void SetPipelineState(IGraphicsPipelineState* pso) = 0;

	virtual void SetViewport(const rhi::ViewportState& vp) = 0;

	virtual void SetStencilRef(uint8_t value) = 0;

	virtual void SetBlendFactor(const float4& value) = 0;

	virtual void PushConstants(const void* data, size_t sizeBytes, size_t offsetBytes) = 0;
	template<class T>
	void PushConstants(const T& data) { PushConstants((const void*)&data, sizeof(T), 0); }

	virtual void BeginRenderPass(rhi::IFramebuffer* fb, const std::vector<RenderPassOp>& renderPassOp, const RenderPassOp& dsvRenderPassOp, RenderPassFlags flags) = 0;
	virtual void EndRenderPass() = 0;

	virtual void Draw(uint32_t vertexCount) = 0;
	virtual void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount) = 0;

	virtual void Discard(IBuffer* buffer) = 0;
	virtual void Discard(ITexture* texture, int mipLevel = -1, int arraySlice = -1) = 0;

	virtual void BeginMarker(const char* str) = 0;
	virtual void EndMarker() = 0;

	virtual QueueType GetType() const = 0;

protected:
	
	ICommandList(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

using CommandListHandle = st::weak<ICommandList>;

} // namespace st::rhi