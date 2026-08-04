#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/MultiBuffer.h"
#include "Gfx/RenderStageFactory.h"
#include "Core/Signal.h"

namespace alm::gfx
{

class CloudsRenderStage : public RenderStage
{
    REGISTER_RENDER_STAGE(CloudsRenderStage)

public:

	enum class DebugChannel
	{
		Disabled,
        Clouds_Transmitance
	};

public:

	struct CloudsSimParams
	{
        bool VolumetricShadows = true;
        uint32_t CloudRaymarchIterations = 128;
        uint32_t LightRaymarchIterations = 16;
        uint32_t MultiScatterOctaves = 2;
	};

public:

    CloudsRenderStage();
    ~CloudsRenderStage() = default;

    const CloudsSimParams& GetCloudsSimParams() const { return m_Params; }
    void SetCloudsSimParams(const CloudsSimParams& params) { m_Params = params; }

    void SetRenderTargetDenominator(int v);
    int GetRenderTargetDenominator() const { return m_RenderTargetDenom; }

    void SetDebugChannel(DebugChannel c) { m_DebugChannel = c; }

private:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
    void OnBackbufferResize() override;

    void ResetCloudsResources();

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_LinearDepthTexture;
    RGFramebufferHandle m_CompositeFB;

    rhi::TextureOwner m_CloudsTexture[2];
    uint2 m_CloudsTextureDims;
    int m_CloudsTextureIdx;

    rhi::ShaderOwner m_CloudsCS;
    rhi::ComputePipelineStateOwner m_CloudsPSO;

    rhi::GraphicsPipelineStateOwner m_CompositePSO;

    gfx::MultiBuffer m_CloudsCB;

    CloudsSimParams m_Params;

    int m_RenderTargetDenom = 2;
    DebugChannel m_DebugChannel;
};

} // namespace alm::gfx