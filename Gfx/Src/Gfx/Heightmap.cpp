#include "Gfx/GfxPCH.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapSource.h"

alm::gfx::Heightmap::Heightmap(std::shared_ptr<IHeightmapSource> source, const float2& uvScale,	const float2& uvOffset, float heightScale, float heightOffset)
	: m_Source{ source }, m_uvScale{ uvScale }, m_uvOffset{ uvOffset }, m_HeightScale{ heightScale }, m_HeightOffset{ heightOffset }
{
	ComputeBounds();
}

alm::gfx::Heightmap::~Heightmap()
{}

float alm::gfx::Heightmap::Sample(const float2& uv) const
{
	const float2 sourceUV = uv * m_uvScale + m_uvOffset;
	return m_Source->Sample(sourceUV) * m_HeightScale + m_HeightOffset;
}

void alm::gfx::Heightmap::ComputeBounds()
{
	const float2 heightRange = m_Source->GetHeightRange();
	const float2 normSize = m_Source->GetNormalizedSize();

	// Actually if m_Source->IsTileable bounds in x/y are infinite?
	m_Bounds.min = float3{ 0.f, 0.f, heightRange.x * m_HeightScale + m_HeightOffset };
	m_Bounds.max = float3{ normSize.x, normSize.y, heightRange.x * m_HeightScale + m_HeightOffset };
}