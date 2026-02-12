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
	void SetEmissiveTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle);
	void SetOcclusionTexture(std::shared_ptr<st::gfx::LoadedTexture> textureHandle);

	void SetBaseColor(const float3& color) { m_BaseColor = color; }
	void SetOpacity(float opacity) { m_Opacity = opacity; }
	void SetEmissiveColor(const float3& color) { m_EmissiveColor = color; }
	void SetMetallicFactor(float factor) { m_MetallicFactor = factor; }
	void SetRoughnessFactor(float factor) { m_RoughnessFactor = factor; }
	void SetNormalTextureScale(const float2 scale) { m_NormalTextureScale = scale; }
	void SetOcclusionStrengh(const float v) { m_OcclusionStrengh = v; }

	const std::shared_ptr<st::gfx::LoadedTexture> GetBaseColorTexture() const { return m_BaseColorTexture; }
	const std::shared_ptr<st::gfx::LoadedTexture> GetMetalRoughTexture() const { return m_MetalRoughTexture; }
	const std::shared_ptr<st::gfx::LoadedTexture> GetNormalTexture() const { return m_NormalTexture; }
	const std::shared_ptr<st::gfx::LoadedTexture> GetEmissiveTexture() const { return m_EmissiveTexture; }
	const std::shared_ptr<st::gfx::LoadedTexture> GetOcclusionTexture() const { return m_OcclusionTexture; }

	rhi::TextureHandle GetBaseColorTextureHandle() const;
	rhi::TextureHandle GetMetalRoughTextureHandle() const;
	rhi::TextureHandle GetNormalTextureHandle() const;
	rhi::TextureHandle GetEmissiveTextureHandle() const;
	rhi::TextureHandle GetOcclusionTextureHandle() const;

	const float3& GetBaseColor() const { return m_BaseColor; }
	const float3& GetEmissiveColor() const { return m_EmissiveColor; }
	float GetOpacity() const { return m_Opacity; }
	float GetOcclusionStrengh() const { return m_OcclusionStrengh; }
	float GetMetallicFactor() const { return m_MetallicFactor; }
	float GetRoughnessFactor() const { return m_RoughnessFactor; };
	const float2& GetNormalTextureScale() const { return m_NormalTextureScale; }

	const std::string& GetName() const { return m_Name; }

private:

	std::shared_ptr<st::gfx::LoadedTexture> m_BaseColorTexture;
	std::shared_ptr<st::gfx::LoadedTexture> m_MetalRoughTexture;
	std::shared_ptr<st::gfx::LoadedTexture> m_NormalTexture;
	std::shared_ptr<st::gfx::LoadedTexture> m_EmissiveTexture;
	std::shared_ptr<st::gfx::LoadedTexture> m_OcclusionTexture;

	float3 m_BaseColor; // Used only if BaseColorTexture or m_MetalRoughTexture is not present
	float m_Opacity;	// Final opacity is multiplied by BaseColorTexture.a if present
	float3 m_EmissiveColor;
	float m_MetallicFactor;
	float m_RoughnessFactor;
	float2 m_NormalTextureScale;
	float m_OcclusionStrengh;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name

	rhi::Device* m_Device;
};

}