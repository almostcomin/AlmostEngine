#pragma once

#define GLM_FORCE_XYZW_ONLY
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace st
{
	using float2 = glm::vec2;
	using float3 = glm::vec3;
	using float4 = glm::vec4;

	using int2 = glm::ivec2;
	using int3 = glm::ivec3;
	using int4 = glm::ivec4;

	using uint2 = glm::uvec2;
	using uint3 = glm::uvec3;
	using uint4 = glm::uvec4;

	using float3x3 = glm::fmat3x3;
	using float4x4 = glm::fmat4x4;

} // namespace st