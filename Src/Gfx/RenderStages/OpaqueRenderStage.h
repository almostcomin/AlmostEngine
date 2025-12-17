#pragma once

#include "Core/Memory.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/Shader.h"
#include "RenderAPI/PipelineState.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Framebuffer.h"
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

	void SetScene(st::weak<Scene> scene) { m_Scene = scene; }

	bool Render() override;
	void OnBackbufferResize(const glm::ivec2& size) override {};

	const char* GetDebugName() const override { return "OpaqueRenderStage"; }

private:

	void OnAttached() override;
	void OnDetached() override;

	rapi::DescriptorIndex GetSceneDI();

private:

	st::weak<Scene> m_Scene;

	st::rapi::ShaderHandle m_VS;
	st::rapi::ShaderHandle m_PS;
	st::rapi::GraphicsPipelineStateHandle m_PSO;

	st::rapi::BufferHandle m_SceneCB;

	st::rapi::TextureHandle m_RenderTarget;
	st::rapi::TextureHandle m_DepthStencil;
	st::rapi::FramebufferHandle m_FB;
};

} // namespace st::gfx