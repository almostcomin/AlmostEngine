#pragma once

#include "RHI/Resource.h"

namespace st::rhi
{
	class ITimerQuery : public IResource
	{
	public:
		virtual void Reset() = 0;
		virtual float GetQueryTimeMs() = 0; // milliseconds
		virtual bool Poll() = 0;
	protected:
		ITimerQuery(Device* device, const std::string& debugName) : IResource(device, debugName) {};
	};
}