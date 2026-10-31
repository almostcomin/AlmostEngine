#include "Gfx/GfxPCH.h"
#include "Gfx/SceneIBL.h"
#include "Gfx/LoadedTexture.h"
#include "RHI/Texture.h"

void alm::gfx::SceneIBL::SetEnvironment(std::shared_ptr<LoadedTexture> lambert, std::shared_ptr<LoadedTexture> ggx)
{
	m_LambertIrradiance = lambert;
	m_GGXPrefiltered = ggx;
}

bool alm::gfx::SceneIBL::IsValid() const
{
	return m_Enabled
		&& m_LambertIrradiance && m_LambertIrradiance->IsValid()
		&& m_GGXPrefiltered && m_GGXPrefiltered->IsValid();
}

alm::rhi::TextureSampledView alm::gfx::SceneIBL::GetLambertSampledView() const
{
	return m_LambertIrradiance && m_LambertIrradiance->IsValid() ?
		m_LambertIrradiance->texture->GetSampledView() : alm::rhi::TextureSampledView{};
}

alm::rhi::TextureSampledView alm::gfx::SceneIBL::GetGGXSampledView() const
{
	return m_GGXPrefiltered && m_GGXPrefiltered->IsValid() ?
		m_GGXPrefiltered->texture->GetSampledView() : alm::rhi::TextureSampledView{};
}

uint32_t alm::gfx::SceneIBL::GetGGXTextureMipCount() const
{
	if (m_GGXPrefiltered && m_GGXPrefiltered->IsValid())
		return m_GGXPrefiltered->texture->GetDesc().mipLevels;
	
	return 0;
}