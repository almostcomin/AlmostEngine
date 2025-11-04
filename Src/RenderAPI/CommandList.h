#pragma once

#include "RenderAPI/Resource.h"
#include "RenderAPI/Barriers.h"
#include "RenderAPI/Texture.h"
#include <span>

namespace st::rapi
{

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

class ICommandList : public IResource
{
public:

	virtual void Open() = 0;
	virtual void Close() = 0;

	virtual void WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size) = 0;
	virtual void WriteTexture(ITexture* dstTexture, const rapi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset) = 0;

	virtual void PushBarriers(std::span<const Barrier> barriers) = 0;
	virtual void PushBarrier(const Barrier& barrier) = 0;

	virtual void Discard(IBuffer* buffer) = 0;
	virtual void Discard(ITexture* texture, int mipLevel = -1, int arraySlice = -1) = 0;

	virtual void BeginMarker(const char* str) = 0;
	virtual void EndMarker() = 0;

	virtual QueueType GetType() const = 0;
};

using CommandListHandle = std::shared_ptr<ICommandList>;

} // namespace st::rapi