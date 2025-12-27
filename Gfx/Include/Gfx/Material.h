#pragma once

#include <string>
#include "RHI/Texture.h"

namespace st::gfx
{
	class LoadedTexture;
};

namespace st::gfx
{

class Material
{
public:

	Material(rhi::Device* device, const char* name = nullptr, const char* filename = nullptr);
	~Material();

	rhi::TextureHandle SetDiffuseTexture(rhi::TextureHandle textureHandle);
	rhi::TextureHandle GetDiffuseTexture() const { return m_DiffuseTexture; }

private:

	rhi::TextureHandle m_DiffuseTexture;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name

	rhi::Device* m_Device;
};

}