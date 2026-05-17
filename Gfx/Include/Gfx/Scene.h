#pragma once

#include "Core/Memory.h"
#include "Core/Common.h"
#include "Core/Math/aabox.h"
#include "RHI/Buffer.h"
#include "Gfx/SceneContentFlags.h"
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
}

namespace alm::gfx
{

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

		Scene(const std::string& name, DeviceManager* deviceManager);
		~Scene();

		alm::weak<SceneGraph> GetSceneGraph() const { return m_SceneGraph.get_weak(); }
		
		void AttachRenderView(alm::weak<RenderView> renderView);
		void DetachRenderView(alm::weak<RenderView> renderView);

		const aabox3f GetWorldBounds(SceneContentType type) const;

		const AmbientParams& GetAmbientParams() const { return m_AmbientParams; }
		void SetAmbientParams(const AmbientParams& v) { m_AmbientParams = v; }

		const SunParams& GetSunParams() const { return m_SunParams; }
		void SetSunParams(const SunParams& v) { m_SunParams = v; }

		rhi::BufferReadOnlyView GetInstancesBufferView() const;

		// Updates scene graph
		void Update();

		void ResetGpuBuffers() { m_ResetGpuBuffers = true; }

	private:

		void OnLeafAdded(SceneGraphLeaf* leaf);
		void OnLeafRemoved(SceneGraphLeaf* leaf);

	private:

		alm::unique<SceneGraph> m_SceneGraph;		
		std::vector<alm::weak<RenderView>> m_RenderViews;

		AmbientParams m_AmbientParams;
		SunParams m_SunParams;

		GpuSceneBuffersHandle m_GpuBuffersHandle;
		bool m_ResetGpuBuffers;

		std::string m_Name;
		DeviceManager* m_DeviceManager;
	};

}