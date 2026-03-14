#include "Gfx/RenderStage.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"

st::gfx::RenderView* st::gfx::RenderStage::GetRenderView() const
{
	if (!m_RenderGraph)
	{
		LOG_ERROR("Render stage {} is not attached", GetDebugName());
		return {};
	}

	return m_RenderGraph->GetRenderView();
}

st::gfx::Scene* st::gfx::RenderStage::GetScene() const
{
	auto scene = GetRenderView()->GetScene();
	return scene ? scene.get() : nullptr;
}

st::gfx::DeviceManager* st::gfx::RenderStage::GetDeviceManager() const
{
	return GetRenderView()->GetDeviceManager();
}

void st::gfx::RenderStage::Attach(RenderGraph* renderGraph)
{
	assert(m_RenderGraph.expired() && "Trying to attach an already attached render pass");
	m_RenderGraph = renderGraph->weak_from_this();

	OnAttached();
}

void st::gfx::RenderStage::Detach()
{
	OnDetached();
	m_RenderGraph.reset();
}

void st::gfx::RenderStage::SetEnabled(bool b) 
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