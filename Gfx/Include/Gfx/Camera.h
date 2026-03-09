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
	
	void SetUpRef(const float3& v);

	void SetVerticalFov(float vFov);
	void SetAspect(float aspect);
	void SetZNear(float zNear);
	void SetZFar(float zFar);

	void SetProjectionModel(ProjectionModel model);

	void Fit(const st::math::aabox3f& bounds);

	const float3& GetPosition() const { return m_Position; }
	const float3& GetForward() const { return m_Forward; }
	const float3& GetRight() const { return m_Right; }
	const float3& GetUp() const { return m_Up; }
	const float3& GetUpRef() const { return m_UpRef; }
	float GetVerticalFOV() const { return m_VerticalFov; }
	float GetZNear() const { return m_zNear; }

	const float4x4& GetViewMatrix();
	const float4x4& GetProjectionMatrix();
	float4x4 GetViewProjectionMatrix();
	const st::math::frustum3f& GetFrustum();

	float GetYaw() const;
	float GetPitch() const;
	float GetRoll() const;

	void SetYaw(float yaw);
	void SetPitch(float pich);
	void SetRoll(float roll);

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

	float3 m_UpRef;

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