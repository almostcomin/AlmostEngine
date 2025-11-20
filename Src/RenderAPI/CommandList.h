#pragma once

#include "RenderAPI/Resource.h"
#include "RenderAPI/Barriers.h"
#include "RenderAPI/Texture.h"
#include <span>

namespace st::rapi
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

	static constexpr float4 Black = float4{ 0.f };
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
	virtual void WriteTexture(ITexture* dstTexture, const rapi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset) = 0;

	virtual void PushBarriers(std::span<const Barrier> barriers) = 0;
	virtual void PushBarrier(const Barrier& barrier) = 0;

	virtual void SetPipelineState(IGraphicsPipelineState* pso) = 0;

	virtual void SetViewport(const rapi::ViewportState& vp) = 0;

	virtual void SetStencilRef(uint8_t value) = 0;

	virtual void SetBlendFactor(const float4& value) = 0;

	virtual void PushConstants(const uint32_t* data, size_t numElements, size_t elementsOffset) = 0;

	virtual void BeginRenderPass(rapi::IFramebuffer* fb, const std::vector<RenderPassOp>& renderPassOp, const RenderPassOp& dsvRenderPassOp, RenderPassFlags flags) = 0;
	virtual void EndRenderPass() = 0;

	virtual void DrawIndexed(uint32_t indexCount) = 0;

	virtual void Discard(IBuffer* buffer) = 0;
	virtual void Discard(ITexture* texture, int mipLevel = -1, int arraySlice = -1) = 0;

	virtual void BeginMarker(const char* str) = 0;
	virtual void EndMarker() = 0;

	virtual QueueType GetType() const = 0;
};

using CommandListHandle = std::shared_ptr<ICommandList>;

} // namespace st::rapi