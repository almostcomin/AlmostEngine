#include "Gfx/GfxPCH.h"
#include "Gfx/BackToFrontDrawStrategy.h"
#include "Gfx/RenderSet.h"

void alm::gfx::BackToFrontDrawStrategy::Prepare(const RenderSet& drawInfos)
{
	m_Order.clear();
	m_Pos = 0;

	for (size_t slot = 0; slot < drawInfos.size(); ++slot)
	{
		if (IsBackToFrontSorted(drawInfos[slot].MaterialDomain))
		{
			m_Order.emplace_back((uint32_t)slot, &drawInfos[slot]);
		}
	}

	// far -> near, ties keep canonic order
	std::ranges::stable_sort(m_Order, [](const OrderEntry& a, const OrderEntry& b)
	{
		return a.DrawInfo->ViewDepth > b.DrawInfo->ViewDepth;
	});
}

alm::gfx::BackToFrontDrawStrategy::DrawBucket alm::gfx::BackToFrontDrawStrategy::GetNextBucket()
{
	if (m_Pos >= m_Order.size())
		return {};

	const OrderEntry& first = m_Order[m_Pos];
	size_t runEnd = m_Pos + 1;
	while (true)
	{
		if (runEnd >= m_Order.size())
			break;
		if (m_Order[runEnd].DrawInfo->MaterialDomain != first.DrawInfo->MaterialDomain)
			break;
		if (m_Order[runEnd].DrawInfo->CullMode != first.DrawInfo->CullMode)
			break;
		if (m_Order[runEnd].DrawInfo->BatchKey != first.DrawInfo->BatchKey)
			break;
		if (m_Order[runEnd].Slot != m_Order[runEnd - 1].Slot + 1) // consecutive
			break;

		++runEnd;
	}

	DrawBucket bucket{
		.FirstSlot = first.Slot,
		.DrawInfos = { first.DrawInfo, runEnd - m_Pos }};
	m_Pos = runEnd;

	return bucket;
}

void alm::gfx::BackToFrontDrawStrategy::Clear()
{
	m_Order.clear();
	m_Pos = 0;
}