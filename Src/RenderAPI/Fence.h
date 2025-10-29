#pragma once

#include "RenderAPI/Resource.h"

namespace st::rapi
{
	class IFence : public IResource
	{};

	using FenceHandle = std::shared_ptr<IFence>;
}