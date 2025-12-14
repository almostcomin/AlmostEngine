#include "Gfx/Material.h"

st::gfx::Material::Material(const std::string& name, const std::string& filename) :
	m_Name{ name },
	m_SourceFileName{ filename }
{}

st::gfx::Material::~Material()
{
	//assert(!m_DiffuseTexture);
}

st::rapi::TextureHandle st::gfx::Material::SetDiffuseTexture(rapi::TextureHandle textureHandle)
{
	auto ret = m_DiffuseTexture;
	m_DiffuseTexture = textureHandle;
	return ret;
}