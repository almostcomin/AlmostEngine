#pragma once

#include <string>
#include <memory>
#include <nvrhi/nvrhi.h>
#include "Core/Math/aabox.h"

namespace st::gfx
{
	class Material;
};

namespace st::gfx
{

class Mesh
{
public:

	Mesh(const char* name, const char* sourceFilename);

	const st::math::aabox3f& GetBounds() const { return m_Bounds; }

	void SetIndexBuffer(nvrhi::BufferHandle indexBuffer) { m_IndexBuffer = indexBuffer; }
	void SetVertexBuffer(nvrhi::BufferHandle vertexBuffer) { m_VertexBuffer = vertexBuffer; }

	void SetMaterial(std::shared_ptr<Material> mat);

	void SetBounds(const st::math::aabox3f& bounds) { m_Bounds = bounds; }

	nvrhi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer; }
	nvrhi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer; }

private:

	std::string m_Name;
	std::string m_SourceFilename; // where this material originated from, e.g. GLTF file name

	st::math::aabox3f m_Bounds;

	nvrhi::BufferHandle m_IndexBuffer;
	nvrhi::BufferHandle m_VertexBuffer;

	std::shared_ptr<Material> m_Material;
};

}