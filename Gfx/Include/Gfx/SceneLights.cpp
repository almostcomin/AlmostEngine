#include "Gfx/SceneLights.h"
#include "Gfx/SceneGraphNode.h"

st::gfx::ScenePointLight::ScenePointLight() : m_Range{ 0.f }, m_Radius{ 0.f }
{}

void st::gfx::ScenePointLight::SetRange(float v)
{ 
	assert(v >= 1.0e-05f);

	m_Range = v; 
	m_Bounds.min = float3{ -m_Range };
	m_Bounds.max = float3{ m_Range };

	OnBoundsChanged();
}
