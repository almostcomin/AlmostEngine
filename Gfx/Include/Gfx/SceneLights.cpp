#include "Gfx/SceneLights.h"
#include "Gfx/SceneGraphNode.h"

float3 alm::gfx::SceneDirectionalLight::GetDirection() const
{
	constexpr float3 localDir = float3{ 0.0f, 0.0f, -1.0f };

	const float4x4& worldMatrix = GetNode()->GetWorldTransform();
	float3 worldDir = glm::normalize(worldMatrix * float4{ localDir, 0.f });
	return worldDir;
}

alm::gfx::ScenePointLight::ScenePointLight() :
	m_Intensity{ 1.f }, m_Range { 0.f }, m_Radius{ 0.f }
{}

void alm::gfx::ScenePointLight::SetRange(float v)
{ 
	assert(v >= 1.0e-05f);

	m_Range = v; 
	m_Bounds.min = float3{ -m_Range };
	m_Bounds.max = float3{ m_Range };

	OnBoundsChanged();
}

alm::gfx::SceneSpotLight::SceneSpotLight() :
	m_Intensity{ 1.f }, m_Range{ 0.f }, m_Radius{ 0.f }, m_InnerAngle{ 0.f }, m_OuterAngle{ 0.f }
{}

void alm::gfx::SceneSpotLight::SetRange(float v)
{
	assert(v >= 1.0e-05f);

	m_Range = v;
	m_Bounds.min = float3{ -m_Range };
	m_Bounds.max = float3{ m_Range };

	OnBoundsChanged();
}

float3 alm::gfx::SceneSpotLight::GetDirection() const
{
	constexpr float3 localDir = float3{ 0.0f, 0.0f, -1.0f };

	const float4x4& worldMatrix = GetNode()->GetWorldTransform();
	float3 worldDir = glm::normalize(worldMatrix * float4{ localDir, 0.f });
	return worldDir;
}