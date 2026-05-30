#include "Gfx/GfxPCH.h"
#include "Gfx/NoiseHeightmapSource.h"
#include "Core/Noise.h"

float alm::gfx::NoiseHeightmapSource::GetHeight(const uint2& p) const
{
	assert(0 && "Calling GetHeight in a NoiseHeightmapSource");
	return 0.f; 
}

float alm::gfx::NoiseHeightmapSource::Sample(const float2& uv) const
{
	return PerlinFbm(float3{ uv.x + m_Params.OffsetX, uv.y + m_Params.OffsetY, 0.f }, m_Params.Frequency, m_Params.Octaves);
}