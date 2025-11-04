#pragma once

#include <string>
#include <memory>
#include "Core/Math/aabox.h"
#include "RenderAPI/Buffer.h"

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

	void SetIndexBuffer(rapi::BufferHandle indexBuffer) { m_IndexBuffer = indexBuffer; }
	void SetVertexBuffer(rapi::BufferHandle vertexBuffer) { m_VertexBuffer = vertexBuffer; }

	void SetMaterial(std::shared_ptr<Material> mat);

	void SetBounds(const st::math::aabox3f& bounds) { m_Bounds = bounds; }

	rapi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer; }
	rapi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer; }

private:

	std::string m_Name;
	std::string m_SourceFilename; // where this material originated from, e.g. GLTF file name

	st::math::aabox3f m_Bounds;

	rapi::BufferHandle m_IndexBuffer;
	rapi::BufferHandle m_VertexBuffer;

	std::shared_ptr<Material> m_Material;
};

}