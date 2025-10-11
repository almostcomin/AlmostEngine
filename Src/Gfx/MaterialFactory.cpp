#include "Gfx/MaterialFactory.h"
#include "Gfx/Material.h"
#include "Gfx/MaterialInstance.h"

st::gfx::MaterialFactory::MaterialFactory(DeviceManager* deviceManager) : 
	m_DeviceManager(deviceManager)
{}

std::shared_ptr<st::gfx::MaterialInstance> st::gfx::MaterialFactory::GetMaterialInstance()
{
	return {};
}