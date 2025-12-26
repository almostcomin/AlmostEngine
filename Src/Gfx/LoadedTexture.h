#pragma once

#include "Core/Common.h"

namespace st::gfx
{

class LoadedTexture : private st::noncopyable_nonmovable
{
	friend class TextureCache;

public:

	enum State
	{
		Unloaded,
		Loading,
		Ready
	};

	rapi::TextureHandle texture;
	std::string id;
	State state = State::Unloaded;

private:

	LoadedTexture() = default;
};

} // namespace st::gfx