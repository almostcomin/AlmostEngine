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

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(alm::rhi::FramebufferHandle frameBuffer);

	alm::weak<Scene> GetScene() { return m_Scene; }
	std::shared_ptr<Camera> GetCamera() { return m_Camera; }
	alm::weak<RenderGraph> GetRenderGraph() { return m_RenderGraph.get_weak(); }
	alm::rhi::FramebufferHandle GetFramebuffer();
	alm::rhi::FramebufferHandle GetOffscreenFramebuffer() { return m_OffscreenFramebuffer; }
	alm::rhi::TextureHandle GetBackBuffer(int idx = 0);

	const float4x4& GetPrevFrameViewProjMatrix() const { return m_PrevViewProjectionMatrix; }

	alm::rhi::BufferUniformView GetSceneBufferUniformView();
	alm::rhi::BufferReadOnlyView GetCameraVisiblityBufferROView();
	alm::rhi::BufferReadOnlyView GetShadowMapVisibilityBufferROView();
	
	const RenderSet& GetCameraVisibleSet() const { return m_CameraVisibleSet; }
	const RenderSet& GetShadowMapVisibleSet() const { return m_ShadowMapVisibleSet; }

	void OnWindowSizeChanged();

	void Render(double timeSec, float timeDeltaSec);

	double GetTime() const { return m_TimeSec; }
	float GetTimeDelta() const { return m_TimeDeltaSec; }

	std::string GetName() const { return m_DebugName; }
	DeviceManager* GetDeviceManager() const { return m_DeviceManager; }

private:

	void UpdateSceneConstantBuffer();
	void UpdateCameraVisibleSet(rhi::ICommandList* commandList);
	void UpdateShadowmapData(rhi::ICommandList* commandList);
	void UpdateDirLightsVisibleBuffer(rhi::ICommandList* commandList);
	void UpdatePointLightsVisibleBuffer(rhi::ICommandList* commandList);
	void UpdateSpotLightsVisibleBuffer(rhi::ICommandList* commandList);

	void GetVisibleSet(const std::span<const math::plane3f>& planes, RenderSet& out_renderSet, math::aabox3f* opt_outBounds = nullptr) const;
	void UpdateVisibilityShaderBuffer(const RenderSet& renderSet, gfx::MultiBuffer& multiBuffer, rhi::ICommandList* commandList);

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

	// Offscreen framebuffer
	alm::rhi::FramebufferHandle m_OffscreenFramebuffer;

	// Bounds of the visible scene
	math::aabox3f m_CameraVisibleBounds;

	// Visible set for the current camera
	gfx::MultiBuffer m_CameraVisibleBuffer;
	RenderSet m_CameraVisibleSet;

	// Visible set for the shadowmapping
	gfx::MultiBuffer m_ShadowMapVisibleBuffer;
	RenderSet m_ShadowMapVisibleSet;
	// Matrices for cascade shadowmap
	float4x4 m_ShadowMapWoldToClipMatrix;
	float4x4 m_ViewToShadowMapClipMatrix;

	// Visible set for directional lights
	gfx::MultiBuffer m_DirLightsVisibleBuffer;
	uint32_t m_DirLightsVisibleCount;

	// Visible set for point lights
	gfx::MultiBuffer m_PointLightsVisibleBuffer;
	uint32_t m_PointLightsVisibleCount;

	// Visible set for spot lights
	gfx::MultiBuffer m_SpotLightsVisibleBuffer;
	uint32_t m_SpotLightsVisibleCount;

	// Scene constant buffer, set at begin frame, no change during frame render
	gfx::MultiBuffer m_SceneConstants;

	// Begin & End command lists
	std::vector<rhi::CommandListOwner> m_BeginCommandLists;
	std::vector<rhi::CommandListOwner> m_EndCommandLists;

	double m_TimeSec;
	float m_TimeDeltaSec;

	std::string m_DebugName;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx