#pragma once

#include <glm/ext/vector_float3.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace st::gfx
{

class Transform
{
public:

	Transform() :
		m_Scale{ 1.f }, 
		m_Rotation{ glm::identity<glm::quat>() }, 
		m_Translation{ 0.f },
		m_Dirty{ false },
		m_Matrix{ glm::identity<glm::mat4x4>() }
	{}

	Transform(const glm::vec3& scale, const glm::quat rot, const glm::vec3 translation) :
		m_Scale(scale), 
		m_Rotation(rot), 
		m_Translation(translation), 
		m_Dirty{ true }
	{}

	// Init from an affine matrix. Warning: m must be in column-major order
	Transform(const glm::mat4x4& m) :
		m_Scale{ GetScale(m) },
		m_Rotation{ GetRotation(m) },
		m_Translation{ GetTranslation(m) },
		m_Dirty{ true }
	{}

	const glm::vec3 GetScale() const 
	{
		return m_Scale;
	}

	const glm::quat GetRotation() const 
	{
		return m_Rotation;
	}

	const glm::vec3 GetTranslation() const 
	{
		return m_Translation;
	}

	// Returns composed matrix. Warning: Column-major
	const glm::mat4x4& GetMatrix() 
	{
		if (m_Dirty)
		{
			m_Matrix = glm::scale(glm::identity<glm::mat4x4>(), m_Scale);
			m_Matrix *= glm::mat4_cast(m_Rotation);
			m_Matrix = glm::translate(m_Matrix, m_Translation);
			m_Dirty = false;
		}
		return m_Matrix;
	}

	Transform& SetScale(const glm::vec3& scale)
	{
		m_Scale = scale;
		m_Dirty = true;
		return *this;
	}

	Transform& SetRotation(const glm::quat& rot)
	{
		m_Rotation = rot;
		m_Dirty = true;
		return *this;
	}

	Transform& SetTranslation(const glm::vec3& t)
	{
		m_Translation = t;
		m_Dirty = true;
		return *this;
	}

	static glm::vec3 GetScale(const glm::mat4& m)
	{
		return {
			glm::length(glm::vec3(m[0])),
			glm::length(glm::vec3(m[1])),
			glm::length(glm::vec3(m[2]))};
	}

	static glm::quat GetRotation(const glm::mat4& m)
	{
		glm::vec3 scale = GetScale(m);
		glm::mat3 rotMat(
			glm::vec3(m[0]) / scale.x,
			glm::vec3(m[1]) / scale.y,
			glm::vec3(m[2]) / scale.z
		);
		return glm::quat_cast(rotMat);
	}

	static glm::vec3 GetTranslation(const glm::mat4& m)
	{
		return glm::vec3(m[3]);
	}

private:

	glm::vec3 m_Scale;
	glm::quat m_Rotation;
	glm::vec3 m_Translation;
	
	bool m_Dirty;
	glm::mat4x4 m_Matrix; // Column major
};

}