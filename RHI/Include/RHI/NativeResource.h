#pragma once

namespace st::rhi
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

} // namespace st::rhi