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

		float GetAmbientIntensity() const { return m_AmbientIntensity; }
		const float3& GetSkyColor() const { return m_SkyColor; }
		const float3& GetGroundColor() const { return m_GroundColor; }

		void SetAmbientIntensity(float v) { m_AmbientIntensity = v; }
		void SetSkyColor(const float3& v) { m_SkyColor = v; }
		void SetGroundColor(const float3& v) { m_GroundColor = v; }

		rhi::DescriptorIndex GetInstancesBufferDI() const;
		rhi::DescriptorIndex GetMeshesBufferDI() const;
		rhi::DescriptorIndex GetMaterialsBufferDI() const;

		// Resturs the index in the instances buffer (GetInstancesBufferDI) of the pInstance instance
		int GetInstanceIndex(const st::gfx::MeshInstance* pInstance);

		void Update();

		weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }

	private:

		unique<SceneGraph> m_SceneGraph;
		
		float m_AmbientIntensity;
		float3 m_SkyColor;
		float3 m_GroundColor;

		// For a mesh instance pointer, index in the m_InstancesBuffer of that instance
		std::map<const st::gfx::MeshInstance*, size_t> m_InstanceIndices;

		rhi::BufferOwner m_InstancesBuffer;
		rhi::BufferOwner m_MeshesBuffer;
		rhi::BufferOwner m_MaterialsBuffer;

		DeviceManager* m_DeviceManager;
	};

}