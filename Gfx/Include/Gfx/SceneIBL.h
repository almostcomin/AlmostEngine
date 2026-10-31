#pragma once

#include "RHI/ShaderViews.h"
#include <array>
#include <expected>
#include <string>

namespace alm::gfx
{

class LoadedTexture;
class DeviceManager;


class SceneIBL
{
public:

	SceneIBL(DeviceManager* deviceManager);

	// Bakes an equirectangular .hdr environment (synchronous: bakes on CPU, uploads and waits).
	// Returns an std::expected<void, std::string>: error message on failure.
	std::expected<void, std::string> SetPanorama(const std::string& hdrFilePath);
	const std::string& GetPanoramaPath() const { return m_PanoramaPath; }

	void SetEnabled(bool enabled) { m_Enabled = enabled; }
	bool IsEnabled() const { return m_Enabled; }

	bool IsValid() const;

	std::shared_ptr<LoadedTexture> GetLambertTexture() { return m_LambertIrradiance; }
	rhi::TextureSampledView GetLambertSampledView() const;

	std::shared_ptr<LoadedTexture> GetGGXTexture() { return m_GGXPrefiltered; }
	rhi::TextureSampledView GetGGXSampledView() const;
	uint32_t GetGGXTextureMipCount() const;

	// Skybox cubemap (full mip chain) baked from the same environment
	std::shared_ptr<LoadedTexture> GetSkyTexture() { return m_Sky; }
	rhi::TextureSampledView GetSkySampledView() const;

	// DFG LUT (R,G = GGX split-sum; B = Charlie/sheen visibility)
	std::shared_ptr<LoadedTexture> GetDfgTexture() { return m_DfgLut; }
	rhi::TextureSampledView GetDfgSampledView() const;

	// Raw windowed irradiance SH (3 bands). Use IBLBaker::PreprocessSHForShader for shader-ready coefficients.
	const std::array<float3, 9>& GetSH() const { return m_Sh; }

private:

	DeviceManager* m_DeviceManager = nullptr;

	std::shared_ptr<LoadedTexture> m_Sky;
	std::shared_ptr<LoadedTexture> m_DfgLut;
	std::array<float3, 9> m_Sh{};

	std::shared_ptr<LoadedTexture> m_LambertIrradiance;
	std::shared_ptr<LoadedTexture> m_GGXPrefiltered;
	bool m_Enabled = false;

	std::string m_PanoramaPath;
};

} // namespace alm::gfx
