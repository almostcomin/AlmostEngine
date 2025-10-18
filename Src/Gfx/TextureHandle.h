#pragma once

#include <nvrhi/nvrhi.h>
#include "Core/Util.h"

namespace st::gfx
{

class TextureHandle : private st::noncopyable_nonmovable
{
	friend class TextureCache;

public:

	enum State
	{
		Unloaded,
		Loading,
		Ready
	};

	nvrhi::TextureHandle texture;
	std::string id;
	State state = State::Unloaded;

private:

	TextureHandle() = default;
};

} // namespace st::gfx