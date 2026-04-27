#pragma once

#include "Core/Memory.h"
#include "Core/Common.h"
#include "Core/Math/aabox.h"
#include "RHI/Buffer.h"
#include "Gfx/SceneContentFlags.h"
#include <map>

namespace alm::gfx
{
	class DeviceManager;
}

namespace alm::gfx
{
	class SceneGraph;
	class MeshInstance;
	class Mesh;

	class Scene : private alm::noncopyable_nonmovable
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
		weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }
		void RefreshSceneGraph();

		const math::aabox3f GetWorldBounds(SceneContentType type) const;

		const AmbientParams& GetAmbientParams() const { return m_AmbientParams; }
		void SetAmbientParams(const AmbientParams& v) { m_AmbientParams = v; }

		const SunParams& GetSunParams() const { return m_SunParams; }
		void SetSunParams(const SunParams& v) { m_SunParams = v; }

		rhi::BufferReadOnlyView GetInstancesBufferView() const;
		rhi::BufferReadOnlyView GetMeshesBufferView() const;
		rhi::BufferReadOnlyView GetMaterialsBufferView() const;

		// Updates scene graph
		void Update();

	private:

		unique<SceneGraph> m_SceneGraph;
		
		AmbientParams m_AmbientParams;
		SunParams m_SunParams;

		rhi::BufferOwner m_InstancesBuffer;		// interop::InstanceData
		rhi::BufferOwner m_MeshesBuffer;		// interop::MeshData
		rhi::BufferOwner m_MaterialsBuffer;		// interop::MaterialData

		DeviceManager* m_DeviceManager;
	};

}