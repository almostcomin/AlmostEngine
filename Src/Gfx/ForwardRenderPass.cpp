#include "Gfx/ForwardRenderPass.h"
#include "Core/Log.h"

bool st::gfx::ForwardRenderPass::Render(nvrhi::IFramebuffer* frameBuffer)
{
	auto scene = m_SceneGraph.lock();
	if (!scene)
	{
		LOG_ERROR("No scene graph set. Nothing to render");
		return false;
	}

	return true;
}