#include "Gfx/Mesh.h"
#include "RenderAPI/Device.h"

st::gfx::Mesh::Mesh(rapi::Device* device, const char* name, const char* sourceFilename) :
	m_Device{ device },
	m_Name{ name ? name : "<null>" },
	m_SourceFilename{ sourceFilename ? sourceFilename : "<null>" },
	m_Bounds{ st::math::aabox3f::InitEmpty },
	m_PrimitiveTopo{ rapi::PrimitiveTopology::TriangleList }
{}

st::gfx::Mesh::~Mesh()
{
	m_Device->ReleaseQueued(m_IndexBuffer);
	m_Device->ReleaseQueued(m_VertexBuffer);
}

void st::gfx::Mesh::SetIndexBuffer(rapi::BufferHandle indexBuffer, rapi::PrimitiveTopology topo)
{
	m_IndexBuffer = indexBuffer;
	m_PrimitiveTopo = topo;
}

void st::gfx::Mesh::SetVertexBuffer(rapi::BufferHandle vertexBuffer, const VertexStride& stride)
{ 
	m_VertexBuffer = vertexBuffer; 
	m_VertexStride = stride;
}

void st::gfx::Mesh::SetMaterial(std::shared_ptr<Material> mat)
{
	m_Material = mat;
}

size_t st::gfx::Mesh::GetIndexCount() const
{
	return m_IndexBuffer ? m_IndexBuffer->GetDesc().sizeBytes / m_IndexBuffer->GetDesc().stride : 0u;
}