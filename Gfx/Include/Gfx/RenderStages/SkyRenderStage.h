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
        float2 Offset = { 0.f, 0.f };
	};

public:

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

    void CreateNoiseTextures();

private:

    RGTextureHandle m_SceneColorTexture;
    RGTextureHandle m_SceneDepthTexture;
    RGFramebufferHandle m_FB;

    rhi::ShaderOwner m_PS;
    rhi::GraphicsPipelineStateOwner m_PSO;

    rhi::TextureOwner m_CloudsShapeTexture;

    SkyParams m_Params;
};

} // namespace alm::gfx