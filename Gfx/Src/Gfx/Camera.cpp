#include "Gfx/GfxPCH.h"
#include "Gfx/Camera.h"
#include "Gfx/Util.h"

alm::gfx::Camera::Camera() :
	m_ProjectionModel{ ProjectionModel::Reverse },
	m_Position{ 0.f, 0.f, 0.f },
	m_Forward{ 0.f, 0.f, -1.f }, // Right-hand
	m_Up{ 0.f, 1.f, 0.f },
	m_Right{ 1.f, 0.f, 0.f },
	m_UpRef{ 0.f, 1.f, 0.f },
	m_VerticalFov{ glm::radians(60.f) },
	m_Aspect{ 1.f },
	m_zNear{ 0.1f },
	m_zFar{ 1000.f },
	m_IsDirty{ true }
{
}

void alm::gfx::Camera::SetPosition(const float3& pos)
{
	m_Position = pos;
	m_IsDirty = true;
}

void alm::gfx::Camera::SetForward(const float3& dir)
{
	m_Forward = glm::normalize(dir);

	m_Right = glm::normalize(glm::cross(m_Forward, m_UpRef));
	m_Up = glm::cross(m_Right, m_Forward);

	m_IsDirty = true;
}

void alm::gfx::Camera::SetUpRef(const float3& v)
{
	m_UpRef = glm::normalize(v);

	m_Right = glm::normalize(glm::cross(m_Forward, m_UpRef));
	m_Up = glm::cross(m_Right, m_Forward);

	m_IsDirty = true;
}

void alm::gfx::Camera::LookAt(const float3& pos)
{
	SetForward(pos - m_Position);
}

void alm::gfx::Camera::SetVerticalFov(float vFov)
{
	m_VerticalFov = vFov;
	m_IsDirty = true;
}

void alm::gfx::Camera::SetAspect(float aspect)
{
	m_Aspect = aspect;
	m_IsDirty = true;
}

void alm::gfx::Camera::SetZNear(float zNear)
{
	m_zNear = zNear;
	m_IsDirty = true;
}

void alm::gfx::Camera::SetZFar(float zFar)
{
	m_zFar = zFar;
	m_IsDirty = true;
}

void alm::gfx::Camera::SetProjectionModel(alm::gfx::Camera::ProjectionModel model)
{
	m_ProjectionModel = model;
	m_IsDirty = true;
}

void alm::gfx::Camera::Fit(const alm::math::aabox3f& bounds)
{
	const float3 targetPos = bounds.center();
	const float radius = glm::length(bounds.extents()) / 2.f;
	const float distance = radius / sinf(m_VerticalFov / 2.f);

	const float3 newFwd = glm::normalize(targetPos - m_Position);
	const float3 newPos = targetPos - newFwd * distance;

	SetPosition(newPos);
	SetForward(newFwd);
}

const float4x4& alm::gfx::Camera::GetViewMatrix()
{
	UpdateMatrices();
	return m_ViewMatrix;
}

const float4x4& alm::gfx::Camera::GetProjectionMatrix()
{
	UpdateMatrices();
	return m_ProjectionMatrix;
}

float4x4 alm::gfx::Camera::GetViewProjectionMatrix()
{
	UpdateMatrices();
	return m_ProjectionMatrix * m_ViewMatrix;
}

float4x4 alm::gfx::Camera::GetClipToTranslatedWorldMatrix()
{
	UpdateMatrices();

	float4x4 invView = glm::inverse(m_ViewMatrix);
	invView[3] = float4(0.f, 0.f, 0.f, 1.f);
	float4x4 invProj = glm::inverse(m_ProjectionMatrix);

	return invView * invProj;
}

const alm::math::frustum3f& alm::gfx::Camera::GetFrustum()
{
	UpdateMatrices();
	return m_Frustum;
}

float alm::gfx::Camera::GetYaw() const
{
	return atan2(m_Forward.x, -m_Forward.z);
}

float alm::gfx::Camera::GetPitch() const
{
	return asinf(m_Forward.y);
}

float alm::gfx::Camera::GetRoll() const
{
	float3 rightNoRoll = glm::normalize(glm::cross(float3{ 0.f, 1.f, 0.f }, m_Forward));
	float3 upNoRoll = glm::cross(m_Forward, rightNoRoll);
	return atan2f(
		glm::dot(glm::cross(upNoRoll, m_UpRef), m_Forward),
		glm::dot(upNoRoll, m_UpRef));
}

void alm::gfx::Camera::SetYaw(float yaw)
{
	float pitch = GetPitch();

	float3 newForward;
	newForward.x = cos(pitch) * sin(yaw);
	newForward.y = sin(pitch);
	newForward.z = cos(pitch) * cos(yaw);
	newForward.z *= -1.f;

	SetForward(newForward);
}

void alm::gfx::Camera::SetPitch(float pitch)
{
	float yaw = GetYaw();

	float3 newForward;
	newForward.x = cos(pitch) * sin(yaw);
	newForward.y = sin(pitch);
	newForward.z = cos(pitch) * cos(yaw);
	newForward.z *= -1.f;

	SetForward(newForward);
}

void alm::gfx::Camera::SetRoll(float roll)
{
	float3 right = glm::normalize(glm::cross(float3{ 0.f, 1.f, 0.f }, m_Forward));
	float3 baseUp = float3{ 0.f, 1.f, 0.f };// glm::cross(m_Forward, right);

	float c = cosf(roll);
	float s = -sinf(roll);
	float3 newUp = baseUp * c + m_Right * s;

	SetUpRef(newUp);
}

void alm::gfx::Camera::UpdateMatrices()
{
	if (m_IsDirty)
	{
		UpdateWorldViewMatrix();
		switch (m_ProjectionModel)
		{
		case ProjectionModel::Reverse:
			UpdateProjectionMatrixReverse();
			break;
		case ProjectionModel::Standard:
			UpdateProjectionMatrix();
		}
		UpdateFrustum();

		m_IsDirty = false;
	}
}

void alm::gfx::Camera::UpdateWorldViewMatrix()
{
	m_ViewMatrix = glm::lookAtRH(m_Position, m_Position + m_Forward, m_Up);
}

// D3D / Vulkan style, z clip space [0, 1]
void alm::gfx::Camera::UpdateProjectionMatrix()
{
	m_ProjectionMatrix = glm::perspectiveRH_ZO(m_VerticalFov, m_Aspect, m_zNear, m_zFar);
}

// Column major, right handed, reverse Z [1, 0]
void alm::gfx::Camera::UpdateProjectionMatrixReverse()
{
	m_ProjectionMatrix = BuildPersInvZInfFar(m_VerticalFov, m_Aspect, m_zNear);
}

void alm::gfx::Camera::UpdateFrustum()
{
	m_Frustum = alm::math::frustum3f{ m_ProjectionMatrix * m_ViewMatrix };
}