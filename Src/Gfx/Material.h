#pragma once

#include <string>
#include <memory>

namespace st::gfx
{
	class TextureHandle;
};

namespace st::gfx
{

class Material
{
public:

	Material(const std::string& name = {}, const std::string& filename = {});
	~Material();

	void SetDiffuseTexture(std::shared_ptr<TextureHandle> textureHandle);

private:

	std::shared_ptr<TextureHandle> m_DiffuseTexture;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name
};

}