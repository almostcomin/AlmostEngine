#pragma once

#include "RHI/Resource.h"

namespace st::rhi
{
	class IFence : public IResource
	{
	public:

		virtual uint64_t GetCompletedValue() = 0;
	};

	using FenceHandle = st::weak<IFence>;
}