#pragma once

#include "Core/Memory.h"
#include "RHI/CommandList.h"
#include "RHI/Shader.h"
#include "RHI/PipelineState.h"
#include "RHI/Buffer.h"
#include "RHI/Framebuffer.h"
#include "Gfx/RenderStage.h"

namespace st::gfx
{
	class Scene;
};

namespace st::gfx
{

class OpaqueRenderStage : public RenderStage
{
public:

	OpaqueRenderStage() = default;

	const char* GetDebugName() const override { return "OpaqueRenderStage"; }

private:

	bool Render() override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

private:

	st::rhi::ShaderOwner m_VS;
	st::rhi::ShaderOwner m_PS;
	st::rhi::GraphicsPipelineStateDesc m_PSODesc;
	st::rhi::GraphicsPipelineStateOwner m_PSO;

	st::rhi::TextureHandle m_RenderTarget;
	st::rhi::TextureHandle m_DepthStencil;
	st::rhi::FramebufferOwner m_FB;
};

} // namespace st::gfx