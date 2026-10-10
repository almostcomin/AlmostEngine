#pragma once

#include "Core/Common.h"
#include "Core/Memory.h"
#include "Core/Math/plane.h"
#include "RHI/Framebuffer.h"
#include "RHI/CommandList.h"
#include "RHI/Buffer.h"
#include "RHI/TimerQuery.h"
#include "RHI/RasterizerState.h"
#include "Gfx/FrameUniformBuffer.h"
#include "Gfx/Scene.h"
#include "Gfx/ViewportSwapChain.h"
#include "Gfx/MultiBuffer.h"
#include "Gfx/MaterialDomain.h"
#include "Gfx/RenderSet.h"
#include "Gfx/SceneFlags.h"
#include "Gfx/MouseState.h"

namespace interop
{
	struct Scene;
}

namespace alm::gfx
{
	class Camera;
	class RenderGraph;
	class RenderStage;
	class DeviceManager;
	class SceneHeightmap;
	class HeightmapInstance;
}

namespace alm::gfx
{

class RenderView : public alm::enable_weak_from_this<RenderView>, private alm::noncopyable_nonmovable
{
public:

	RenderView(DeviceManager* deviceManager, const char* debugName);
	RenderView(ViewportSwapChainId, DeviceManager* deviceManager, const char* debugName);
	~RenderView();

	void SetScene(alm::weak<Scene> scene);
	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen backbuffer. If not initialized or set to null, will render to 
	// main on-screen backbuffer
	void SetOffscreenBackbuffer(alm::rhi::FramebufferHandle framebuffer);

	// Sets the backbuffer viewport, that portion of the backbuffer that would be used for render
	void SetBackbufferViewport(const uint2& origin, const uint2& size, bool force = false);
	// Set the backbuffer viewport to the entire backbuffer
	void ResetBackbufferViewport();

	std::pair<uint2, uint2> GetBackbufferViewport() const { return { m_BackbufferViewportOrigin, m_BackbufferViewportSize }; }
	const uint2& GetBackbufferViewportSize() const { return m_BackbufferViewportSize; }
	uint2 GetBackbufferSize() const;

	void RegisterHeightmap(const SceneHeightmap* sceneHeightmap);
	void UnregisterHeightmap(const SceneHeightmap* sceneHeightmap);

	alm::weak<Scene> GetScene() { return m_Scene; }
	std::shared_ptr<Camera> GetCamera() const { return m_Camera; }
	alm::weak<RenderGraph> GetRenderGraph() { return m_RenderGraph.get_weak(); }
	alm::rhi::FramebufferHandle GetBackbuffer() const;
	alm::rhi::FramebufferHandle GetOffscreenBackbuffer() { return m_OffscreenBackbuffer; }
	alm::rhi::TextureHandle GetBackBufferColorRT(int idx = 0);

	const float4x4& GetPrevFrameViewProjMatrix() const { return m_PrevViewProjectionMatrix; }

	const float4x4& GetCloudsShadowMapWorldToClipMatrix() const { return m_CloudsShadowMapWorldToClipMatrix; }
	const float4x4& GetCloudsShadowMapClipToTranslatedWorldMatrix() const { return m_CloudsShadowMapClipToTranslatedWorldMatrix; }
	const float3& GetCloudsShadowmapSunPosition() const { return m_CloudsShadowmapSunPosition; }
	float GetCloudsZNear() const { return m_CloudsZNear; }

	alm::rhi::BufferUniformView GetSceneBufferUniformView();
	alm::rhi::BufferReadOnlyView GetCameraVisiblityBufferROView();
	alm::rhi::BufferReadOnlyView GetShadowMapVisibilityBufferROView();
	
	const RenderSet& GetCameraVisibleSet() const { return m_CameraVisibleSet; }
	const RenderSet& GetShadowMapVisibleSet() const { return m_ShadowMapVisibleSet; }

	bool IsShadowmapValid() const { return m_ShadowmapValid; }

	void OnBackbufferResize(const uint2& oldSize, const uint2& newSize);

	void Render(double timeSec, float timeDeltaSec, const MouseState& mouseState);

	void SetPrevExposure(rhi::TextureHandle t) { m_PrevExposure = t; }
	rhi::TextureSampledView GetPrevExposureSampledView() const;

	double GetTime() const { return m_TimeSec; }
	float GetTimeDelta() const { return m_TimeDeltaSec; }
	const MouseState& GetMouseState() const { return m_MouseState; }

	HeightmapInstance* GetHeightmapInstance(const SceneHeightmap* sceneHeightmap) const;

