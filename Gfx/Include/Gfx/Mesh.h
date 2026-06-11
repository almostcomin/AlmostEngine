#pragma once

#include <string>
#include <memory>
#include "Core/Math/aabox.h"
#include "RHI/Buffer.h"
#include "RHI/Common.h"

namespace alm::gfx
{
	class Material;
	struct TerrainMaterial;
};

namespace alm::gfx
{

class Mesh
{
public:

	struct VertexFormat
	{
		uint32_t VertexStride = UINT32_MAX;
		uint32_t PositionOffset = UINT32_MAX;
		uint32_t NormalOffset = UINT32_MAX;
		uint32_t TangentOffset = UINT32_MAX;
		uint32_t TexCoord0Offset = UINT32_MAX;
		uint32_t TexCoord1Offset = UINT32_MAX;
		uint32_t ColorOffset = UINT32_MAX;
	};

	Mesh(rhi::Device* device, const char* name, const char* sourceFilename);
	~Mesh();

	const alm::aabox3f& GetBounds() const { return m_Bounds; }

	void SetIndexBuffer(std::shared_ptr<rhi::BufferOwner> indexBuffer, rhi::PrimitiveTopology topo, uint8_t indexSize);
	void SetVertexBuffer(std::shared_ptr<rhi::BufferOwner> vertexBuffer, const VertexFormat& fmt);

	void SetMaterial(std::shared_ptr<Material> mat);
	void SetTerrainMaterial(std::shared_ptr<TerrainMaterial> mat);

	void SetBounds(const alm::aabox3f& bounds) { m_Bounds = bounds; }

	rhi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer ? m_IndexBuffer->get_weak() : rhi::BufferHandle{}; }
	size_t GetIndexCount() const;
	rhi::PrimitiveTopology GetPrimitiveTopology() const { return m_PrimitiveTopo; }
	uint8_t GetIndexSize() const { return m_IndexSize; }

	rhi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer ? m_VertexBuffer->get_weak() : rhi::BufferHandle{}; }
	const VertexFormat& GetVertexFormat() const { return m_VertexFormat; }

	std::shared_ptr<Material> GetMaterial() const { return m_Material; }
	std::shared_ptr<TerrainMaterial> GetTerrainMaterial() const { return m_TerrainMaterial; }

	const std::string& GetName() const { return m_Name; }

private:

	std::string m_Name;
	std::string m_SourceFilename; // where this material originated from, e.g. GLTF file name

	alm::aabox3f m_Bounds;

	std::shared_ptr<rhi::BufferOwner> m_IndexBuffer;
	rhi::PrimitiveTopology m_PrimitiveTopo;
	uint8_t m_IndexSize;

	std::shared_ptr<rhi::BufferOwner> m_VertexBuffer;
	VertexFormat m_VertexFormat;

	std::shared_ptr<Material> m_Material;
	std::shared_ptr<TerrainMaterial> m_TerrainMaterial;

	rhi::Device* m_Device;
};

}