#pragma once

#include "Core/Math/glm_config.h"
#include "Core/Memory.h"

namespace st::rapi
{
	class IFramebuffer;
};

namespace st::gfx
{
class RenderPass
{
	friend class RenderView;

public:

	virtual bool Render(rapi::IFramebuffer* frameBuffer) = 0;
	virtual void OnBackbufferResize(const glm::ivec2& size) = 0;

protected:

	st::weak<RenderView> m_RenderView;

private:

	void Attach(RenderView* renderView);
	void Detach();

	virtual void OnAttached() {};
	virtual void OnDetached() {};
};
}