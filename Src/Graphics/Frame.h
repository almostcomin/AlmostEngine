#pragma once
#include "Core/Core.h"

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

private:

	std::vector<RenderPass*> m_RenderPasses;
	DeviceManager* m_DeviceManager;
};

}