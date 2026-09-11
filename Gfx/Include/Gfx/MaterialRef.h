#pragma once

namespace alm::gfx
{

class Material;

class MaterialRef
{
	friend class MaterialManager;

public:

	MaterialRef() = default;
	MaterialRef(const MaterialRef& other);

	~MaterialRef();
	
	MaterialRef& operator=(const MaterialRef& other);

	Material* GetMaterial() const { return m_Proxy ? m_Proxy->Material.get() : nullptr; }
	int GetRefCount() const { return m_Proxy ? m_Proxy->RefCount.load() : 0; }

	bool IsValid() const { return m_Proxy != nullptr; }
	void Reset();

private:

	struct Proxy
	{
		std::unique_ptr<gfx::Material> Material = nullptr;
		std::atomic<int> RefCount;
		gfx::MaterialManager* MaterialManager = nullptr;
	};

	MaterialRef(Proxy* proxy);

	Proxy* m_Proxy = nullptr;
};

} // namespace alm::gfx