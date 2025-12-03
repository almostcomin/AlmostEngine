#pragma once

#include "RenderAPI/Resource.h"

namespace st::rapi
{
	class IFence : public IResource
	{
	public:

		virtual uint64_t GetCompletedValue() = 0;
	};

	using FenceHandle = st::weak<IFence>;
}