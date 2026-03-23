#pragma once

#include "RHI/Resource.h"

namespace alm::rhi
{
	class IFence : public IResource
	{
	public:
		virtual uint64_t GetCompletedValue() = 0;
	protected:
		IFence(Device* device, const std::string& debugName) : IResource(device, debugName) {};
	};
}