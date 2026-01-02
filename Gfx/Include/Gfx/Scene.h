#pragma once

#include "Core/Memory.h"
#include "Core/Common.h"
#include "RHI/Buffer.h"
#include <map>

namespace st::gfx
{
	class DeviceManager;
}

namespace st::gfx
{
	class SceneGraph;
	class MeshInstance;

	class Scene : private st::noncopyable_nonmovable
	{
	public:

		Scene(DeviceManager* deviceManager);
		~Scene();

		void SetSceneGraph(unique<SceneGraph>&& graph);

		void Update();

		rhi::DescriptorIndex GetInstancesBufferDI() const;
		rhi::DescriptorIndex GetMeshesBufferDI() const;

		int GetInstanceIndex(const st::gfx::MeshInstance* pInstance);

		weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }

	private:

		unique<SceneGraph> m_SceneGraph;
		
		// For a mesh instance pointer, index in the m_InstancesBuffer of that instance
		std::map<const st::gfx::MeshInstance*, size_t> m_InstanceIndices;

		rhi::BufferOwner m_InstancesBuffer;
		rhi::BufferOwner m_MeshesBuffer;

		DeviceManager* m_DeviceManager;
	};

}