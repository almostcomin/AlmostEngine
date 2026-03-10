#pragma once

#include "Gfx/SceneGraphLeaf.h"

namespace st::gfx
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
	SceneContentFlags	GetContentFlags()	const override	{ return st::gfx::SceneContentFlags::DirectionalLights; }
	Type				GetType()			const override	{ return Type::DirectionalLight; }
	LightType			GetLightType()		const override	{ return LightType::Directional; }

	// Setters
	void				SetIrradiance(float v)				{ m_Irradiance = v; }
	void				SetDirection(const float3& dir)		{ m_Direction = dir; }
	void				SetAngularSize(float v)				{ m_AngularSize = v; }

	// Getters
	float				GetIrradiance() const				{ return m_Irradiance; }
	float3				GetDirection() const				{ return m_Direction; }
	float				GetAngularSize() const				{ return m_AngularSize; }

private:

	float				m_Irradiance; // Lux (lumen per square meter)
	float3				m_Direction;
	float				m_AngularSize;
};

class ScenePointLight : public SceneLight
{
public:

	ScenePointLight();

	// Overrides
	bool				HasBounds()			const override	{ return true; }
	BoundsType			GetBoundsType()		const override	{ return BoundsType::Light; }
	const st::math::aabox3f& GetBounds()	const override	{ return m_Bounds; }
	SceneContentFlags	GetContentFlags()	const override	{ return st::gfx::SceneContentFlags::PointLights; }
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
	st::math::aabox3f m_Bounds;

};

} // namespace st::gfx