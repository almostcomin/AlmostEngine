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

	Material(const std::string& name = {}, const std::string& filename = {});
	~Material();

	void SetDiffuseTexture(rapi::TextureHandle textureHandle);

private:

	rapi::TextureHandle m_DiffuseTexture;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name
};

}