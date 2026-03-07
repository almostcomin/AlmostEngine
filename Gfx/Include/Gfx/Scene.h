#pragma once

#include "Core/Memory.h"
#include "Core/Common.h"
#include "RHI/Buffer.h"
#include "Gfx/SceneBounds.h"
#include <map>

namespace st::gfx
{
	class DeviceManager;
}

namespace st::gfx
{
	class SceneGraph;
	class MeshInstance;
	class Mesh;

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
		weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }

		const math::aabox3f GetWorldBounds(BoundsType boundsType) const;

		const AmbientParams& GetAmbientParams() const { return m_AmbientParams; }
		void SetAmbientParams(const AmbientParams& v) { m_AmbientParams = v; }

		const SunParams& GetSunParams() const { return m_SunParams; }
		void SetSunParams(const SunParams& v) { m_SunParams = v; }

		rhi::BufferReadOnlyView GetInstancesBufferView() const;
		rhi::BufferReadOnlyView GetMeshesBufferView() const;
		rhi::BufferReadOnlyView GetMaterialsBufferView() const;
		rhi::BufferReadOnlyView GetPointLightsBufferView() const;

		// Updates scene graph
		void Update();

	private:

		unique<SceneGraph> m_SceneGraph;
		
		AmbientParams m_AmbientParams;
		SunParams m_SunParams;

		rhi::BufferOwner m_InstancesBuffer;		// interop::InstanceData
		rhi::BufferOwner m_MeshesBuffer;		// interop::MeshData
		rhi::BufferOwner m_MaterialsBuffer;		// interop::MaterialData
		rhi::BufferOwner m_PointLightsBuffer;	// interop::PointLightData

		DeviceManager* m_DeviceManager;
	};

}