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

	struct VertexStride
	{
		uint32_t Vertex = UINT32_MAX;
		uint32_t Position = UINT32_MAX;
		uint32_t Normal = UINT32_MAX;
		uint32_t Tangent = UINT32_MAX;
		uint32_t TexCoord0 = UINT32_MAX;
		uint32_t TexCoord1 = UINT32_MAX;
		uint32_t Color = UINT32_MAX;
	};

	Mesh(const char* name, const char* sourceFilename);

	const st::math::aabox3f& GetBounds() const { return m_Bounds; }

	void SetIndexBuffer(rapi::BufferHandle indexBuffer) { m_IndexBuffer = indexBuffer; }
	void SetVertexBuffer(rapi::BufferHandle vertexBuffer, const VertexStride& stride);

	void SetMaterial(std::shared_ptr<Material> mat);

	void SetBounds(const st::math::aabox3f& bounds) { m_Bounds = bounds; }

	rapi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer; }
	size_t GetPrimitiveCount() const;

	rapi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer; }
	const VertexStride& GetVertexStride() const { return m_Stride; }

	std::shared_ptr<Material> GetMaterial() const { return m_Material; }

private:

	std::string m_Name;
	std::string m_SourceFilename; // where this material originated from, e.g. GLTF file name

	st::math::aabox3f m_Bounds;

	rapi::BufferHandle m_IndexBuffer;
	rapi::BufferHandle m_VertexBuffer;
	VertexStride m_Stride;

	std::shared_ptr<Material> m_Material;
};

}