#include "Gfx/GfxPCH.h"
#include "Gfx/Mesh.h"
#include "RHI/Device.h"

alm::gfx::Mesh::Mesh(rhi::Device* device, const char* name, const char* sourceFilename) :
	m_Name{ name ? name : "<null>" },
	m_SourceFilename{ sourceFilename ? sourceFilename : "<null>" },
	m_Bounds{ alm::aabox3f::InitEmpty },
	m_PrimitiveTopo{ rhi::PrimitiveTopology::TriangleList },
	m_IndexSize{ 0 },
	m_Device{ device }
{}

alm::gfx::Mesh::~Mesh()
{}

void alm::gfx::Mesh::SetIndexBuffer(std::shared_ptr<rhi::BufferOwner> indexBuffer, rhi::PrimitiveTopology topo, uint8_t indexSize)
{
	m_IndexBuffer = indexBuffer;
	m_PrimitiveTopo = topo;
	m_IndexSize = indexSize;
}

void alm::gfx::Mesh::SetVertexBuffer(std::shared_ptr<rhi::BufferOwner> vertexBuffer, const VertexFormat& fmt)
{ 
	m_VertexBuffer = vertexBuffer;
	m_VertexFormat = fmt;
}

void alm::gfx::Mesh::SetMaterial(std::shared_ptr<Material> mat)
{
	m_Material = mat;
}

void alm::gfx::Mesh::SetTerrainMaterial(std::shared_ptr<TerrainMaterial> mat)
{
	m_TerrainMaterial = mat;
}

size_t alm::gfx::Mesh::GetIndexCount() const
{
	return m_IndexBuffer ? (*m_IndexBuffer)->GetDesc().sizeBytes / (*m_IndexBuffer)->GetDesc().stride : 0u;
}