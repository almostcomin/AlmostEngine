#pragma once

#include <string>
#include <memory>
#include "Core/Math/aabox.h"
#include "RHI/Buffer.h"
#include "RHI/Common.h"
#include "Gfx/MaterialRef.h"

namespace alm::gfx
{
	class MaterialRef;
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

	void SetMaterial(MaterialRef mat);
	void SetTerrainMaterial(std::shared_ptr<TerrainMaterial> mat);

	void SetBounds(const alm::aabox3f& bounds) { m_Bounds = bounds; }

	rhi::BufferHandle GetIndexBuffer() const { return m_IndexBuffer ? m_IndexBuffer->get_weak() : rhi::BufferHandle{}; }
	size_t GetIndexCount() const;
	rhi::PrimitiveTopology GetPrimitiveTopology() const { return m_PrimitiveTopo; }
	uint8_t GetIndexSize() const { return m_IndexSize; }

	rhi::BufferHandle GetVertexBuffer() const { return m_VertexBuffer ? m_VertexBuffer->get_weak() : rhi::BufferHandle{}; }
	const VertexFormat& GetVertexFormat() const { return m_VertexFormat; }

	MaterialRef GetMaterialRef() const { return m_MaterialRef; }
	Material* GetMaterial() const { return m_MaterialRef.GetMaterial(); }

	std::shared_ptr<TerrainMaterial> GetTerrainMaterial() const { return m_TerrainMaterial; }

	void SetCpuPositions(std::vector<float3>&& posVec) { m_CpuPositions = std::move(posVec); m_CpuPositions.shrink_to_fit(); }
	void SetCpuIndices(std::vector<uint32_t>&& indicesVec) { m_CpuIndices = std::move(indicesVec); m_CpuIndices.shrink_to_fit(); }

	bool HasCpuGeometry() const { return !m_CpuPositions.empty() && !m_CpuIndices.empty(); }
	const std::vector<float3>& GetCpuPositions() const { return m_CpuPositions; }
	const std::vector<uint32_t>& GetCpuIndices() const { return m_CpuIndices; }

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

	MaterialRef m_MaterialRef;
	std::shared_ptr<TerrainMaterial> m_TerrainMaterial;

	std::vector<float3> m_CpuPositions;
	std::vector<uint32_t> m_CpuIndices;

	rhi::Device* m_Device;
};

}