	bool GetFreezeCulling() const { return m_FreezeCulling; }
	void SetFreezeCulling(bool b) { m_FreezeCulling = b; }

	float GetGpuFrameTime() const;

	std::string GetName() const { return m_DebugName; }
	DeviceManager* GetDeviceManager() const { return m_DeviceManager; }

private:

	void UpdateSceneConstantBuffer();
	void UpdateCameraVisibleSet(rhi::ICommandList* commandList);

	bool UpdateShadowmapData(rhi::ICommandList* commandList);
	bool UpdateCloudsShadowmapData(rhi::ICommandList* commandList);

	void UpdateDirLightsVisibleBuffer(rhi::ICommandList* commandList);
	void UpdatePointLightsVisibleBuffer(rhi::ICommandList* commandList);
	void UpdateSpotLightsVisibleBuffer(rhi::ICommandList* commandList);

	void UpdateHeightmaps(const uint2& backbufferSize, rhi::ICommandList* commandList);

	void GetVisibleSet(const VisibleSetContext& context, const std::span<const plane3f>& planes, SceneContentType type,
		RenderSet* opt_outRenderSet, aabox3f* opt_outBounds = nullptr) const;
	void UpdateVisibilityShaderBuffer(const RenderSet& renderSet, gfx::MultiBuffer& multiBuffer, rhi::ICommandList* commandList, const char* marker);

	aabox3d BuildCloudsShadowVolume() const;
	
	void BuildSunSpaceShadowMatrices(const aabox3d& shadowVolumeWorld,
		float4x4* opt_out_worldToClip, float4x4* opt_out_viewToShadowClip, float4x4* opt_out_clipToTranslatedWorld,
		float3* opt_out_sunPos, float* opt_out_zNear) const;

private:

	alm::weak<Scene> m_Scene;
	std::shared_ptr<Camera> m_Camera;
	alm::unique<RenderGraph> m_RenderGraph;

	// Prev frame camera
	float4x4 m_PrevViewProjectionMatrix;
	float3 m_PrevCameraPosition;
	bool m_ResetPrevFrameCamera;

	// ImGui viewports
	ViewportSwapChainId m_ViewportSwapChainId;

	// Offscreen backbuffer
	alm::rhi::FramebufferHandle m_OffscreenBackbuffer;
	// Backbuffer viewport
	uint2 m_BackbufferViewportOrigin;
	uint2 m_BackbufferViewportSize;

	// Bounds of the visible scene
	aabox3f m_CameraVisibleBounds;

	// Visible set for the current camera
	gfx::MultiBuffer m_CameraVisibleBuffer;
	RenderSet m_CameraVisibleSet;

	// Visible set for the shadowmapping
	gfx::MultiBuffer m_ShadowMapVisibleBuffer;
	RenderSet m_ShadowMapVisibleSet;

	// Matrices for cascade shadowmap
	float4x4 m_ShadowMapWorldToClipMatrix;
	float4x4 m_ViewToShadowMapClipMatrix;
	std::array<plane3f, 6> m_ShadowCullPlanes;

	// Matrices for clouds shadowmap
	float4x4 m_CloudsShadowMapWorldToClipMatrix;
	float4x4 m_CloudsShadowMapClipToTranslatedWorldMatrix;
	float3 m_CloudsShadowmapSunPosition;
	float m_CloudsZNear;

	// Visible set for directional lights
	gfx::MultiBuffer m_DirLightsVisibleBuffer;
	uint32_t m_DirLightsVisibleCount;

	// Visible set for point lights
	gfx::MultiBuffer m_PointLightsVisibleBuffer;
	uint32_t m_PointLightsVisibleCount;

	// Visible set for spot lights
	gfx::MultiBuffer m_SpotLightsVisibleBuffer;
	uint32_t m_SpotLightsVisibleCount;

	std::unordered_map<const SceneHeightmap*, std::unique_ptr<HeightmapInstance>> m_HeightmapInstances;

	// Scene constant buffer, set at begin frame, no change during frame render
	gfx::MultiBuffer m_SceneConstants;

	// Begin & End command lists
	std::vector<rhi::CommandListOwner> m_BeginCommandLists;
	std::vector<rhi::CommandListOwner> m_EndCommandLists;

	bool m_ShadowmapValid;
	bool m_CloudsShadowmapValid;

	rhi::TextureHandle m_PrevExposure;

	bool m_FreezeCulling;
	alm::math::frustum3f m_CameraFrustum;

	double m_TimeSec;
	float m_TimeDeltaSec;
	MouseState m_MouseState;

	std::string m_DebugName;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx