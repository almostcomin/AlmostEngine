#include "Gfx/Camera.h"
#include "Gfx/Util.h"

st::gfx::Camera::Camera() :
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

void st::gfx::Camera::SetPosition(const float3& pos)
{
	m_Position = pos;
	m_IsDirty = true;
}

void st::gfx::Camera::SetForward(const float3& dir)
{
	m_Forward = glm::normalize(dir);

	m_Right = glm::normalize(glm::cross(m_Forward, m_UpRef));
	m_Up = glm::cross(m_Right, m_Forward);

	m_IsDirty = true;
}

void st::gfx::Camera::SetUpRef(const float3& v)
{
	m_UpRef = glm::normalize(v);

	m_Right = glm::normalize(glm::cross(m_Forward, m_UpRef));
	m_Up = glm::cross(m_Right, m_Forward);

	m_IsDirty = true;
}

void st::gfx::Camera::LookAt(const float3& pos)
{
	SetForward(pos - m_Position);
}

void st::gfx::Camera::SetVerticalFov(float vFov)
{
	m_VerticalFov = vFov;
	m_IsDirty = true;
}

void st::gfx::Camera::SetAspect(float aspect)
{
	m_Aspect = aspect;
	m_IsDirty = true;
}

void st::gfx::Camera::SetZNear(float zNear)
{
	m_zNear = zNear;
	m_IsDirty = true;
}

void st::gfx::Camera::SetZFar(float zFar)
{
	m_zFar = zFar;
	m_IsDirty = true;
}

void st::gfx::Camera::SetProjectionModel(st::gfx::Camera::ProjectionModel model)
{
	m_ProjectionModel = model;
	m_IsDirty = true;
}

void st::gfx::Camera::Fit(const st::math::aabox3f& bounds)
{
	const float3 targetPos = bounds.center();
	const float radius = glm::length(bounds.extents()) / 2.f;
	const float distance = radius / sinf(m_VerticalFov / 2.f);

	const float3 newFwd = glm::normalize(targetPos - m_Position);
	const float3 newPos = targetPos - newFwd * distance;

	SetPosition(newPos);
	SetForward(newFwd);
}

float3 st::gfx::Camera::GetFrustumTopLeftDirView()
{
	float tanHalfFov = tanf(m_VerticalFov * 0.5f);
	return { -tanHalfFov * m_Aspect, tanHalfFov, -1.f };
}

float3 st::gfx::Camera::GetFrustumTopRightDirView()
{
	float tanHalfFov = tanf(m_VerticalFov * 0.5f);
	return { tanHalfFov * m_Aspect, tanHalfFov, -1.f };
}

float3 st::gfx::Camera::GetFrustumBottomLeftDirView()
{
	float tanHalfFov = tanf(m_VerticalFov * 0.5f);
	return { -tanHalfFov * m_Aspect, -tanHalfFov, -1.f };
}

float3 st::gfx::Camera::GetFrustumBottomRightDirView()
{
	float tanHalfFov = tanf(m_VerticalFov * 0.5f);
	return { tanHalfFov * m_Aspect, -tanHalfFov, -1.f };
}

const float4x4& st::gfx::Camera::GeViewMatrix()
{
	UpdateMatrices();
	return m_ViewMatrix;
}

const float4x4& st::gfx::Camera::GetProjectionMatrix()
{
	UpdateMatrices();
	return m_ProjectionMatrix;
}

float4x4 st::gfx::Camera::GetViewProjectionMatrix()
{
	UpdateMatrices();
	return m_ProjectionMatrix * m_ViewMatrix;
}

const st::math::frustum3f& st::gfx::Camera::GetFrustum()
{
	UpdateMatrices();
	return m_Frustum;
}

float st::gfx::Camera::GetYaw() const
{
	return atan2(m_Forward.x, -m_Forward.z);
}

float st::gfx::Camera::GetPitch() const
{
	return asinf(m_Forward.y);
}

float st::gfx::Camera::GetRoll() const
{
	float3 rightNoRoll = glm::normalize(glm::cross(float3{ 0.f, 1.f, 0.f }, m_Forward));
	float3 upNoRoll = glm::cross(m_Forward, rightNoRoll);
	return atan2f(
		glm::dot(glm::cross(upNoRoll, m_UpRef), m_Forward),
		glm::dot(upNoRoll, m_UpRef));
}

void st::gfx::Camera::SetYaw(float yaw)
{
	float pitch = GetPitch();

	float3 newForward;
	newForward.x = cos(pitch) * sin(yaw);
	newForward.y = sin(pitch);
	newForward.z = cos(pitch) * cos(yaw);
	newForward.z *= -1.f;

	SetForward(newForward);
}

void st::gfx::Camera::SetPitch(float pitch)
{
	float yaw = GetYaw();

	float3 newForward;
	newForward.x = cos(pitch) * sin(yaw);
	newForward.y = sin(pitch);
	newForward.z = cos(pitch) * cos(yaw);
	newForward.z *= -1.f;

	SetForward(newForward);
}

void st::gfx::Camera::SetRoll(float roll)
{
	float3 right = glm::normalize(glm::cross(float3{ 0.f, 1.f, 0.f }, m_Forward));
	float3 baseUp = float3{ 0.f, 1.f, 0.f };// glm::cross(m_Forward, right);

	float c = cosf(roll);
	float s = -sinf(roll);
	float3 newUp = baseUp * c + m_Right * s;

	SetUpRef(newUp);
}

void st::gfx::Camera::UpdateMatrices()
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

void st::gfx::Camera::UpdateWorldViewMatrix()
{
	m_ViewMatrix = glm::lookAtRH(m_Position, m_Position + m_Forward, m_Up);
}

// D3D / Vulkan style, z clip space [0, 1]
void st::gfx::Camera::UpdateProjectionMatrix()
{
	m_ProjectionMatrix = glm::perspectiveRH_ZO(m_VerticalFov, m_Aspect, m_zNear, m_zFar);
}

// Column major, right handed, reverse Z [1, 0]
void st::gfx::Camera::UpdateProjectionMatrixReverse()
{
	m_ProjectionMatrix = BuildPersInvZInfFar(m_VerticalFov, m_Aspect, m_zNear);
}

void st::gfx::Camera::UpdateFrustum()
{
	m_Frustum = st::math::frustum3f{ m_ProjectionMatrix * m_ViewMatrix };
}