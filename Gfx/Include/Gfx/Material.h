#pragma once

#include "Gfx/LoadedTexture.h"
#include "Gfx/MaterialDomain.h"
#include "RHI/RasterizerState.h"

namespace alm::gfx
{
	class LoadedTexture;
};

namespace alm::gfx
{

struct Material
{
public:

	Material(const std::string& name, const std::string& filename = {});
	~Material() = default;

	bool operator==(const Material& other) const;

public:

	std::shared_ptr<alm::gfx::LoadedTexture> BaseColorTexture;
	std::shared_ptr<alm::gfx::LoadedTexture> MetalRoughTexture;
	std::shared_ptr<alm::gfx::LoadedTexture> NormalTexture;
	std::shared_ptr<alm::gfx::LoadedTexture> EmissiveTexture;
	std::shared_ptr<alm::gfx::LoadedTexture> OcclusionTexture;
	std::shared_ptr<alm::gfx::LoadedTexture> TransmissionTexture;

	float3 BaseColor = float3{ 1.f };
	float Opacity = 1.f;
	float3 EmissiveColor = float3{ 0.f };
	float MetallicFactor = 0.f;
	float RoughnessFactor = 0.5f;
	float2 NormalTextureScale = float2{ 1.f };
	float OcclusionStrengh = 1.f;
	float AlphaCutoff = 0.f;
	float TransmissionFactor = 0.f;

	rhi::CullMode CullMode = rhi::CullMode::Back;
	MaterialDomain Domain = MaterialDomain::Opaque;

	std::string Name;
	std::string SourceFileName; // where this material originated from, e.g. GLTF file name
};

}