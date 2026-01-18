#include "Gfx/Material.h"
#include "RHI/Device.h"

st::gfx::Material::Material(st::rhi::Device* device, const char* name, const char* filename) :
	m_BaseColor{ 0.f, 0.f, 0.f },
	m_MetallicFactor{ 0.f },
	m_RoughnessFactor{ 0.f },
	m_NormalScale{ 1.f },
	m_Device{ device },
	m_Name{ name ? name : "<null>" },
	m_SourceFileName{ filename ? filename : "<null>" }
{}

st::gfx::Material::~Material()
{
}

void st::gfx::Material::SetBaseColorTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle)
{
	m_BaseColorTexture = textureHandle;
}

void st::gfx::Material::SetMetalRoughTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle)
{
	m_MetalRoughTexture = textureHandle;
}

void st::gfx::Material::SetNormalTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle)
{
	m_NormalTexture = textureHandle;
}

const std::shared_ptr<st::gfx::LoadedTexture> st::gfx::Material::GetBaseColorTexture() const
{
	return m_BaseColorTexture;
}

st::rhi::TextureHandle st::gfx::Material::GetBaseColorTextureHandle() const
{
	return m_BaseColorTexture ? m_BaseColorTexture->texture.get_weak() : nullptr;
}

st::rhi::TextureHandle st::gfx::Material::GetMetalRoughTextureHandle() const
{
	return m_MetalRoughTexture ? m_MetalRoughTexture->texture.get_weak() : nullptr;
}

st::rhi::TextureHandle st::gfx::Material::GetNormalTextureHandle() const
{
	return m_NormalTexture ? m_NormalTexture->texture.get_weak() : nullptr;
}