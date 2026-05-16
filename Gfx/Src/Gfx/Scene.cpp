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
#include "RHI/Device.h"
#include "Interop/RenderResources.h"
#include "Core/unique_vector.h"
#include <cassert>

alm::gfx::Scene::Scene(const std::string& name, DeviceManager* deviceManager) : 
	m_Name{ name }, m_ResetGpuBuffers{ false }, m_DeviceManager{ deviceManager }
{
	m_AmbientParams = AmbientParams{
		.SkyColor = float3{ 0.17f, 0.37f, 0.65f },
		.GroundColor = float3{ 0.62f, 0.58f, 0.55f },
		.Intensity = 0.22f
	};

	m_SunParams = SunParams{
		.ElevationDeg = 60.f,
		.AzimuthDeg = -135.f,
		.Irradiance = 1.f,
		.AngularSizeDeg = 0.53f,
		.Color = float3{ 1.f, 1.f, 1.f },
	};

	m_GpuBuffersHandle = m_DeviceManager->GetGpuSceneBuffers()->RequestSceneHandle(m_Name);
	m_SceneGraph = alm::make_unique_with_weak<alm::gfx::SceneGraph>(m_GpuBuffersHandle, m_DeviceManager->GetGpuSceneBuffers());
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
}

void alm::gfx::Scene::DetachRenderView(alm::weak<RenderView> renderView)
{
	auto it = std::ranges::find(m_RenderViews, renderView);
	if (it != m_RenderViews.end())
	{
		fast_erase(m_RenderViews, it);
	}
	else
	{
		LOG_WARNING("Trying to detach non attached render view '{}'", renderView->GetName());
	}
}

const alm::math::aabox3f alm::gfx::Scene::GetWorldBounds(SceneContentType type) const
{
	if (m_SceneGraph && m_SceneGraph->GetRoot() && has_any_flag(m_SceneGraph->GetRoot()->GetContentFlags(), ToFlag(type)))
	{
		return m_SceneGraph->GetRoot()->GetWorldBounds(type);
	}
	return math::aabox3f::get_empty();
}

alm::rhi::BufferReadOnlyView alm::gfx::Scene::GetInstancesBufferView() const
{
	return m_DeviceManager->GetGpuSceneBuffers()->GetInstancesBufferView(m_GpuBuffersHandle);
}

void alm::gfx::Scene::Update()
{
	if (m_SceneGraph)
	{
		// TODO: Maybe we should merge results to allow multiple updated on single draw.
		m_SceneGraph->Update();
	}
}
