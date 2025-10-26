#pragma once

#include <memory>

namespace st::rapi
{
	class IResource : public std::enable_shared_from_this<IResource>
	{
	protected:

		IResource() = default;
		virtual ~IResource() = default;
	};
}