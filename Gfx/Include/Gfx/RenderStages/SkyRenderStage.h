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

    static constexpr float kAtmosRefHeight = 60000.f;

    // Planet Earth defaults
    static constexpr float3 kRefRayleighWaveLengths = { 680, 530.f, 440.f };
    static constexpr float kRefRayleighScaleHeight = 8000.f;
    static constexpr float kMieBase = 21e-6f;
    static constexpr float kRefMieScaleHeight = 1200.f;

	struct SkySimParams
	{
        uint32_t NumSteps = 32;
        uint32_t NumLightSteps = 3;
	};

public:

    const SkySimParams& GetSkySimParams() const { return m_Params; }
    void SetSkySimParams(const SkySimParams& params) { m_Params = params; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_LinearDepthTexture;

    rhi::ShaderOwner m_CS;
    rhi::ComputePipelineStateOwner m_PSO;

    gfx::MultiBuffer m_ShaderCB;

    SkySimParams m_Params;
};

} // namespace alm::gfx