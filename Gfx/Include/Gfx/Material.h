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

	void SetBaseColorTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle);
	void SetMetalRoughTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle);
	void SetNormalTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle);

	void SetBaseColor(const float3& color) { m_BaseColor = color; }
	void SetMetallicFactor(float factor) { m_MetallicFactor = factor; }
	void SetRoughnessFactor(float factor) { m_RoughnessFactor = factor; }
	void SetOpacity(float opacity) { m_Opacity = opacity; }
	void SetNormalScale(const float2 scale) { m_NormalScale = scale; }

	const std::shared_ptr<st::gfx::LoadedTexture> GetBaseColorTexture() const;

	rhi::TextureHandle GetBaseColorTextureHandle() const;
	rhi::TextureHandle GetMetalRoughTextureHandle() const;
	rhi::TextureHandle GetNormalTextureHandle() const;

	const float3& GetBaseColor() const { return m_BaseColor; }
	float GetOpacity() const { return m_Opacity; }
	float GetMetallicFactor() const { return m_MetallicFactor; }
	float GetRoughnessFactor() const { return m_RoughnessFactor; };
	const float2& GetNormalScale() const { return m_NormalScale; }

	const std::string& GetName() const { return m_Name; }

private:

	std::shared_ptr<st::gfx::LoadedTexture> m_BaseColorTexture;
	std::shared_ptr<st::gfx::LoadedTexture> m_MetalRoughTexture;
	std::shared_ptr<st::gfx::LoadedTexture> m_NormalTexture;

	float3 m_BaseColor; // Used only if BaseColorTexture or m_MetalRoughTexture is not present
	float m_Opacity;	// Final opacity is multiplied by BaseColorTexture.a if present
	float m_MetallicFactor;
	float m_RoughnessFactor;
	float2 m_NormalScale;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name

	rhi::Device* m_Device;
};

}