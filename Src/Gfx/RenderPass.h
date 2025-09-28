#pragma once

//#include <glm/vec2.hpp>
#include "Core/Math/glm_config.h"
#include "Core/Memory.h"

namespace nvrhi
{
	class IFramebuffer;
};

namespace st::gfx
{
class RenderPass
{
	friend class RenderView;
public:

	virtual bool Render(nvrhi::IFramebuffer* frameBuffer) = 0;
	virtual void OnBackbufferResize(const glm::ivec2& size) = 0;

protected:

	st::weak<RenderView> m_RenderView;

private:

	void Attach(RenderView* renderView);
	void Detach();
};
}