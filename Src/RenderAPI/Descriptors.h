#pragma once

#include <cstdint>

namespace st::rapi
{

using DescriptorIndex = uint32_t;
constexpr DescriptorIndex c_InvalidDescriptorIndex = ~0u;

enum class DescriptorType : uint8_t
{
	SRV,
	UAV,

	_Size
};

enum class ResourceUsage
{
	None = 0x0,
	ShaderResource = 0x1,
	UnorderedAccess = 0x2
};
ENUM_CLASS_FLAG_OPERATORS(ResourceUsage)

} // namespace st::rapi