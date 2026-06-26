#pragma once

#include "Core/Math/frustum.h"
#include "Core/Math/aabox.h"

namespace alm::gfx
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

	void Fit(const alm::aabox3f& bounds);

	const float3& GetPosition() const { return m_Position; }
	const float3& GetForward() const { return m_Forward; }
	const float3& GetRight() const { return m_Right; }
	const float3& GetUp() const { return m_Up; }
	const float3& GetUpRef() const { return m_UpRef; }
	float GetVerticalFOV() const { return m_VerticalFov; }
	float GetZNear() const { return m_zNear; }

	const float4x4& GetViewMatrix() const;
	const float4x4& GetProjectionMatrix() const;
	float4x4 GetViewProjectionMatrix() const;
	
	// Inverse view-projection matrix but removing view translation
	float4x4 GetClipToTranslatedWorldMatrix();

	const alm::math::frustum3f& GetFrustum() const;

	float GetYaw() const;
	float GetPitch() const;
	float GetRoll() const;

	void SetYaw(float yaw);
	void SetPitch(float pich);
	void SetRoll(float roll);

	float3 ScreenToWorld(const uint2& pixelPos, float linearDepth, const uint2& viewportSize) const;

	// World -> NDC (D3D/Vulkan). ndc.xy E [-1, +1], ndc.z E [0,1] (standard) o [1,0] (reverse-Z).
	// The returned .w component is pre-division. If w <= 0 point is behind near plane
	float4 WorldToNDC(const float3& worldPos) const;

	// Returns <visible, pixel_coords>
	std::pair<bool, uint2> WorldToPixel(const float3& worldPos, const uint2& viewportSize) const;

private:

	void UpdateMatrices() const;
	void UpdateWorldViewMatrix() const;
	void UpdateProjectionMatrix() const;
	void UpdateProjectionMatrixReverse() const;
	void UpdateFrustum() const;

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

	mutable float4x4 m_ViewMatrix;
	mutable float4x4 m_ProjectionMatrix;

	mutable alm::math::frustum3f m_Frustum;

	mutable bool m_IsDirty;
};

} // namespace st::gfx