#include "Gfx/GfxPCH.h"
#include "Gfx/Mesh.h"
#include "RHI/Device.h"

alm::gfx::Mesh::Mesh(rhi::Device* device, const char* name, const char* sourceFilename) :
	m_Device{ device },
	m_Name{ name ? name : "<null>" },
	m_SourceFilename{ sourceFilename ? sourceFilename : "<null>" },
	m_Bounds{ alm::math::aabox3f::InitEmpty },
	m_PrimitiveTopo{ rhi::PrimitiveTopology::TriangleList },
	m_IndexSize{ 0 }
{}

alm::gfx::Mesh::~Mesh()
{
	m_Device->ReleaseQueued(std::move(m_IndexBuffer));
	m_Device->ReleaseQueued(std::move(m_VertexBuffer));
}

void alm::gfx::Mesh::SetIndexBuffer(rhi::BufferOwner&& indexBuffer, rhi::PrimitiveTopology topo, uint8_t indexSize)
{
	m_IndexBuffer = std::move(indexBuffer);
	m_PrimitiveTopo = topo;
	m_IndexSize = indexSize;
}

void alm::gfx::Mesh::SetVertexBuffer(rhi::BufferOwner&& vertexBuffer, const VertexFormat& fmt)
{ 
	m_VertexBuffer = std::move(vertexBuffer);
	m_VertexFormat = fmt;
}

void alm::gfx::Mesh::SetMaterial(std::shared_ptr<Material> mat)
{
	m_Material = mat;
}

size_t alm::gfx::Mesh::GetIndexCount() const
{
	return m_IndexBuffer ? m_IndexBuffer->GetDesc().sizeBytes / m_IndexBuffer->GetDesc().stride : 0u;
}