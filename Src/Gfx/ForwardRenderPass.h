#pragma once

#include "Gfx/RenderPass.h"
#include "Core/Memory.h"
#include <nvrhi/nvrhi.h>

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

	bool Init(nvrhi::DeviceHandle device);

	void SetSceneGraph(st::weak<SceneGraph> sceneGraph) { m_SceneGraph = sceneGraph; }

	bool Render(nvrhi::IFramebuffer* frameBuffer) override;
	void OnBackbufferResize(const glm::ivec2& size) override {};


private:

	st::weak<SceneGraph> m_SceneGraph;

	nvrhi::CommandListHandle m_CommandList;

	nvrhi::DeviceHandle m_Device;
};

} // namespace st::gfx