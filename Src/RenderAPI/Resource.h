#pragma once

#include <memory>

namespace st::rapi
{
	struct NativeResource
	{
		union {
			uint64_t integer;
			void* pointer;
		};

		NativeResource(uint64_t i) : integer(i) {}
		NativeResource(void* p) : pointer(p) {}

		template<typename T> 
		operator T* () const { return static_cast<T*>(pointer); }
	};

	class IResource : public std::enable_shared_from_this<IResource>
	{
	public:

		// Returns a native object or interface, for example ID3D11Device*, or nullptr if the requested interface is unavailable.
		// Does *not* AddRef the returned interface.
		virtual NativeResource GetNativeResource() { return nullptr; }

	protected:

		IResource() = default;
		virtual ~IResource() = default;
	};
}