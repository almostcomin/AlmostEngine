#include "Gfx/GfxPCH.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Material.h"
#include "Gfx/SceneLights.h"
#include "Gfx/RenderView.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/SceneHeightmap.h"
#include "RHI/Device.h"
#include "Interop/RenderResources.h"
#include "Core/unique_vector.h"
#include <cassert>

alm::gfx::Scene::Scene(const std::string& name, DeviceManager* deviceManager) : 
	m_ResetGpuBuffers{ false }, m_Name{ name }, m_DeviceManager{ deviceManager }
{
	m_GpuBuffersHandle = m_DeviceManager->GetGpuSceneBuffers()->RequestSceneHandle(m_Name);

	m_SceneGraph = alm::make_unique_with_weak<alm::gfx::SceneGraph>(m_GpuBuffersHandle, m_DeviceManager->GetGpuSceneBuffers());
	m_SceneGraph->SetRegisterLeafCallback([this](SceneGraphLeaf* leaf)
	{
		OnLeafAdded(leaf);
	});
	m_SceneGraph->SetUnregisterLeafCallback([this](SceneGraphLeaf* leaf)
	{
		OnLeafRemoved(leaf);
	});
}

alm::gfx::Scene::~Scene()
{
	m_SceneGraph.reset();
	m_DeviceManager->GetGpuSceneBuffers()->ReleaseSceneHandle(m_GpuBuffersHandle);
}

void alm::gfx::Scene::AttachRenderView(alm::weak<RenderView> renderView)
{
	assert(std::ranges::find(m_RenderViews, renderView) == m_RenderViews.end());
	m_RenderViews.push_back(renderView);

	const auto& heightmaps = m_SceneGraph->GetSceneHeightmaps();
	for (auto* sceneHeightmap : heightmaps)
	{
		renderView->RegisterHeightmap(sceneHeightmap);
	}
}

void alm::gfx::Scene::DetachRenderView(alm::weak<RenderView> renderView)
{
	auto it = std::ranges::find(m_RenderViews, renderView);
	if (it != m_RenderViews.end())
	{
		const auto& heightmaps = m_SceneGraph->GetSceneHeightmaps();
		for (auto* sceneHeightmap : heightmaps)
		{
			renderView->UnregisterHeightmap(sceneHeightmap);
		}

		fast_erase(m_RenderViews, it);
	}
	else
	{
		LOG_WARNING("Trying to detach non attached render view '{}'", renderView->GetName());
	}
}

const alm::aabox3f alm::gfx::Scene::GetWorldBounds(SceneContentType type)
{
	Update();
	if (m_SceneGraph && m_SceneGraph->GetRoot() && has_any_flag(m_SceneGraph->GetRoot()->GetContentFlags(), ToFlag(type)))
	{
		return m_SceneGraph->GetRoot()->GetWorldBounds(type);
	}
	return aabox3f::get_empty();
}

void alm::gfx::Scene::Update()
{
	if (m_SceneGraph)
	{
		m_SceneGraph->Update();
	}
}

void alm::gfx::Scene::OnLeafAdded(SceneGraphLeaf* leaf)
{
	switch (leaf->GetType())
	{
	case SceneGraphLeaf::Type::Heightmap:
	{
		for (auto& renderView : m_RenderViews)
		{
			renderView->RegisterHeightmap(checked_cast<SceneHeightmap*>(leaf));
		}
	} break;

	default:
		break;
	}
}

void alm::gfx::Scene::OnLeafRemoved(SceneGraphLeaf* leaf)
{
	switch (leaf->GetType())
	{
	case SceneGraphLeaf::Type::Heightmap:
	{
		for (auto& renderView : m_RenderViews)
		{
			renderView->UnregisterHeightmap(checked_cast<SceneHeightmap*>(leaf));
		}
	} break;

	default:
		break;
	}
}