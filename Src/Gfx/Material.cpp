#include "Gfx/Material.h"
#include "RenderAPI/Device.h"

st::gfx::Material::Material(st::rapi::Device* device, const char* name, const char* filename) :
	m_Device{ device },
	m_Name{ name ? name : "<null>" },
	m_SourceFileName{ filename ? filename : "<null>" }
{}

st::gfx::Material::~Material()
{
	m_Device->ReleaseQueued(m_DiffuseTexture);
}

st::rapi::TextureHandle st::gfx::Material::SetDiffuseTexture(rapi::TextureHandle textureHandle)
{
	auto ret = m_DiffuseTexture;
	m_DiffuseTexture = textureHandle;
	return ret;
}