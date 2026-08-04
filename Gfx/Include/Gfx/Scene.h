#pragma once

#include "Core/Memory.h"
#include "Core/Common.h"
#include "Core/Math/aabox.h"
#include "RHI/Buffer.h"
#include "Gfx/SceneFlags.h"
#include "Gfx/GpuSceneBuffersHandle.h"
#include <map>

namespace alm::rhi
{
	class ICommandList;
}

namespace alm::gfx
{
	class DeviceManager;
	class SceneGraph;
	class MeshInstance;
	class Mesh;
	class RenderView;
	class SceneGraphLeaf;
	class AtmosphereConfig;
}

namespace alm::gfx
{

	class Scene : private alm::noncopyable_nonmovable
	{
	public:

		Scene(const std::string& name, DeviceManager* deviceManager);
		~Scene();

		AtmosphereConfig* GetAtmosphereConfig() { return m_AtmosConfig.get(); }

		alm::weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }
		
		GpuSceneBuffersHandle GetGpuSceneBuffersHandle() const { return m_GpuBuffersHandle; }
		
		void AttachRenderView(alm::weak<RenderView> renderView);
		void DetachRenderView(alm::weak<RenderView> renderView);

		const aabox3f GetWorldBounds(SceneContentType type);

		// Updates scene graph
		void Update(float elapsedSec);

		void RefreshSceneGraph();
		void ResetGpuBuffers() { m_ResetGpuBuffers = true; }

	private:

		void OnLeafAdded(SceneGraphLeaf* leaf);
		void OnLeafRemoved(SceneGraphLeaf* leaf);

	private:

		alm::unique<SceneGraph> m_SceneGraph;		
		std::vector<alm::weak<RenderView>> m_RenderViews;

		alm::unique<AtmosphereConfig> m_AtmosConfig;

		GpuSceneBuffersHandle m_GpuBuffersHandle;
		bool m_ResetGpuBuffers;

		std::string m_Name;
		DeviceManager* m_DeviceManager;
	};

}