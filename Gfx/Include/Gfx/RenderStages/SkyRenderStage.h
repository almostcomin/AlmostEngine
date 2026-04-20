#pragma once

#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/MultiBuffer.h"
#include "Gfx/RenderStageFactory.h"
#include "Core/Signal.h"

namespace alm::gfx
{

class SkyRenderStage : public RenderStage
{
    REGISTER_RENDER_STAGE(SkyRenderStage)

public:

	struct SkyParams
	{
        float2 WindVelocity = { 0.001f, 0.0005f };
        float CloudsScale = 0.0001f;
        float CloudsCoverage = 0.3f;
        float AbsorptionCoeff = 0.1f;
        float CloudsLayerMin = 1350.f;// 2000.f;
        float CloudsLayerMax = 2350.f;// 7000.f;
        float CloudsFadeDistance = 50000.f;
        float EarthRadius = 1500000.f; // 6360000.0.f;
        uint32_t CloudRaymarchIterations = 16;
        uint32_t LightRaymarchIterations = 8;
	};

public:

    SkyRenderStage();
    ~SkyRenderStage() = default;

    const SkyParams& GetSkyParams() const { return m_Params; }
    void SetSkyParams(const SkyParams& params) { m_Params = params; }

    void SetCloudsShapeTexture(rhi::TextureOwner&& texture) { m_CloudsShapeTexture = std::move(texture); }
    rhi::TextureHandle GetCloudsShapeTexture() const { return m_CloudsShapeTexture.get_weak(); }

    static std::expected<std::pair<rhi::TextureOwner, alm::SignalListener>, std::string>
    CreateCloudsShapeTexture(DeviceManager* deviceManager);

private:

	void Setup(RenderGraphBuilder& builder);
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
    void OnBackbufferResize() override;

    void ResetSkyResources();

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_LinearDepthTexture;
    RGFramebufferHandle m_CompositeFB;

    rhi::TextureOwner m_SkyTexture[2];
    rhi::ResourceState m_SkyTextureState[2];
    rhi::FramebufferOwner m_SkyFB[2];
    int m_SkyTextureIdx;

    rhi::ShaderOwner m_SkyPS;
    rhi::GraphicsPipelineStateOwner m_SkyPSO;

    rhi::GraphicsPipelineStateOwner m_CompositePSO;

    rhi::TextureOwner m_CloudsShapeTexture;
    gfx::MultiBuffer m_CloudsCB;

    SkyParams m_Params;
    float2 m_CloudsOffset = { 0.f, 0.f };
};

} // namespace alm::gfx