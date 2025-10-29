#pragma once

#include "RenderAPI/Resource.h"

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

		virtual void WriteBuffer(IBuffer* buffer, const void* data, size_t dataSize, uint64_t offset);

		virtual void BeginMarker(const char* str) = 0;
		virtual void EndMarker() = 0;

		virtual QueueType GetType() const = 0;

	};

	using CommandListHandle = std::shared_ptr<ICommandList>;
}