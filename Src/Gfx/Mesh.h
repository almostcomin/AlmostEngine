#pragma once

#include <string>
#include <memory>
#include "Core/Math/aabox.h"
#include "RHI/Buffer.h"
#include "RHI/Common.h"

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

	Mesh(rhi::Device* device, const char* name, const char* sourceFilename);
	~Mesh();

	const st::math::aabox3f& GetBounds() const { return m_Bounds; }

	void SetIndexBuffer(rhi::BufferHandle indexBuffer, rhi::PrimitiveTopology topo);
	void SetVertexBuffer(rhi::BufferHandle vertexBuffer, const VertexStride& stride);

	void SetMaterial(std::shared_ptr<Material> mat);

	void SetBounds(const st::math::aabox3f& bounds) { m_Bounds = bounds; }

	rhi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer; }
	size_t GetIndexCount() const;
	rhi::PrimitiveTopology GetPrimitiveTopology() const { return m_PrimitiveTopo; }

	rhi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer; }
	const VertexStride& GetVertexStride() const { return m_VertexStride; }

	std::shared_ptr<Material> GetMaterial() const { return m_Material; }

private:

	std::string m_Name;
	std::string m_SourceFilename; // where this material originated from, e.g. GLTF file name

	st::math::aabox3f m_Bounds;

	rhi::BufferHandle m_IndexBuffer;
	rhi::PrimitiveTopology m_PrimitiveTopo;
	rhi::BufferHandle m_VertexBuffer;
	VertexStride m_VertexStride;

	std::shared_ptr<Material> m_Material;

	rhi::Device* m_Device;
};

}