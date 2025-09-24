#include "Gfx/Camera.h"
#include <glm/ext/matrix_transform.hpp>

st::gfx::Camera::Camera() :
	m_ProjectionModel{ ProjectionModel::Reverse },
	m_Position{ 0.f, 0.f, 0.f },
	m_Forward{ 0.f, 0.f, 1.f },
	m_Up{ 0.f, 1.f, 0.f },
	m_Right{ -1.f, 0.f, 0.f },
	m_VerticalFov{ glm::radians(60.f) },
	m_Aspect{ 1.f },
	m_zNear{ 1.f },
	m_zFar{ 1000.f },
	m_IsDirty{ true }
{
}

st::gfx::Camera& st::gfx::Camera::SetPosition(const glm::vec3& pos)
{
	m_Position = pos;
	m_IsDirty = true;
	return *this;
}

st::gfx::Camera& st::gfx::Camera::SetForward(const glm::vec3& dir)
{
	m_Forward = glm::normalize(dir);

	m_Right = glm::cross(m_Forward, glm::vec3{ 0.f, 1.f, 0.f });
	m_Up = glm::cross(m_Right, m_Forward);

	m_IsDirty = true;
	return *this;
}

st::gfx::Camera& st::gfx::Camera::SetVerticalFov(float vFov)
{
	m_VerticalFov = vFov;
	m_IsDirty = true;
	return *this;
}

st::gfx::Camera& st::gfx::Camera::SetAspect(float aspect)
{
	m_Aspect = aspect;
	m_IsDirty = true;
	return *this;
}

st::gfx::Camera& st::gfx::Camera::SetZNear(float zNear)
{
	m_zNear = zNear;
	m_IsDirty = true;
	return *this;
}

st::gfx::Camera& st::gfx::Camera::SetZFar(float zFar)
{
	m_zFar = zFar;
	m_IsDirty = true;
	return *this;
}

st::gfx::Camera& st::gfx::Camera::SetProjectionModel(st::gfx::Camera::ProjectionModel model)
{
	m_ProjectionModel = model;
	m_IsDirty = true;
	return *this;
}

const glm::mat4& st::gfx::Camera::GeViewMatrix()
{
	UpdateMatrices();
	return m_ViewMatrix;
}

const glm::mat4& st::gfx::Camera::GetProjectionMatrix()
{
	UpdateMatrices();
	return m_ProjectionMatrix;
}

glm::mat4 st::gfx::Camera::GetViewProjectionMatrix()
{
	UpdateMatrices();
	return m_ProjectionMatrix * m_ViewMatrix;
}

const st::math::frustum& st::gfx::Camera::GetFrustum()
{
	UpdateMatrices();
	return m_Frustum;
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
	float yScale = 1.0f / tanf(0.5f * m_VerticalFov);
	float xScale = yScale / m_Aspect;
	float zScale = 1.0f / (m_zFar - m_zNear);

	m_ProjectionMatrix = glm::mat4{
		xScale,	0,		0,							0,
		0,		yScale,	0,							0,
		0,		0,		m_zFar * zScale,			1,
		0,		0,		-m_zNear * m_zFar * zScale,	0 };
}

// D3D / Vulkan style, z clip space [1, 0]
void st::gfx::Camera::UpdateProjectionMatrixReverse()
{
	float yScale = 1.0f / tanf(0.5f * m_VerticalFov);
	float xScale = yScale / m_Aspect;
	m_ProjectionMatrix = glm::mat4{
		xScale,	0,		0,			0,
		0,		yScale, 0,			0,
		0,		0,		0,			1,
		0,		0,		m_zNear,	0 };
}

void st::gfx::Camera::UpdateFrustum()
{
	m_Frustum = st::math::frustum{ m_ProjectionMatrix * m_ViewMatrix };
}