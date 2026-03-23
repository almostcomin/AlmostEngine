#pragma once

#include <memory>

namespace alm::gfx
{
	class DeviceManager;
	class Material;
	class MaterialInstance;
};

namespace alm::gfx
{

class MaterialFactory
{
public:

	MaterialFactory(DeviceManager* deviceManager);

	std::shared_ptr<MaterialInstance> GetMaterialInstance();

private:

	class DeviceManager* m_DeviceManager;
};

} // namespace st::gfx