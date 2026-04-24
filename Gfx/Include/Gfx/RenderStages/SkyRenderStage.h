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
        float3 ZenitColor{ 0.22f, 0.55f,  0.935f };         // Base color at zenith
        float ZenitFalloff{ 0.5f };                         // How fast zenith fades to horizon
        float3 HorizonColor{ 0.595f, 0.6375f, 0.7225f };    // Color at horizon line
        float HorizonFalloff{ 6.f };                        // Power curve for horizon blend (higher = sharper line)
        float3 GroundColor{ 0.f, 0.1f, 0.2f };              // Darkening below horizon
        float GroundFalloff{ 10.f };                        // How fast darkening kicks in below horizon   

        // Intensities
        float SunWideIntensity{ 0.25f };
        float SunMediumIntensity{ 0.25f };
        float SunTightIntensity{ 0.2f };
        float SunSoftGlowIntensity{ 0.2f };
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