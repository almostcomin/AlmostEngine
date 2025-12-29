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

	rhi::TextureOwner texture;
	std::string id;
	State state = State::Unloaded;

private:

	LoadedTexture() = default;
	~LoadedTexture()
	{
		texture.reset();
	}
};

} // namespace st::gfx