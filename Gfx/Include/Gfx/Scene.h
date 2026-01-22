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

		struct AmbientParams
		{
			float3 SkyColor;
			float3 GroundColor;
			float Intensity;
		};

		struct SunParams
		{
			float ElevationDeg;
			float AzimuthDeg;
			float Irradiance;
			float AngularSizeDeg;
			float3 Color;
		};

	public:

		Scene(DeviceManager* deviceManager);
		~Scene();

		void SetSceneGraph(unique<SceneGraph>&& graph);

		const AmbientParams& GetAmbientParams() const { return m_AmbientParams; }
		void SetAmbientParams(const AmbientParams& v) { m_AmbientParams = v; }

		const SunParams& GetSunParams() const { return m_SunParams; }
		void SetSunParams(const SunParams& v) { m_SunParams = v; }

		rhi::DescriptorIndex GetInstancesBufferDI() const;
		rhi::DescriptorIndex GetMeshesBufferDI() const;
		rhi::DescriptorIndex GetMaterialsBufferDI() const;

		// Resturs the index in the instances buffer (GetInstancesBufferDI) of the pInstance instance
		int GetInstanceIndex(const st::gfx::MeshInstance* pInstance);

		void Update();

		weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }

	private:

		unique<SceneGraph> m_SceneGraph;
		
		AmbientParams m_AmbientParams;
		SunParams m_SunParams;

		// For a mesh instance pointer, index in the m_InstancesBuffer of that instance
		std::map<const st::gfx::MeshInstance*, size_t> m_InstanceIndices;

		rhi::BufferOwner m_InstancesBuffer;
		rhi::BufferOwner m_MeshesBuffer;
		rhi::BufferOwner m_MaterialsBuffer;

		DeviceManager* m_DeviceManager;
	};

}