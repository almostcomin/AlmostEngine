#pragma once

#include "RenderAPI/Resource.h"

namespace st::rapi
{
	class ICommandList : public IResource
	{
	public:
	};

	using CommandListHandle = std::shared_ptr<ICommandList>;
}