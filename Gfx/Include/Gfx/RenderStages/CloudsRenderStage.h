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

    void SetCloudsShapeTexture(rhi::TextureOwner&& texture) { m_CloudsShapeTexture = std::move(texture); }
    rhi::TextureHandle GetCloudsShapeTexture() const { return m_CloudsShapeTexture.get_weak(); }

    void SetCloudsDetailTexture(rhi::TextureOwner&& texture) { m_CloudsDetailTexture = std::move(texture); }
    rhi::TextureHandle GetCloudsDetailTexture() const { return m_CloudsDetailTexture.get_weak(); }

    static std::expected<std::pair<rhi::TextureOwner, alm::SignalListener>, std::string>
    CreateCloudsShapeTexture(DeviceManager* deviceManager);
    
    static std::expected<std::pair<rhi::TextureOwner, alm::SignalListener>, std::string>
    CreateCloudsDetailTexture(DeviceManager* deviceManager);

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

    CloudsSimParams m_Params;
    float2 m_CloudsOffset = { 0.f, 0.f };

    int m_RenderTargetDenom = 2;
    DebugChannel m_DebugChannel;
};

} // namespace alm::gfx