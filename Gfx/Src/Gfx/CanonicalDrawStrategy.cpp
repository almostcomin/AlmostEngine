#include "Gfx/GfxPCH.h"
#include "Gfx/CanonicalDrawStrategy.h"
#include "Gfx/RenderSet.h"

void alm::gfx::CanonicalDrawStrategy::Prepare(const RenderSet& drawInfos)
{
	m_RenderSet = &drawInfos;
	m_Next = m_RenderSet->begin();
}

alm::gfx::CanonicalDrawStrategy::DrawBucket alm::gfx::CanonicalDrawStrategy::GetNextBucket()
{
	if (!m_RenderSet || m_Next == m_RenderSet->end())
		return {};

	auto first = m_Next;
	auto cursor = first + 1;
	while (cursor != m_RenderSet->end())
	{
		if (cursor->MaterialDomain != first->MaterialDomain ||
			cursor->CullMode != first->CullMode ||
			cursor->BatchKey != first->BatchKey)
		{
			break;
		}
		++cursor;
	}
	m_Next = cursor;

	// In canonical order, as defined in the VisibilityBuffer, slots are consecutive.
	return DrawBucket{
		.FirstSlot = static_cast<uint32_t>(first - m_RenderSet->begin()),
		.DrawInfos = { first, static_cast<std::size_t>(cursor - first) }};
}

void alm::gfx::CanonicalDrawStrategy::Clear()
{
	m_RenderSet = nullptr;
}