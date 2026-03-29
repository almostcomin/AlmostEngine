#include "Gfx/RenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"

alm::gfx::RenderView* alm::gfx::RenderStage::GetRenderView() const
{
	if (!m_RenderGraph)
	{
		LOG_ERROR("Render stage {} is not attached", GetDebugName());
		return {};
	}

	return m_RenderGraph->GetRenderView();
}

alm::gfx::Camera* alm::gfx::RenderStage::GetCamera() const
{
	return GetRenderView()->GetCamera().get();
}

alm::gfx::Scene* alm::gfx::RenderStage::GetScene() const
{
	auto scene = GetRenderView()->GetScene();
	return scene ? scene.get() : nullptr;
}

alm::gfx::DeviceManager* alm::gfx::RenderStage::GetDeviceManager() const
{
	return GetRenderView()->GetDeviceManager();
}

void alm::gfx::RenderStage::Attach(RenderGraph* renderGraph)
{
	assert(m_RenderGraph.expired() && "Trying to attach an already attached render pass");
	m_RenderGraph = renderGraph->weak_from_this();

	OnAttached();
}

void alm::gfx::RenderStage::Detach()
{
	OnDetached();
	m_RenderGraph.reset();
}

void alm::gfx::RenderStage::SetEnabled(bool b) 
{ 
	m_Enabled = b; 
	if (m_Enabled)
	{
		OnEnabled();
	}
	else
	{
		OnDisabled();
	}
}