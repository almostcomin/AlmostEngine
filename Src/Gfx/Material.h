#pragma once

#include <string>
#include "RenderAPI/Texture.h"

namespace st::gfx
{
	class LoadedTexture;
};

namespace st::gfx
{

class Material
{
public:

	Material(rapi::Device* device, const char* name = nullptr, const char* filename = nullptr);
	~Material();

	rapi::TextureHandle SetDiffuseTexture(rapi::TextureHandle textureHandle);
	rapi::TextureHandle GetDiffuseTexture() const { return m_DiffuseTexture; }

private:

	rapi::TextureHandle m_DiffuseTexture;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name

	rapi::Device* m_Device;
};

}