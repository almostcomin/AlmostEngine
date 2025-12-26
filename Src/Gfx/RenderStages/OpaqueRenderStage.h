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

	bool Render() override;
	void OnBackbufferResize(const glm::ivec2& size) override {};

	const char* GetDebugName() const override { return "OpaqueRenderStage"; }

private:

	void OnAttached() override;
	void OnDetached() override;

private:

	st::rhi::ShaderHandle m_VS;
	st::rhi::ShaderHandle m_PS;
	st::rhi::GraphicsPipelineStateHandle m_PSO;

	st::rhi::TextureHandle m_RenderTarget;
	st::rhi::TextureHandle m_DepthStencil;
	st::rhi::FramebufferHandle m_FB;
};

} // namespace st::gfx