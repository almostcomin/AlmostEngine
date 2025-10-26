#pragma once

#include "RenderAPI/Resource.h"

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

		virtual QueueType GetType() const = 0;

	};

	using CommandListHandle = std::shared_ptr<ICommandList>;
}