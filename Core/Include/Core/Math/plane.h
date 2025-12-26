#pragma once

#include "Core/Math/glm_config.h"

namespace st::math
{

template<typename T>
struct plane3
{
	using vec_t = glm::vec<3, T, glm::defaultp>;

	vec_t normal;	// normal
	T d;			// independent value

	plane3() = default;

	// Construct from normal and point
	plane3(const vec_t& n, const vec_t& p)
	{
		normal = glm::normalize(n);
		d = -glm::dot(normal, p);
	}

	plane3(const vec_t& n, T dVal) : normal{ n }, d{ dVal } {}

	plane3(const glm::vec<4, T, glm::defaultp>& xyzd)
	{
		normal = vec_t{ xyzd.x, xyzd.y, xyzd.z };
		d = xyzd.w;
		normalize();
	}

	// Distance to a point
	T distance(const vec_t p) const
	{
		return glm::dot(normal, p) + d;
	}

	// Normalize plane
	void normalize()
	{
		T len = glm::length(normal);
		normal = normal / len;
		d = d / len;
	}
};

using plane3f = plane3<float>;

} // namespace st::math
