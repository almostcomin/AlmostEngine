#pragma once

#include "Gfx/IDrawStrategy.h"

namespace alm::gfx
{

class BackToFrontDrawStrategy : public IDrawStrategy
{
public:

	void Prepare(const RenderSet& drawInfos) override;
	DrawBucket GetNextBucket() override;
	void Clear() override;

private:

	struct OrderEntry
	{
		uint32_t Slot;
		const RenderableDrawInfo* DrawInfo;
	};

private:

	std::vector<OrderEntry> m_Order;
	size_t m_Pos = 0;
};

} // namespace alm::gfx