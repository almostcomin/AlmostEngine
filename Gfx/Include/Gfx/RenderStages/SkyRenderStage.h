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

	struct SkyParams
	{
        float3 skyColor{ 0.17f, 0.37f, 0.65f };
        float3 horizonColor{ 0.50f, 0.70f, 0.92f };
        float3 groundColor{ 0.62f, 0.59f, 0.55f };
        float3 directionUp{ 0.f, 1.f, 0.f };
        float brightness = 0.1f;        // scaler for sky brightness
        float horizonSize = 5.f;        // +/- degrees
        float glowSize = 5.f;           // degrees, starting from the edge of the light disk
        float glowIntensity = 0.1f;     // [0-1] relative to light intensity
        float glowSharpness = 4.f;      // [1-10] is the glow power exponent
        float maxLightRadiance = 100.f; // clamp for light radiance derived from its angular size, 0 = no clamp
	};

public:

    const SkyParams& GetSkyParams() const { return m_Params; }
    void SetSkyParams(const SkyParams& params) { m_Params = params; }

private:

	void Setup(RenderGraphBuilder& builder);
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_SceneDepthTexture;
    RGFramebufferHandle m_FB;

    rhi::ShaderOwner m_PS;
    rhi::GraphicsPipelineStateOwner m_PSO;

    gfx::MultiBuffer m_ShaderCB;

    SkyParams m_Params;
};

} // namespace alm::gfx