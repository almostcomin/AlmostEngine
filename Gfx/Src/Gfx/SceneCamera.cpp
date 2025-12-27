#include "Gfx/SceneCamera.h"

st::gfx::PerspectiveCamera& st::gfx::PerspectiveCamera::SetFar(float _far)
{ 
	if (_far > 0.f)
		m_zFar = _far;
	else
		m_zFar.reset();
	
	return *this;
}

st::gfx::PerspectiveCamera& st::gfx::PerspectiveCamera::SetAspect(float aspect)
{ 
	if (aspect > 0.f)
		m_Aspect = aspect;
	else
		m_Aspect.reset();

	return *this;
}