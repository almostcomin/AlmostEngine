#pragma once

#include <string>
#include <memory>
#include <nvrhi/nvrhi.h>
#include "Gfx/Math/aabox.h"

namespace st::gfx
{
	class Material;
};

namespace st::gfx
{

class Mesh
{
public:

	Mesh(const char* name, const char* sourceFilename) :
		m_Name{ name ? name : "<null>" },
		m_SourceFilename{ sourceFilename ? sourceFilename : "<null>" }
	{}

	const st::math::aabox3f& GetAABBox() const { return m_AABBox; }

	void SetIndexBuffer(nvrhi::BufferHandle indexBuffer) { m_IndexBuffer = indexBuffer; }
	void SetVertexBuffer(nvrhi::BufferHandle vertexBuffer) { m_VertexBuffer = vertexBuffer; }

	void SetMaterial(std::shared_ptr<Material> mat);



private:

	std::string m_Name;
	std::string m_SourceFilename; // where this material originated from, e.g. GLTF file name

	st::math::aabox3f m_AABBox;

	nvrhi::BufferHandle m_IndexBuffer;
	nvrhi::BufferHandle m_VertexBuffer;

	std::shared_ptr<Material> m_Material;
};

}