#pragma once

#include "RenderAPI/Resource.h"
#include "RenderAPI/Barriers.h"
#include <span>

namespace st::rapi
{
	class IBuffer;

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

		virtual void WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size) = 0;

		virtual void PushBarriers(std::span<Barrier> barriers) = 0;

		virtual void BeginMarker(const char* str) = 0;
		virtual void EndMarker() = 0;

		virtual QueueType GetType() const = 0;

	};

	using CommandListHandle = std::shared_ptr<ICommandList>;
}