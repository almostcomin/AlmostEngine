#pragma once

#include "Gfx/RenderPass.h"
#include "Core/Memory.h"

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

	bool Render(nvrhi::IFramebuffer* frameBuffer) override;
	void OnBackbufferResize(const glm::ivec2& size) override {};

private:

	st::weak<SceneGraph> m_SceneGraph;
};

} // namespace st::gfx