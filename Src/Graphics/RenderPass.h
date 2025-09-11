#pragma once

#include <glm/vec2.hpp>

namespace nvrhi
{
	class IFramebuffer;
};

namespace st::gfx
{
class RenderPass
{
public:

	virtual bool Render(nvrhi::IFramebuffer* frameBuffer) = 0;

	virtual void OnBackbufferResize(const glm::ivec2& size) = 0;
};
}