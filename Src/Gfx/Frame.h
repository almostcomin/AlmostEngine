#pragma once

#include <vector>
#include <glm/vec2.hpp>

namespace nvrhi
{
	class IFramebuffer;
};

namespace st::gfx
{
	class DeviceManager;
	class RenderPass;
}

namespace st::gfx
{

class Frame
{
public:

	void Init(DeviceManager* deviceManager, std::vector<RenderPass*>&& renderPasses);

	void Render(nvrhi::IFramebuffer* frameBuffer);

	void OnBackbufferResize(const glm::ivec2& size);

private:

	std::vector<RenderPass*> m_RenderPasses;
	DeviceManager* m_DeviceManager;
};

}