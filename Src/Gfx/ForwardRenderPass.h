#pragma once

#include "Core/Memory.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/Shader.h"
#include "RenderAPI/PipelineState.h"
#include "RenderAPI/Buffer.h"
#include "Gfx/RenderPass.h"

namespace st::gfx
{
	class SceneGraph;
};

namespace st::gfx
{

class ForwardRenderPass : public RenderPass
{
public:

	ForwardRenderPass() = default;

	void SetSceneGraph(st::weak<SceneGraph> sceneGraph) { m_SceneGraph = sceneGraph; }

	bool Render() override;
	void OnBackbufferResize(const glm::ivec2& size) override {};

private:

	void OnAttached() override;
	void OnDetached() override;

	rapi::DescriptorIndex GetCameraCB();

private:

	st::weak<SceneGraph> m_SceneGraph;

	st::rapi::ShaderHandle m_VS;
	st::rapi::ShaderHandle m_PS;
	st::rapi::GraphicsPipelineStateHandle m_PSO;

	st::rapi::BufferHandle m_CameraCB;
	st::rapi::BufferHandle m_TransformCB;
};

} // namespace st::gfx