#pragma once

#include "Gfx/SceneGraphLeaf.h"
#include <optional>

namespace st::gfx
{

class SceneCamera : public SceneGraphLeaf
{
public:

	SceneCamera() = default;

	virtual SceneContentFlags GetContentFlags() const { return st::gfx::SceneContentFlags::Cameras; }

private:
};

class PerspectiveCamera : public SceneCamera
{
public:

	PerspectiveCamera& SetNear(float near) { m_zNear = near; return *this; }
	//! Negative to reset / not specified.
	PerspectiveCamera& SetFar(float far);
	PerspectiveCamera& SetVerticalFOV(float fov) { m_VerticalFOV = fov; return *this; }
	//! Negative to reset / not specified.
	PerspectiveCamera& SetAspect(float aspect);

private:

	float m_zNear = 1.f;
	std::optional<float> m_zFar; // use reverse infinite projection if not specified
	float m_VerticalFOV;
	std::optional<float> m_Aspect;
};

class OrthographicCamera : public SceneCamera
{
public:

	OrthographicCamera& SetNear(float near) { m_zNear = near; return *this; }
	OrthographicCamera& SetFar(float far) { m_zFar = far; return *this; }
	OrthographicCamera& SetXMag(float xMag) { m_xMag = xMag; return *this; }
	OrthographicCamera& SetYMag(float yMag) { m_yMag = yMag; return *this; }

private:

	float m_zNear = 0.f;
	float m_zFar = 1.f;
	float m_xMag = 1.f;
	float m_yMag = 1.f;
};

} // namespace st::gfx