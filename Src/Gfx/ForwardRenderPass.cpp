#include "Gfx/ForwardRenderPass.h"
#include "Core/Log.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/RenderView.h"
#include "Gfx/Camera.h"

bool st::gfx::ForwardRenderPass::Render(nvrhi::IFramebuffer* frameBuffer)
{
	if (!m_SceneGraph)
	{
		LOG_ERROR("No scene graph set. Nothing to render");
		return false;
	}

	auto camera = m_RenderView->GetCamera();
	if (!camera)
	{
		LOG_ERROR("No camera set. Can't render without a camera.");
	}

	const auto& frustum = camera->GetFrustum();

	st::gfx::SceneGraph::Walker walker{ *m_SceneGraph };
	while(walker)
	{
		if (walker->HasBounds() && frustum.check(walker->GetBounds()))
		{
			auto leaf = walker->GetLeaf();
			if (leaf && leaf->GetContentFlags() == SceneContentFlags::OpaqueMeshes)
			{
				st::math::aabox3f worldAABox = leaf->GetBounds() * walker->GetWorldTransform();
				// todo: draw bbox?

				auto mesh = static_cast<st::gfx::MeshInstance*>(leaf.get());
			}
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	} // while(walker)

	return true;
}