#include "Gfx/RenderPass.h"
#include "Gfx/RenderView.h"

void st::gfx::RenderPass::Attach(st::gfx::RenderView* renderView)
{
	assert(m_RenderView.expired() && "Trying to attach an already attached render pass");
	m_RenderView = renderView->weak_from_this();

	OnAttached();
}

void st::gfx::RenderPass::Detach()
{
	OnDetached();

	assert(!m_RenderView.expired() && "Trying to detach a non-attached render pass");
	m_RenderView.reset();
}