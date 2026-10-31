#pragma once

#include "RHI/ShaderViews.h"

namespace alm::gfx
{

class LoadedTexture;


class SceneIBL
{
public:

    void SetEnvironment(std::shared_ptr<LoadedTexture> lambert, std::shared_ptr<LoadedTexture> ggx);

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    bool IsValid() const;

    std::shared_ptr<LoadedTexture> GetLambertTexture() { return m_LambertIrradiance; }
    rhi::TextureSampledView GetLambertSampledView() const;

    std::shared_ptr<LoadedTexture> GetGGXTexture() { return m_GGXPrefiltered; }
    rhi::TextureSampledView GetGGXSampledView() const;
    uint32_t GetGGXTextureMipCount() const;

private:

    std::shared_ptr<LoadedTexture> m_LambertIrradiance;
    std::shared_ptr<LoadedTexture> m_GGXPrefiltered;
    bool m_Enabled = false;
};

} // namespace alm::gfx