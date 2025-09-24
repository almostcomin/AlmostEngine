#pragma once

#include "Core/Math/frustum.h"

namespace st::gfx
{

class Camera
{
public:

	enum class ProjectionModel
	{
		Reverse, // default
		Standard
	};

public:

	Camera();

	Camera& SetPosition(const glm::vec3& pos);
	Camera& SetForward(const glm::vec3& dir);
	
	Camera& SetVerticalFov(float vFov);
	Camera& SetAspect(float aspect);
	Camera& SetZNear(float zNear);
	Camera& SetZFar(float zFar);

	Camera& SetProjectionModel(ProjectionModel model);

	const glm::vec3& GetPosition() const { return m_Position; }
	const glm::vec3& GetForward() const { return m_Forward; }

	const glm::mat4& GeViewMatrix();
	const glm::mat4& GetProjectionMatrix();
	glm::mat4 GetViewProjectionMatrix();
	const st::math::frustum& GetFrustum();

private:

	void UpdateMatrices();
	void UpdateWorldViewMatrix();
	void UpdateProjectionMatrix();
	void UpdateProjectionMatrixReverse();
	void UpdateFrustum();

private:

	ProjectionModel m_ProjectionModel;

	glm::vec3 m_Position;
	glm::vec3 m_Forward;
	glm::vec3 m_Up;
	glm::vec3 m_Right;

	float m_VerticalFov;
	float m_zNear;
	float m_zFar;
	float m_Aspect;

	glm::mat4 m_ViewMatrix;
	glm::mat4 m_ProjectionMatrix;

	st::math::frustum m_Frustum;

	bool m_IsDirty;
};

} // namespace st::gfx