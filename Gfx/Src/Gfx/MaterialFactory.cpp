#include "Gfx/GfxPCH.h"
#include "Gfx/MaterialFactory.h"
#include "Gfx/Material.h"
#include "Gfx/MaterialInstance.h"

alm::gfx::MaterialFactory::MaterialFactory(DeviceManager* deviceManager) : 
	m_DeviceManager(deviceManager)
{}

std::shared_ptr<alm::gfx::MaterialInstance> alm::gfx::MaterialFactory::GetMaterialInstance()
{
	return {};
}