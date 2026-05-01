#pragma once

namespace alm::gfx
{

template<class T>
struct ResourceRefCount
{
	T* resource = nullptr;
	uint32_t refCount = 0;

	ResourceRefCount() = default;
	ResourceRefCount(T* t, uint32_t count = 0)
		: resource{ t }, refCount{ count }
	{}

	bool operator==(const ResourceRefCount<T>& other) const
	{
		return resource == other.resource;
	}

	T* get() noexcept { return resource; }
	const T* get() const noexcept { return resource; }

	T* operator ->() noexcept { return resource; }
	const T* operator ->() const noexcept { return resource; }

	T* operator *() noexcept { return resource; }
	const T* operator *() const noexcept { return resource; }
};

} // namespace alm::gfx

namespace std 
{

	template<class T>
	struct hash<alm::gfx::ResourceRefCount<T>> 
	{
		size_t operator()(const alm::gfx::ResourceRefCount<T>& mrc) const noexcept 
		{
			return hash<T*>{}(mrc.resource);
		}
	};

} // namespace std

