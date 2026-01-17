#pragma once

#include "Core/Math/frustum.h"
#include "Core/Math/aabox.h"

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

	void SetPosition(const float3& pos);
	void SetForward(const float3& dir);
	void LookAt(const float3& pos);
	
	void SetVerticalFov(float vFov);
	void SetAspect(float aspect);
	void SetZNear(float zNear);
	void SetZFar(float zFar);

	void SetProjectionModel(ProjectionModel model);

	void Fit(const st::math::aabox3f& bbox);

	const float3& GetPosition() const { return m_Position; }
	const float3& GetForward() const { return m_Forward; }
	const float3& GetRight() const { return m_Right; }
	const float3& GetUp() const { return m_Up; }
	float GetVerticalFOV() const { return m_VerticalFov; }
	float GetZNear() const { return m_zNear; }

	const float4x4& GeViewMatrix();
	const float4x4& GetProjectionMatrix();
	float4x4 GetViewProjectionMatrix();
	const st::math::frustum3f& GetFrustum();

private:

	void UpdateMatrices();
	void UpdateWorldViewMatrix();
	void UpdateProjectionMatrix();
	void UpdateProjectionMatrixReverse();
	void UpdateFrustum();

private:

	ProjectionModel m_ProjectionModel;

	float3 m_Position;
	float3 m_Forward;
	float3 m_Up;
	float3 m_Right;

	float m_VerticalFov;
	float m_zNear;
	float m_zFar;
	float m_Aspect;

	float4x4 m_ViewMatrix;
	float4x4 m_ProjectionMatrix;

	st::math::frustum3f m_Frustum;

	bool m_IsDirty;
};

} // namespace st::gfx