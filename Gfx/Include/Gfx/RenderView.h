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

namespace interop
{
	struct Scene;
}

namespace st::gfx
{
	class Camera;
	class RenderGraph;
	class RenderStage;
	class DeviceManager;
}

namespace st::gfx
{

class RenderView : public st::enable_weak_from_this<RenderView>, private st::noncopyable_nonmovable
{
public:

	using RenderSet = std::vector<std::pair<rhi::CullMode, std::vector<const st::gfx::MeshInstance*>>>;

public:

	RenderView(DeviceManager* deviceManager, const char* debugName);
	RenderView(ViewportSwapChainId, DeviceManager* deviceManager, const char* debugName);
	~RenderView();

	void SetScene(st::weak<Scene> scene);
	void SetCamera(std::shared_ptr<Camera> camera);

	// Sets render to an offscreen framebuffer. If not initialized or set to null, will render to 
	// main onscreen framebuffer aka main framebuffer
	void SetOffscreenFrameBuffer(st::rhi::FramebufferHandle frameBuffer);

	st::weak<Scene> GetScene() { return m_Scene; }
	std::shared_ptr<Camera> GetCamera() { return m_Camera; }
	st::weak<RenderGraph> GetRenderGraph() { return m_RenderGraph.get_weak(); }
	st::rhi::FramebufferHandle GetFramebuffer();
	st::rhi::FramebufferHandle GetOffscreenFramebuffer() { return m_OffscreenFramebuffer; }
	st::rhi::TextureHandle GetBackBuffer(int idx = 0);

	st::rhi::BufferUniformView GetSceneBufferUniformView();
	st::rhi::BufferReadOnlyView GetCameraVisiblityBufferROView();
	st::rhi::BufferReadOnlyView GetShadowMapVisibilityBufferROView();
	
	const RenderSet& GetCameraVisibleSet() const { return m_CameraVisibleSet; }
	const RenderSet& GetShadowMapVisibleSet() const { return m_ShadowMapVisibleSet; }

	void OnWindowSizeChanged();

	void Render(float timeDeltaSec);

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

	RenderSet GetVisibleSet(const std::span<const math::plane3f>& planes, math::aabox3f* opt_outBounds = nullptr) const;
	void UpdateVisibilityShaderBuffer(const RenderSet& renderSet, gfx::MultiBuffer& multiBuffer, rhi::ICommandList* commandList);

private:

	st::weak<Scene> m_Scene;
	std::shared_ptr<Camera> m_Camera;
	st::unique<RenderGraph> m_RenderGraph;

	ViewportSwapChainId m_ViewportSwapChainId;
	st::rhi::FramebufferHandle m_OffscreenFramebuffer;

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

	float m_TimeDeltaSec = 0.f;

	std::string m_DebugName;
	DeviceManager* m_DeviceManager;
};

} // namespace st::gfx