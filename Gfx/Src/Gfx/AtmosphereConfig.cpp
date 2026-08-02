#include "Gfx/GfxPCH.h"
#include "Gfx/AtmosphereConfig.h"

float3 alm::gfx::AtmosphereConfig::GetSunDirection() const
{
	const float3 sunDir = alm::ElevationAzimuthRadToDir(
		glm::radians(Sun.ElevationDeg), glm::radians(Sun.AzimuthDeg));

	return sunDir;
}