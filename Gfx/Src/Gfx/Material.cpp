#include "Gfx/GfxPCH.h"
#include "Gfx/Material.h"
#include "RHI/Device.h"

alm::gfx::Material::Material(alm::rhi::Device* device, const std::string& name, const std::string& filename) :
	m_BaseColor{ 1.f },
	m_Opacity{ 1.f },
	m_EmissiveColor{ 0.f },
	m_MetallicFactor{ 0.f },
	m_RoughnessFactor{ 0.5f },
	m_NormalTextureScale{ 1.f },
	m_OcclusionStrengh{ 1.f },
	m_AlphaCutoff{ 0.f },
	m_CullMode{ rhi::CullMode::Back },
	m_Domain{ MaterialDomain::Opaque },
	m_Device{ device },
	m_Name{ name.empty() ? "<null>" : name },
	m_SourceFileName{ filename.empty() ? "<null>" : filename }
{}

alm::gfx::Material::~Material()
{}

void alm::gfx::Material::SetBaseColorTexture(std::shared_ptr<alm::gfx::LoadedTexture> textureHandle)
{
	m_BaseColorTexture = textureHandle;
}

void alm::gfx::Material::SetMetalRoughTexture(std::shared_ptr<alm::gfx::LoadedTexture> textureHandle)
{
	m_MetalRoughTexture = textureHandle;
}

void alm::gfx::Material::SetNormalTexture(std::shared_ptr<alm::gfx::LoadedTexture> textureHandle)
{
	m_NormalTexture = textureHandle;
}

void alm::gfx::Material::SetEmissiveTexture(std::shared_ptr<alm::gfx::LoadedTexture> textureHandle)
{
	m_EmissiveTexture = textureHandle;
}

void alm::gfx::Material::SetOcclusionTexture(std::shared_ptr<alm::gfx::LoadedTexture> textureHandle)
{
	m_OcclusionTexture = textureHandle;
}

alm::rhi::TextureHandle alm::gfx::Material::GetBaseColorTextureHandle() const
{
	return m_BaseColorTexture ? m_BaseColorTexture->texture.get_weak() : nullptr;
}

alm::rhi::TextureHandle alm::gfx::Material::GetMetalRoughTextureHandle() const
{
	return m_MetalRoughTexture ? m_MetalRoughTexture->texture.get_weak() : nullptr;
}

alm::rhi::TextureHandle alm::gfx::Material::GetNormalTextureHandle() const
{
	return m_NormalTexture ? m_NormalTexture->texture.get_weak() : nullptr;
}

alm::rhi::TextureHandle alm::gfx::Material::GetEmissiveTextureHandle() const
{
	return m_EmissiveTexture ? m_EmissiveTexture->texture.get_weak() : nullptr;
}

alm::rhi::TextureHandle alm::gfx::Material::GetOcclusionTextureHandle() const
{
	return m_OcclusionTexture ? m_OcclusionTexture->texture.get_weak() : nullptr;
}

bool alm::gfx::Material::operator==(const Material& other) const
{
	return
		m_BaseColorTexture == other.m_BaseColorTexture &&
		m_MetalRoughTexture == other.m_MetalRoughTexture &&
		m_NormalTexture == other.m_NormalTexture &&
		m_EmissiveTexture == other.m_EmissiveTexture &&
		m_OcclusionTexture == other.m_OcclusionTexture &&
		AlmostEqual(m_BaseColor, other.m_BaseColor) &&
		AlmostEqual(m_Opacity, other.m_Opacity) &&
		AlmostEqual(m_EmissiveColor, other.m_EmissiveColor) &&
		AlmostEqual(m_MetallicFactor, other.m_MetallicFactor) &&
		AlmostEqual(m_RoughnessFactor, other.m_RoughnessFactor) &&
		AlmostEqual(m_NormalTextureScale, other.m_NormalTextureScale) &&
		AlmostEqual(m_OcclusionStrengh, other.m_OcclusionStrengh) &&
		AlmostEqual(m_AlphaCutoff, other.m_AlphaCutoff) &&
		m_Domain == other.m_Domain;
}