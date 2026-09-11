#pragma once

#include "Gfx/MaterialRef.h"

namespace alm::gfx
{

class Material;
class MaterialRef;

class MaterialManager
{
	friend class MaterialRef;

public:

	MaterialManager();
	~MaterialManager();

	MaterialRef CreateNewMaterial(const std::string& name, const std::string& filename = {});
	MaterialRef CreateNewMaterial(std::unique_ptr<Material>&& material);
	MaterialRef GetOrCreateMaterial(std::unique_ptr<Material>&& material);

	MaterialRef GetEmptyMaterial() const;

	size_t GetMaterialCount() const { return m_Materials.size(); }
	MaterialRef GetMaterial(size_t index) const;

private:

	void DumpMaterial(MaterialRef::Proxy* proxy);

private:

	std::mutex m_MaterialsMutex;
	std::vector<MaterialRef::Proxy*> m_Materials;

	MaterialRef m_EmptyMaterial;
};


} // namespace alm::gfx