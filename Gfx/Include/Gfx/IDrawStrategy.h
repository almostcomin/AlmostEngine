#pragma once

#include "Gfx/RenderSet.h"

namespace alm::gfx
{

class IDrawStrategy
{
public:

	struct DrawBucket
	{
		uint32_t FirstSlot;
		std::span<const RenderableDrawInfo> DrawInfos;
	};

public:

	virtual ~IDrawStrategy() = default;

	virtual void Prepare(const RenderSet& renderSet) = 0;

	virtual DrawBucket GetNextBucket() = 0;
	virtual void Clear() = 0;
};

} // namespace alm::gfx