#include "Gfx/GfxPCH.h"
#include "Gfx/MaterialManager.h"
#include "Gfx/Material.h"

alm::gfx::MaterialManager::MaterialManager()
{
	m_EmptyMaterial = CreateNewMaterial("<empty>", "<null>");
}

alm::gfx::MaterialManager::~MaterialManager()
{
	m_EmptyMaterial.Reset();
	assert(m_Materials.empty());
}

alm::gfx::MaterialRef alm::gfx::MaterialManager::CreateNewMaterial(const std::string& name, const std::string& filename)
{
	auto* proxy = new MaterialRef::Proxy;

	proxy->Material = std::make_unique<gfx::Material>(name, filename);
	proxy->RefCount.fetch_add(1);
	proxy->MaterialManager = this;

	{
		std::scoped_lock lock{ m_MaterialsMutex };
		m_Materials.push_back(proxy);
	}

	alm::gfx::MaterialRef ref;
	ref.m_Proxy = proxy;

	return ref;
}

alm::gfx::MaterialRef alm::gfx::MaterialManager::CreateNewMaterial(std::unique_ptr<Material>&& material)
{
	auto* proxy = new MaterialRef::Proxy;

	proxy->Material = std::move(material);
	proxy->RefCount.fetch_add(1);
	proxy->MaterialManager = this;

	{
		std::scoped_lock lock{ m_MaterialsMutex };
		m_Materials.push_back(proxy);
	}

	alm::gfx::MaterialRef ref;
	ref.m_Proxy = proxy;

	return ref;
}

alm::gfx::MaterialRef alm::gfx::MaterialManager::GetOrCreateMaterial(std::unique_ptr<Material>&& material)
{
	std::scoped_lock lock{ m_MaterialsMutex };
	for (MaterialRef::Proxy* proxy : m_Materials)
	{
		if (*proxy->Material == *material)
		{
			return MaterialRef{ proxy };
		}
	}

	return CreateNewMaterial(std::move(material));
}

alm::gfx::MaterialRef alm::gfx::MaterialManager::GetEmptyMaterial() const
{
	return m_EmptyMaterial;
}

alm::gfx::MaterialRef alm::gfx::MaterialManager::GetMaterial(size_t index) const
{
	return index < m_Materials.size() ?
		MaterialRef{ m_Materials[index] } : MaterialRef{};
}

void alm::gfx::MaterialManager::DumpMaterial(MaterialRef::Proxy* proxy)
{
	std::scoped_lock lock{ m_MaterialsMutex };
	auto it = std::ranges::find(m_Materials, proxy);
	assert(it != m_Materials.end());

	delete *it;
	fast_erase(m_Materials, it);
}
