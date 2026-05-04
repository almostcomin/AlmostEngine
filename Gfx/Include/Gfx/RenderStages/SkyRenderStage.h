#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/MultiBuffer.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class SkyRenderStage : public RenderStage
{
    REGISTER_RENDER_STAGE(SkyRenderStage)

public:

    static constexpr float kEarthRefRadius = 6360000.f;
    static constexpr float kAtmosRefHeight = 60000.f;
    static constexpr float3 kRefRayleighScattering = { 5.8e-6f, 13.5e-6f, 33.1e-6f };
    static constexpr float kRefRayleighScaleHeight = 8000.f;
    static constexpr float3 kRefMieScattering = { 21e-6f, 21e-6f, 21e-6f };
    static constexpr float kRefMieScaleHeight = 1200.f;

	struct SkyParams
	{
        float3 EarthCenter = { 0.f, 0.f, 0.f };
        float EarthRadius = kEarthRefRadius;
        float AtmosHeight = kAtmosRefHeight;
        float MieAnisotropy = 0.76f;
        uint32_t NumSteps = 64;
        uint32_t NumLightSteps = 8;
	};

public:

    const SkyParams& GetSkyParams() const { return m_Params; }
    void SetSkyParams(const SkyParams& params) { m_Params = params; }

    void SetEarthCenter(const float3& pos);
    void SetEarthRadius(float radius, float atmosRelScale = 1.f);

private:

	void Setup(RenderGraphBuilder& builder);
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_SceneDepthTexture;
    RGTextureHandle m_LinearDepthTexture;
    RGFramebufferHandle m_FB;

    rhi::ShaderOwner m_PS;
    rhi::GraphicsPipelineStateOwner m_PSO;

    gfx::MultiBuffer m_ShaderCB;

    SkyParams m_Params;
};

} // namespace alm::gfx