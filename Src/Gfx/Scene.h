#pragma once

#include "Core/Memory.h"
#include "Core/Util.h"
#include "RenderAPI/Buffer.h"
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

		rapi::DescriptorIndex GetInstancesBufferDI() const;
		rapi::DescriptorIndex GetMeshesBufferDI() const;

		int GetInstanceIndex(const st::gfx::MeshInstance* pInstance);

		weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }

	private:

		unique<SceneGraph> m_SceneGraph;
		
		std::map<const st::gfx::MeshInstance*, size_t> m_InstanceIndices;

		rapi::BufferHandle m_InstancesBuffer;
		rapi::BufferHandle m_MeshesBuffer;

		DeviceManager* m_DeviceManager;
	};

}