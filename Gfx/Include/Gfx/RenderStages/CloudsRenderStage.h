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

    static constexpr float kEarthRefRadius = 6360000.f;
    static constexpr float kCloudsLayerHStart = 1500.f;
    static constexpr float kCloudsLayerHEnd = 12000.f;
    static constexpr float kCloudsFadeDistance = 48000.f;

	struct CloudsParams
	{
        float2 WindVelocity = { 0.002f, 0.001f };
        float StratusWeight = 0.3f;
        float CumulusWeight = 0.7f;
        float CumulonimbusWeight = 0.f;
        float CloudsScale = 0.004f;
        float CloudsCoverage = 0.35f;
        float AbsorptionCoeff = 0.8f / 1000;   // 1/m
        float ScatteringCoeff = 3.f / 1000;   // 1/m        
        float MultiScatterContribution = 0.1f;
        float MultiScatterOcclusion = 0.5f;
        float MultiScatterEccentricity = 0.5f;
        float PhaseGForward = 0.7f;
        float PhaseGBackward = -0.3f;
        float MultiScatterBaseG = 0.8f;
        float PowderStrength = 0.7f;
        float PowderEdgeWidth = 0.1f;
        float3 AmbientTop = { 0.6, 0.7, 0.9 };
        float3 AmbientBottom = { 0.3, 0.25, 0.2 };
        float AmbientStrength = 0.f;
        float CloudsLayerMin = kCloudsLayerHStart;
        float CloudsLayerMax = kCloudsLayerHEnd;
        float CloudsFadeDistance = kCloudsFadeDistance;
        float EarthRadius = kEarthRefRadius;
        float3 EarthCenter = float3{ 0.f };
        float DetailScale = 80.f;
        float DetailErosionStrength = 0.2f;
        bool VolumetricShadows = true;
        uint32_t CloudRaymarchIterations = 128;
        uint32_t LightRaymarchIterations = 16;
        uint32_t MultiScatterOctaves = 2;
	};

public:

    CloudsRenderStage();
    ~CloudsRenderStage() = default;

    const CloudsParams& GetCloudsParams() const { return m_Params; }
    void SetCloudsParams(const CloudsParams& params) { m_Params = params; }

    void SetEarthCenter(const float3& c) { m_Params.EarthCenter = c; }
    void SetEarthRadius(float r, bool keepRelativeScale);

    void SetCloudsShapeTexture(rhi::TextureOwner&& texture) { m_CloudsShapeTexture = std::move(texture); }
    rhi::TextureHandle GetCloudsShapeTexture() const { return m_CloudsShapeTexture.get_weak(); }

    void SetCloudsDetailTexture(rhi::TextureOwner&& texture) { m_CloudsDetailTexture = std::move(texture); }
    rhi::TextureHandle GetCloudsDetailTexture() const { return m_CloudsDetailTexture.get_weak(); }

    static std::expected<std::pair<rhi::TextureOwner, alm::SignalListener>, std::string>
    CreateCloudsShapeTexture(DeviceManager* deviceManager);
    
    static std::expected<std::pair<rhi::TextureOwner, alm::SignalListener>, std::string>
    CreateCloudsDetailTexture(DeviceManager* deviceManager);

    std::expected<std::pair<alm::rhi::TextureOwner, alm::SignalListener>, std::string>
    ComputeMultiScatterLUT(float mu_s, float mu_a);

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
    rhi::ResourceState m_CloudsTextureState[2];
    rhi::FramebufferOwner m_CloudsFB[2];
    int m_CloudsTextureIdx;

    rhi::ShaderOwner m_CloudsPS;
    rhi::GraphicsPipelineStateOwner m_CloudsPSO;

    rhi::GraphicsPipelineStateOwner m_CompositePSO;

    rhi::TextureOwner m_CloudsShapeTexture;
    rhi::TextureOwner m_CloudsDetailTexture;
    gfx::MultiBuffer m_CloudsCB;

    CloudsParams m_Params;
    float2 m_CloudsOffset = { 0.f, 0.f };
    float m_ScaleFactor = 1.f;

    int m_RenderTargetDenom = 2;
    DebugChannel m_DebugChannel;
};

} // namespace alm::gfx