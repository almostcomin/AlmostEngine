#include "Gfx/GfxPCH.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/GpuSceneBuffers.h"

alm::gfx::MeshInstance::MeshInstance(std::shared_ptr<alm::gfx::Mesh> mesh) :
	m_Mesh{ mesh },
	m_MeshSceneIndex{ UINT32_MAX },
	m_InstanceFlags{ Flags::Default }
{}

alm::gfx::MeshInstance::~MeshInstance()
{}

bool alm::gfx::MeshInstance::HasBounds() const
{
	return m_Mesh ? true : false;
}

const alm::aabox3f& alm::gfx::MeshInstance::GetBounds() const
{
	return m_Mesh ? m_Mesh->GetBounds() : alm::aabox3f::get_empty();
}

void alm::gfx::MeshInstance::SetInstanceFlags(Flags flags)
{
	m_InstanceFlags = flags;
}

alm::gfx::SceneContentFlags alm::gfx::MeshInstance::GetContentFlags() const
{ 
	SceneContentFlags flags = SceneContentFlags::Meshes;

	if (has_any_flag(m_InstanceFlags, Flags::CastShadows))
		flags |= SceneContentFlags::ShadowCasters;

	return flags;
}

void alm::gfx::MeshInstance::CollectDrawInfos(const VisibleSetContext&, const GpuSceneBuffers* gpuSceneBuffers, std::vector<RenderableDrawInfo>& out) const
{
	out.emplace_back(
		m_Mesh->GetMaterial()->GetDomain(),		// MaterialDomain
		m_Mesh->GetMaterial()->GetCullMode(),	// CullMode
		(uintptr_t)(m_Mesh.get()),				// BatchKey
		GetLeafSceneIndex(),					// InstanceIdx
		m_MeshSceneIndex,						// MeshIndex
		gpuSceneBuffers->GetMaterialIndexFromMeshIdx(m_MeshSceneIndex), // MaterialIndex
		UINT32_MAX,								// TransientBaseIndex (not used)
		m_Mesh->GetIndexCount()					// IndexCount
	);
}