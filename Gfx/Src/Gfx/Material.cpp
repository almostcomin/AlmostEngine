#include "Gfx/Material.h"
#include "RHI/Device.h"

st::gfx::Material::Material(st::rhi::Device* device, const char* name, const char* filename) :
	m_Device{ device },
	m_Name{ name ? name : "<null>" },
	m_SourceFileName{ filename ? filename : "<null>" }
{}

st::gfx::Material::~Material()
{
}

void st::gfx::Material::SetDiffuseTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle)
{
	m_DiffuseTexture = textureHandle;
}

st::rhi::TextureHandle st::gfx::Material::GetDiffuseTexture() const 
{
	return m_DiffuseTexture ? m_DiffuseTexture->texture.get_weak() : nullptr;
}