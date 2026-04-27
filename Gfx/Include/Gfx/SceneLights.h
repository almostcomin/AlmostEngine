#pragma once

#include "Gfx/SceneGraphLeaf.h"

namespace alm::gfx
{

class SceneLight : public SceneGraphLeaf
{
public:

	enum class LightType
	{
		Directional,
		Point,
		Spot
	};

public:

	SceneLight() : m_Name{ "<noname>" }, m_Color{ 1.f }
	{}
	
	virtual LightType	GetLightType() const = 0;

	// Setters
	void				SetName(const std::string& name)		{ m_Name = name; }
	void				SetColor(const float3& c)				{ m_Color = c; }

	// Getters
	const std::string&	GetName() const							{ return m_Name; }
	const float3&		GetColor() const						{ return m_Color; }

protected:

	std::string m_Name;
	float3 m_Color;
};

class SceneDirectionalLight : public SceneLight
{
public:

	SceneDirectionalLight() = default;

	// Overrides
	bool				HasBounds()			const override	{ return false; }
	SceneContentFlags	GetContentFlags()	const override	{ return alm::gfx::SceneContentFlags::DirectionalLights; }
	Type				GetType()			const override	{ return Type::DirectionalLight; }
	LightType			GetLightType()		const override	{ return LightType::Directional; }

	// Setters
	void				SetIrradiance(float v)				{ m_Irradiance = v; }
	void				SetAngularSize(float v)				{ m_AngularSize = v; }

	// Getters
	float3				GetDirection() const;
	float				GetIrradiance() const				{ return m_Irradiance; }
	float				GetAngularSize() const				{ return m_AngularSize; }

private:

	float				m_Irradiance; // Lux (lumen per square meter)
	float				m_AngularSize;
};

class ScenePointLight : public SceneLight
{
public:

	ScenePointLight();

	// Overrides
	bool				HasBounds()			const override	{ return true; }
	const alm::math::aabox3f& GetBounds()	const override	{ return m_Bounds; }
	SceneContentFlags	GetContentFlags()	const override	{ return alm::gfx::SceneContentFlags::PointLights; }
	Type				GetType()			const override	{ return Type::PointLight; }
	LightType			GetLightType()		const override	{ return LightType::Point; }

	// Setters
	void				SetIntensity(float v)				{ m_Intensity = v; }
	void				SetRange(float v);
	void				SetRadius(float v)					{ m_Radius = v; }

	// Getters
	float				GetIntensity() const				{ return m_Intensity; }
	float				GetRange() const					{ return m_Range; }
	float				GetRadius() const					{ return m_Radius; }

private:

	float m_Intensity; // Candela (lumen per steradian)
	float m_Range;
	float m_Radius;

	alm::math::aabox3f m_Bounds;
};

class SceneSpotLight : public SceneLight
{
public:

	SceneSpotLight();

	// Overrides
	bool				HasBounds()			const override	{ return true; }
	const alm::math::aabox3f& GetBounds()	const override	{ return m_Bounds; }
	SceneContentFlags	GetContentFlags()	const override	{ return alm::gfx::SceneContentFlags::SpotLights; }
	Type				GetType()			const override	{ return Type::SpotLight; }
	LightType			GetLightType()		const override	{ return LightType::Spot; }

	// Setters
	void				SetIntensity(float v)				{ m_Intensity = v; }
	void				SetRange(float v);
	void				SetRadius(float v)					{ m_Radius = v; }
	void				SetInnerConeAngle(float v)			{ m_InnerAngle = v; }
	void				SetOuterConeAngle(float v)			{ m_OuterAngle = v; }

	// Getters
	float3				GetDirection() const;
	float				GetIntensity() const				{ return m_Intensity; }
	float				GetRange() const					{ return m_Range; }
	float				GetRadius() const					{ return m_Radius; }
	float				GetInnerConeAngle() const			{ return m_InnerAngle; }
	float				GetOuterConeAngle() const			{ return m_OuterAngle; }

private:

	float m_Intensity; // Candela (lumen per steradian)
	float m_Range;
	float m_Radius;
	float m_InnerAngle;
	float m_OuterAngle;

	alm::math::aabox3f m_Bounds;
};

} // namespace st::gfx