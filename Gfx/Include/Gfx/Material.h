#pragma once

#include "Gfx/LoadedTexture.h"

namespace st::rhi
{
	class Device;
}

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

	void SetDiffuseTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle);
	const std::shared_ptr<st::gfx::LoadedTexture> GetDiffuseTexture() const;
	rhi::TextureHandle GetDiffuseTextureHandle() const;


	const std::string& GetName() const { return m_Name; }

private:

	std::shared_ptr<st::gfx::LoadedTexture> m_DiffuseTexture;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name

	rhi::Device* m_Device;
};

}