#pragma once

#include "Core/Math/glm_config.h"

namespace st::math
{

template<typename T, int n>
struct plane
{
	static_assert(n > 1);

	using vec_t = glm::vec<n, T, glm::defaultp>;

	vec_t normal;	// normal
	T d;			// independent value

	plane() = default;

	// Construct from normal and point
	plane(const vec_t& n, const vec_t& p)
	{
		normal = glm::normalize(n);
		d = -glm::dot(normal, p);
	}

	plane(const vec_t& n, T dVal) : normal{ n }, d{ dVal } {}

	plane(const glm::vec<n + 1, T, glm::defaultp>& xyzd)
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

using plane2f = plane<float, 2>;
using plane3f = plane<float, 3>;

} // namespace st::math
