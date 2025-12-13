#include "Gfx/Mesh.h"

st::gfx::Mesh::Mesh(const char* name, const char* sourceFilename) :
	m_Name{ name ? name : "<null>" },
	m_SourceFilename{ sourceFilename ? sourceFilename : "<null>" },
	m_Bounds{ st::math::aabox3f::InitEmpty }
{}

void st::gfx::Mesh::SetVertexBuffer(rapi::BufferHandle vertexBuffer, const VertexStride& stride)
{ 
	m_VertexBuffer = vertexBuffer; 
	m_Stride = stride;
}

void st::gfx::Mesh::SetMaterial(std::shared_ptr<Material> mat)
{
	m_Material = mat;
}

size_t st::gfx::Mesh::GetPrimitiveCount() const
{
	return m_IndexBuffer ? m_IndexBuffer->GetDesc().sizeBytes / m_IndexBuffer->GetDesc().stride : 0u;
}