#include "Gfx/ForwardRenderPass.h"
#include "Core/Log.h"

bool st::gfx::ForwardRenderPass::Render(nvrhi::IFramebuffer* frameBuffer)
{
	if (!m_SceneGraph)
	{
		LOG_ERROR("No scene graph set. Nothing to render");
		return false;
	}

	return true;
}