#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/MultiBuffer.h"
#include "Gfx/RenderStageFactory.h"

namespace alm::gfx
{

class AtmosSkyRenderStage : public RenderStage
{
    REGISTER_RENDER_STAGE(AtmosSkyRenderStage)

public:

    static constexpr float kAtmosRefHeight = 60000.f;

    // Planet Earth defaults
    static constexpr float3 kRefRayleighWaveLengths = { 680, 530.f, 440.f };
    static constexpr float kRefRayleighScaleHeight = 8000.f;
    static constexpr float kMieBase = 21e-6f;
    static constexpr float kRefMieScaleHeight = 1200.f;

	struct SimParams
	{
        uint32_t NumSteps = 32;
        uint32_t NumLightSteps = 3;
	};

public:

    const SimParams& GetSimParams() const { return m_Params; }
    void SetSimParams(const SimParams& params) { m_Params = params; }

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

    SimParams m_Params;
};

} // namespace alm::gfx