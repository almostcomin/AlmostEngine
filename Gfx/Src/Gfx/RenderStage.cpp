#include "Gfx/RenderStage.h"
#include "Gfx/RenderView.h"

void st::gfx::RenderStage::Attach(st::gfx::RenderView* renderView)
{
	assert(m_RenderView.expired() && "Trying to attach an already attached render pass");
	m_RenderView = renderView->weak_from_this();

	OnAttached();
}

void st::gfx::RenderStage::Detach()
{
	OnDetached();
	m_RenderView.reset();
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