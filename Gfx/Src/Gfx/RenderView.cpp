#include "Gfx/GfxPCH.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderStage.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Core/Log.h"
#include "Interop/RenderResources.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneLights.h"
#include "Gfx/UploadBuffer.h"
#include "Gfx/Util.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/Renderable.h"
#include "Gfx/VisibleSetContext.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/HeightmapInstance.h"
#include "Gfx/AtmosphereConfig.h"

static std::optional<double2> RayShellOutsideEarth(
	const double3& rayOrigin, const double3& rayDir,
	const double3& earthCenter, double earthRadius,
	double innerRadius, double outerRadius)
{
	auto tOuter = alm::RaySphereIntersection(rayOrigin, rayDir, earthCenter, outerRadius);
	if (!tOuter || tOuter->y <= 0.0)
		return std::nullopt;

	auto tInner = alm::RaySphereIntersection(rayOrigin, rayDir, earthCenter, innerRadius);

	double tShellNear = std::max(tOuter->x, 0.0);
	// If the origin is inside the inner sphere, the entry is tInner->y
	if (tInner && tInner->x < 0.0 && tInner->y > 0.0)
		tShellNear = std::max(tShellNear, tInner->y);
	double tShellFar = tOuter->y;

	auto tEarth = alm::RaySphereIntersection(rayOrigin, rayDir, earthCenter, earthRadius);
	double tEarthStart = 0.0;
	double tEarthEnd = std::numeric_limits<double>::infinity();

	if (tEarth && tEarth->y > 0.0)
	{
		if (tEarth->x >= 0.0)
			tEarthEnd = tEarth->x; // ray collides with earth
		else
			tEarthStart = tEarth->y; // rayOrigin is inside earth
	}
	// If tEarth is nullopt or both points are neg, the rays does not collide with earth

	double tNear = std::max(tShellNear, tEarthStart);
	double tFar = std::min(tShellFar, tEarthEnd);

	if (tNear >= tFar)
		return std::nullopt;

	return double2{ tNear, tFar };
}

alm::gfx::RenderView::RenderView(DeviceManager* deviceManager, const char* debugName) :
	RenderView{ nullptr, deviceManager, debugName }
{}

alm::gfx::RenderView::RenderView(ViewportSwapChainId viewportId, DeviceManager* deviceManager, const char* debugName) :
	m_PrevViewProjectionMatrix{ glm::identity<float4x4>() },
	m_PrevCameraPosition{ 0.f, 0.f, 0.f },
	m_ResetPrevFrameCamera{ true },
	m_ViewportSwapChainId{ viewportId },
	m_ShadowmapValid{ false },
	m_CloudsShadowmapValid{ false },
	m_TimeSec{ 0.0 },
	m_TimeDeltaSec{ 0.f },
	m_DebugName{ debugName },
	m_DeviceManager{ deviceManager }
{
	rhi::Device* device = m_DeviceManager->GetDevice();

	rhi::CommandListParams params{
		.queueType = rhi::QueueType::Graphics
	};
	for (int i = 0; i < m_DeviceManager->GetFramesInFlightCount(); ++i)
	{
		m_BeginCommandLists.push_back(device->CreateCommandList(
			params, std::format("{} - BeginCmdList[{}]", m_DebugName, i)));
		m_EndCommandLists.push_back(device->CreateCommandList(
			params, std::format("{} - EndCmdList[{}]", m_DebugName, i)));
	}

	m_SceneConstants.InitUniformBuffer(sizeof(interop::SceneConstants), m_DeviceManager, "SceneUniformBuffer");

	m_CameraVisibleBuffer.InitRaw(alm::rhi::BufferShaderUsage::ReadOnly, 0, rhi::ResourceState::SHADER_RESOURCE,
		m_DeviceManager, "CameraVisibleIndices");
	m_ShadowMapVisibleBuffer.InitRaw(alm::rhi::BufferShaderUsage::ReadOnly, 0, rhi::ResourceState::SHADER_RESOURCE,
		m_DeviceManager, "ShadowMapVisibleIndices");
	m_DirLightsVisibleBuffer.InitStructured(alm::rhi::BufferShaderUsage::ReadOnly, 0, sizeof(interop::DirLightData),
		rhi::ResourceState::SHADER_RESOURCE, m_DeviceManager, "DirLightsVisibleBuffer");
	m_PointLightsVisibleBuffer.InitStructured(alm::rhi::BufferShaderUsage::ReadOnly, 0, sizeof(interop::PointLightData),
		rhi::ResourceState::SHADER_RESOURCE, m_DeviceManager, "PointLightsVisibleBuffer");
	m_SpotLightsVisibleBuffer.InitStructured(alm::rhi::BufferShaderUsage::ReadOnly, 0, sizeof(interop::SpotLightData),
		rhi::ResourceState::SHADER_RESOURCE, m_DeviceManager, "SpotLightsVisibleBuffer");

	m_RenderGraph = alm::make_unique_with_weak<RenderGraph>(this, debugName);
}

alm::gfx::RenderView::~RenderView()
{
	if (m_Scene)
	{
		m_Scene->DetachRenderView(weak_from_this());
	}
	m_RenderGraph.reset();
}

void alm::gfx::RenderView::SetScene(alm::weak<Scene> scene)
{
	m_CameraVisibleBuffer.Reset();
	m_ShadowMapVisibleBuffer.Reset();
	m_DirLightsVisibleBuffer.Reset();
	m_PointLightsVisibleBuffer.Reset();
	m_SpotLightsVisibleBuffer.Reset();

	if (m_Scene)
	{
		m_Scene->DetachRenderView(weak_from_this());
	}
	m_Scene = scene;
	if (m_Scene)
	{
		m_Scene->AttachRenderView(weak_from_this());
	}

	m_RenderGraph->OnSceneChanged();
}

void alm::gfx::RenderView::SetCamera(std::shared_ptr<alm::gfx::Camera> camera)
{
	m_Camera = camera;
}

void alm::gfx::RenderView::SetOffscreenFrameBuffer(alm::rhi::FramebufferHandle frameBuffer)
{
	m_OffscreenFramebuffer = frameBuffer;
}

void alm::gfx::RenderView::RegisterHeightmap(const SceneHeightmap* sceneHeightmap)
{
	assert(m_HeightmapInstances.find(sceneHeightmap) == m_HeightmapInstances.end() &&
		"Heightmap already registered");
	m_HeightmapInstances.emplace(sceneHeightmap, std::make_unique<HeightmapInstance>(sceneHeightmap));
}

void alm::gfx::RenderView::UnregisterHeightmap(const SceneHeightmap* sceneHeightmap)
{
	assert(m_HeightmapInstances.find(sceneHeightmap) != m_HeightmapInstances.end() &&
		"Heightmap not registered");
	m_HeightmapInstances.erase(sceneHeightmap);
}

alm::rhi::FramebufferHandle alm::gfx::RenderView::GetFramebuffer()
{
	if (m_OffscreenFramebuffer)
	{
		return m_OffscreenFramebuffer;
	}
	if (m_ViewportSwapChainId != nullptr)
	{
		return m_DeviceManager->GetViewportCurrentFramebuffer(m_ViewportSwapChainId);
	}
	return m_DeviceManager->GetCurrentFramebuffer();
}

alm::rhi::TextureHandle alm::gfx::RenderView::GetBackBuffer(int idx)
{
	return GetFramebuffer()->GetBackBuffer(idx);
}

alm::rhi::BufferUniformView alm::gfx::RenderView::GetSceneBufferUniformView()
{
	return m_SceneConstants.GetUniformView();
}

alm::rhi::BufferReadOnlyView alm::gfx::RenderView::GetCameraVisiblityBufferROView()
{
	return m_CameraVisibleBuffer.GetReadOnlyView();
}

alm::rhi::BufferReadOnlyView alm::gfx::RenderView::GetShadowMapVisibilityBufferROView()
{
	return m_ShadowMapVisibleBuffer.GetReadOnlyView();
}

void alm::gfx::RenderView::OnWindowSizeChanged()
{
	// Actually we are only interested in this if we are rendering to the main swap chain BB
	if (!m_OffscreenFramebuffer && !m_ViewportSwapChainId)
	{
		const auto newSize = m_DeviceManager->GetWindowDimensions();
		m_RenderGraph->OnRenderTargetChanged(newSize);
	}
}

void alm::gfx::RenderView::Render(double timeSec, float timeDeltaSec, const MouseState& mouseState)
{
	ZoneScoped

	alm::rhi::FramebufferHandle frameBuffer = GetFramebuffer();
	if (!frameBuffer)
	{
		LOG_ERROR("No frame buffer specified. Nothing to render");
		return;
	}
	alm::gfx::GpuSceneBuffers* gpuSceneBuffers = m_DeviceManager->GetGpuSceneBuffers();

	rhi::ICommandList* beginCommandList = m_BeginCommandLists[m_DeviceManager->GetFrameModuleIndex()].get();
	beginCommandList->Open();
	beginCommandList->BeginMarker(m_DebugName.c_str());
	beginCommandList->BeginMarker("Begin commands");

	// Update common data
	m_TimeSec = timeSec;
	m_TimeDeltaSec = timeDeltaSec;
	m_MouseState = mouseState;

	if (m_Camera && m_ResetPrevFrameCamera)
	{
		m_PrevViewProjectionMatrix = m_Camera->GetViewProjectionMatrix();
		m_PrevCameraPosition = m_Camera->GetPosition();
		m_ResetPrevFrameCamera = false;
	}

	// Transients
	if (m_Scene)
	{
		gpuSceneBuffers->ResetTransients(m_Scene->GetGpuSceneBuffersHandle());

		// Update heightmaps, the only transients for the moment
		{
			const auto& fbInfo = frameBuffer->GetFramebufferInfo();
			UpdateHeightmaps({ fbInfo.width, fbInfo.height }, beginCommandList);
		}
		
		gpuSceneBuffers->FlushTransients(m_Scene->GetGpuSceneBuffersHandle(), beginCommandList);
	}

	// Collects draw infos for camera view
	UpdateCameraVisibleSet(beginCommandList);
	// Collects draw infos for shadowmap
	m_ShadowmapValid = UpdateShadowmapData(beginCommandList);
	m_CloudsShadowmapValid = UpdateCloudsShadowmapData(beginCommandList);

	// Updates visible lights buffers
	UpdateDirLightsVisibleBuffer(beginCommandList);
	UpdatePointLightsVisibleBuffer(beginCommandList);
	UpdateSpotLightsVisibleBuffer(beginCommandList);

	// Update the Scene constant buffer
	UpdateSceneConstantBuffer();

	// Back buffer is in COMMON state and need to be transitioned to RT
	beginCommandList->PushBarrier(rhi::Barrier::Texture(
		frameBuffer->GetDesc().ColorAttachments[0].texture.get(), rhi::ResourceState::PRESENT, rhi::ResourceState::RENDERTARGET));

	// Give the oportunity to the rendergraph to do some initial setup using beginCommandList
	m_RenderGraph->BeginRender(beginCommandList);

	// Done with beginCommandList
	beginCommandList->EndMarker();
	beginCommandList->Close();
	m_DeviceManager->GetDevice()->ExecuteCommandList(beginCommandList, alm::rhi::QueueType::Graphics);

	// Render the stages
	m_RenderGraph->Render(GetFramebuffer());

	// Finish rendering
	rhi::ICommandList* endCommandList = m_EndCommandLists[m_DeviceManager->GetFrameModuleIndex()].get();
	endCommandList->Open();
	endCommandList->BeginMarker("End commands");

	// Give the oportunity to the rendergraph to do some final operations using endCommandList
	m_RenderGraph->EndRender(endCommandList);

	// Back buffer to common so it can be presented
	endCommandList->PushBarrier(rhi::Barrier().Texture(
		frameBuffer->GetDesc().ColorAttachments[0].texture.get(), rhi::ResourceState::RENDERTARGET, rhi::ResourceState::PRESENT));

	endCommandList->EndMarker();
	endCommandList->EndMarker();
	endCommandList->Close();

	// Done with endCommandList
	m_DeviceManager->GetDevice()->ExecuteCommandList(endCommandList, alm::rhi::QueueType::Graphics);

	if (m_Camera)
	{
		m_PrevViewProjectionMatrix = m_Camera->GetViewProjectionMatrix();
		m_PrevCameraPosition = m_Camera->GetPosition();
	}
}

alm::gfx::HeightmapInstance* alm::gfx::RenderView::GetHeightmapInstance(const SceneHeightmap* sceneHeightmap) const
{
	auto it = m_HeightmapInstances.find(sceneHeightmap);
	if (it != m_HeightmapInstances.end())
		return it->second.get();
	return nullptr;
}

float alm::gfx::RenderView::GetGpuFrameTime() const
{
	return m_RenderGraph->GetAggregatedGPUTimeMs();
}

void alm::gfx::RenderView::UpdateSceneConstantBuffer()
{
	interop::SceneConstants* sceneShaderConstant = (interop::SceneConstants*)m_SceneConstants.Map();
	*sceneShaderConstant = {};

	const float2 screenResolution = float2{ GetFramebuffer()->GetFramebufferInfo().width, GetFramebuffer()->GetFramebufferInfo().height };
	sceneShaderConstant->screenResolution = screenResolution;
	sceneShaderConstant->invScreenResolution = 1.0f / screenResolution;	
	sceneShaderConstant->aspect = screenResolution.x / screenResolution.y;

	sceneShaderConstant->time = m_TimeSec;
	sceneShaderConstant->deltaTime = m_TimeDeltaSec;
	sceneShaderConstant->mouseState = float4{
		m_MouseState.pos.x, m_MouseState.pos.y,
		m_MouseState.leftButton ? 1.f : 0.f,
		m_MouseState.rightButton ? 1.f : 0.f
	};

	// Camera
	if (m_Camera)
	{
		sceneShaderConstant->camViewProjMatrix = m_Camera->GetViewProjectionMatrix();
		sceneShaderConstant->camViewMatrix = m_Camera->GetViewMatrix();
		sceneShaderConstant->camProjMatrix = m_Camera->GetProjectionMatrix();
		sceneShaderConstant->camWorldPos = m_Camera->GetPosition();
		sceneShaderConstant->camZNear = m_Camera->GetZNear();
		const math::frustum3f& frustumPlanes = m_Camera->GetFrustum();
		for (int i = 0; i < 6; ++i)
		{
			sceneShaderConstant->frustumPlanes[i] = float4{ frustumPlanes[i] };
		}
		for (int i = 0; i < 6; ++i)
		{
			sceneShaderConstant->shadowCasterPlanes[i] = float4{ m_ShadowCullPlanes[i] };
		}
	}
	else
	{
		sceneShaderConstant->camViewProjMatrix = float4x4{ 1.f };
		sceneShaderConstant->camViewMatrix = float4x4{ 1.f };
		sceneShaderConstant->camProjMatrix = float4x4{ 1.f };
		sceneShaderConstant->camWorldPos = float3{ 0.f };
		sceneShaderConstant->camZNear = 0.f;
		for (int i = 0; i < 6; ++i)
		{
			sceneShaderConstant->frustumPlanes[i] = float4{ 0.f, 0.f, 0.f, 0.f };
		}
		for (int i = 0; i < 6; ++i)
		{
			sceneShaderConstant->shadowCasterPlanes[i] = float4{ 0.f, 0.f, 0.f, 0.f };
		}
	}
	sceneShaderConstant->invCamViewProjMatrix = glm::inverse(sceneShaderConstant->camViewProjMatrix);
	sceneShaderConstant->invCamViewMatrix = glm::inverse(sceneShaderConstant->camViewMatrix);
	sceneShaderConstant->invCamProjMatrix = glm::inverse(sceneShaderConstant->camProjMatrix);

	// Scene data
	if (m_Scene)
	{
		const AtmosphereConfig* atmos = m_Scene->GetAtmosphereConfig();
		// ShadowMap matrices
		{
			sceneShaderConstant->shadowMapWorldToClipMatrix = m_ShadowMapWorldToClipMatrix;
			sceneShaderConstant->shadowMapViewToClipMatrix = m_ViewToShadowMapClipMatrix;
		}

		// Clouds
		{
			sceneShaderConstant->CloudsShadowmapWorldToClipMatrix = m_CloudsShadowMapWorldToClipMatrix;
			sceneShaderConstant->CloudsShadowmapSunPosition = m_CloudsShadowmapSunPosition;
		}

		// Ambient
		{
			sceneShaderConstant->ambientTop = float4{ atmos->Ambient.SkyColor * atmos->Ambient.Intensity, 0.f };
			sceneShaderConstant->ambientBottom = float4{ atmos->Ambient.GroundColor * atmos->Ambient.Intensity, 0.f };
		}

		// Lights
		{
			const float3 sunDir = atmos->GetSunDirection();

			sceneShaderConstant->mainDirLight.viewSpaceDirection = m_Camera->GetViewMatrix() * float4 { sunDir, 0.f };
			sceneShaderConstant->mainDirLight.irradiance = atmos->Sun.Irradiance;
			sceneShaderConstant->mainDirLight.color = atmos->Sun.Color;
			sceneShaderConstant->mainDirLight.halfAngularSize = glm::radians(atmos->Sun.AngularSizeDeg) / 2.f;

			sceneShaderConstant->dirLightCount = m_DirLightsVisibleCount;
			sceneShaderConstant->dirLightsDataDI = m_DirLightsVisibleBuffer.GetReadOnlyView();
			sceneShaderConstant->pointLightCount = m_PointLightsVisibleCount;
			sceneShaderConstant->pointLightsDataDI = m_PointLightsVisibleBuffer.GetReadOnlyView();
			sceneShaderConstant->spotLightCount = m_SpotLightsVisibleCount;
			sceneShaderConstant->spotLightsDataDI = m_SpotLightsVisibleBuffer.GetReadOnlyView();;
		}

		// Data buffers
		const alm::gfx::GpuSceneBuffers* gpuSceneBuffers = m_DeviceManager->GetGpuSceneBuffers();

		sceneShaderConstant->instanceBufferDI = gpuSceneBuffers->GetInstancesBufferView(m_Scene->GetGpuSceneBuffersHandle());
		sceneShaderConstant->meshesBufferDI = gpuSceneBuffers->GetMeshesBufferView();
		sceneShaderConstant->materialsBufferDI = gpuSceneBuffers->GetMaterialsBufferView();
		sceneShaderConstant->patchDataBufferDI = gpuSceneBuffers->GetHeightmapPatchDataBufferView(m_Scene->GetGpuSceneBuffersHandle());
		sceneShaderConstant->terrainMaterialsBufferDI = gpuSceneBuffers->GetTerrainMaterialsBufferView();
	}

	m_SceneConstants.Unmap();
}

void alm::gfx::RenderView::UpdateCameraVisibleSet(rhi::ICommandList* commandList)
{
	ZoneScoped;

	m_CameraVisibleSet.Elements.clear();
	m_CameraVisibleBounds.reset();
	m_ShadowCastersCameraVisibleBounds.reset();
	if (!m_Camera || !m_Scene || !m_Scene->GetSceneGraph())
	{
		return;
	}

	VisibleSetContext context{ 
		.HeightmapInstances = &m_HeightmapInstances,
		.View = this };

	GetVisibleSet(context, m_Camera->GetFrustum().get_planes(), SceneContentType::Meshes, m_CameraVisibleSet, &m_CameraVisibleBounds, 
		SceneContentType::ShadowCasters, &m_ShadowCastersCameraVisibleBounds);

	UpdateVisibilityShaderBuffer(m_CameraVisibleSet, m_CameraVisibleBuffer, commandList, "Camera Visible Buffer");
}

bool alm::gfx::RenderView::UpdateShadowmapData(rhi::ICommandList* commandList)
{
	ZoneScoped;

	m_ShadowMapVisibleSet.Elements.clear();
	m_ShadowMapWorldToClipMatrix = {};
	m_ViewToShadowMapClipMatrix = {};

	if (!m_Scene)
		return false;
	if (!m_CameraVisibleBounds.valid())
		return false;

	const AtmosphereConfig* atmos = m_Scene->GetAtmosphereConfig();
	const double3 sunDir = atmos->GetSunDirection();

	const alm::aabox3f& worldCasterBoundsF = m_Scene->GetWorldBounds(SceneContentType::ShadowCasters);
	if (!worldCasterBoundsF.valid())
		return false;  // no shadow casters in the scene

	// Promote bounds to double for the precision-critical calculations
	const aabox3d cameraVisibleBoundsD(m_CameraVisibleBounds);
	const aabox3d worldCasterBoundsD(worldCasterBoundsF);

	// --- Build search volume (camera + world casters, extended in sun direction)
	//     for GetVisibleSet. This is NOT extracted — it's specific to shadow casters culling.

	// --- 1. Sun view matrix (arbitrary sunPos, it will be recalculated later)

	double3 sunPos = m_CameraVisibleBounds.center();
	const double3 sunUp = fabs(glm::dot(sunDir, { 0, 1, 0 })) > 0.999f ? double3(0, 0, 1) : double3(0, 1, 0);
	double4x4 sunViewMatrixD = glm::lookAtRH(sunPos, sunPos + sunDir, sunUp);

	// --- 2. Construct shadow search volume in sun-space

	aabox3d cameraBoundsSun = m_CameraVisibleBounds.transform(sunViewMatrixD);
	const aabox3d worldCasterBoundsSun = worldCasterBoundsD.transform(sunViewMatrixD);

	// Extends Z towards the sun (max.z positive is behind the sun in sun-space) 
	// so we cover any caster that could be betwween sun and shadow receivers
	cameraBoundsSun.max.z = std::max(cameraBoundsSun.max.z, worldCasterBoundsSun.max.z);
	// So cameraBoundsSun is the volume that includes all visible receivers extended towards sun direction
	// to include world shadow casters.

	// --- 3. Culling shadow casters using search volume

	const aabox3d searchVolumeWorldD = cameraBoundsSun.transform(glm::inverse(sunViewMatrixD));
	const aabox3f searchVolumeWorld(searchVolumeWorldD);  // back to float for the cull
	const auto searchPlanes = searchVolumeWorld.getClipPlanes();
	m_ShadowCullPlanes = searchPlanes;

	VisibleSetContext context{
		.HeightmapInstances = &m_HeightmapInstances,
		.View = this };
	aabox3f casterBoundsForShadowMapF;
	GetVisibleSet(context, searchPlanes, SceneContentType::ShadowCasters, m_ShadowMapVisibleSet, &casterBoundsForShadowMapF);
	if (!casterBoundsForShadowMapF.valid())
		return false;  // no casters cast shadow on visible receivers
	// So casterBoundsForShadowMapF is the bbox of all the casters that cast shadow to something visible

	// Promote back to double for the precision-critical Z calculations
	const aabox3d casterBoundsForShadowMapD(casterBoundsForShadowMapF);

	// --- 4. Build matrices

	BuildSunSpaceShadowMatrices(casterBoundsForShadowMapD, &m_ShadowMapWorldToClipMatrix, &m_ViewToShadowMapClipMatrix, nullptr,
		nullptr, nullptr);

	// --- 5. Update visible set for shadowmap

	UpdateVisibilityShaderBuffer(m_ShadowMapVisibleSet, m_ShadowMapVisibleBuffer, commandList, "Shadowmap Visible Buffer");

	return true;
}

bool alm::gfx::RenderView::UpdateCloudsShadowmapData(rhi::ICommandList* commandList)
{
	ZoneScoped;

	m_CloudsShadowMapWorldToClipMatrix = {};
	m_CloudsShadowMapClipToTranslatedWorldMatrix = {};
	m_CloudsShadowmapSunPosition = {};
	m_CloudsZNear = 0.f;

	if (!m_Scene)
		return false;

	const AtmosphereConfig* atmos = m_Scene->GetAtmosphereConfig();
	if (!atmos->CloudsSubsystemInitialized())
		return false;

	const aabox3d shadowBBox = BuildCloudsShadowVolume();
	if (!shadowBBox.valid())
		return false;

	BuildSunSpaceShadowMatrices(shadowBBox,
		&m_CloudsShadowMapWorldToClipMatrix, nullptr, &m_CloudsShadowMapClipToTranslatedWorldMatrix, &m_CloudsShadowmapSunPosition, &m_CloudsZNear);

	return true;
}

void alm::gfx::RenderView::UpdateDirLightsVisibleBuffer(rhi::ICommandList* commandList)
{
	ZoneScoped;

	m_DirLightsVisibleCount = 0;

	if (!m_Scene || !m_Scene->GetSceneGraph())
		return;

	const auto& visibleDirLights = m_Scene->GetSceneGraph()->GetSceneDirLights();
	m_DirLightsVisibleCount = visibleDirLights.size();

	if (m_DirLightsVisibleCount == 0)
		return;

	uint32_t reqSize = m_DirLightsVisibleCount * sizeof(interop::DirLightData);
	m_DirLightsVisibleBuffer.Grow(reqSize);  // Exact size, since directional light do not cull

	commandList->BeginMarker("DirLights Visible Buffer");

	rhi::BufferHandle buffer = m_DirLightsVisibleBuffer.GetCurrentBuffer();
	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

	// Copy to upload data
	UploadBuffer* uploadBuffer = m_DeviceManager->GetUploadBuffer();
	auto [data, offset] = uploadBuffer->RequestSpaceForBufferDataUpload(reqSize);

	auto* ptr = (interop::DirLightData*)data;
	for (const auto* dirLight : visibleDirLights)
	{
		ptr->viewSpaceDirection = glm::normalize(m_Camera->GetViewMatrix() * float4{ dirLight->GetDirection(), 0.f });
		ptr->irradiance = dirLight->GetIrradiance();
		ptr->color = dirLight->GetColor();
		ptr->halfAngularSize = dirLight->GetAngularSize() / 2.f;
		ptr++;
	}

	// Copy to buffer
	commandList->CopyBufferToBuffer(buffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, reqSize);

	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

	commandList->EndMarker();
}

void alm::gfx::RenderView::UpdatePointLightsVisibleBuffer(rhi::ICommandList* commandList)
{
	ZoneScoped;

	m_PointLightsVisibleCount = 0;

	if (!m_Scene || !m_Scene->GetSceneGraph())
		return;

	const auto& frustum = m_Camera->GetFrustum();
	auto testFrustum = [&frustum](const float3 & pos, float radius) -> bool
	{
		for (const auto& plane : frustum.get_planes())
		{
			if (plane.distance(pos) < -radius)
				return false;
		}
		return true;
	};

	std::vector<const alm::gfx::ScenePointLight*> visiblePointLights;
	alm::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (has_any_flag(node->GetContentFlags(), SceneContentFlags::PointLights) &&
			node->Test(SceneContentType::PointLights, frustum.get_planes()))
		{
			auto leaf = node->GetLeaf();
			if (leaf && leaf->GetType() == SceneGraphLeaf::Type::PointLight)
			{
				const auto* pointLight = alm::checked_cast<const alm::gfx::ScenePointLight*>(leaf.get());
				if (testFrustum(node->GetWorldPosition(), pointLight->GetRange()))
				{
					visiblePointLights.push_back(pointLight);
				}
			}
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	}
	m_PointLightsVisibleCount = visiblePointLights.size();

	if (m_PointLightsVisibleCount == 0)
		return;

	uint32_t reqSize = m_PointLightsVisibleCount * sizeof(interop::PointLightData);
	m_PointLightsVisibleBuffer.Grow(reqSize * 2);

	commandList->BeginMarker("PointLights Visible Buffer");

	rhi::BufferHandle buffer = m_PointLightsVisibleBuffer.GetCurrentBuffer();
	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

	// Copy to upload data
	UploadBuffer* uploadBuffer = m_DeviceManager->GetUploadBuffer();
	auto [data, offset] = uploadBuffer->RequestSpaceForBufferDataUpload(reqSize);

	auto* ptr = (interop::PointLightData*)data;
	for (const auto* pointLight : visiblePointLights)
	{
		ptr->viewSpacePosition = m_Camera->GetViewMatrix() * float4 { pointLight->GetNode()->GetWorldPosition(), 1.f };
		ptr->range = pointLight->GetRange();
		ptr->color = pointLight->GetColor();
		ptr->intensity = pointLight->GetIntensity();
		ptr->radius = pointLight->GetRadius();
		ptr++;
	}

	// Copy to buffer
	commandList->CopyBufferToBuffer(buffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, reqSize);

	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

	commandList->EndMarker();
}

void alm::gfx::RenderView::UpdateSpotLightsVisibleBuffer(rhi::ICommandList* commandList)
{
	ZoneScoped;

	m_SpotLightsVisibleCount = 0;

	if (!m_Scene || !m_Scene->GetSceneGraph())
		return;

	const auto& frustum = m_Camera->GetFrustum();
	auto testFrustum = [&frustum](const float3& pos, float radius) -> bool
		{
			for (const auto& plane : frustum.get_planes())
			{
				if (plane.distance(pos) < -radius)
					return false;
			}
			return true;
		};

	std::vector<const alm::gfx::SceneSpotLight*> visibleSpotLights;
	alm::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (has_any_flag(node->GetContentFlags(), SceneContentFlags::SpotLights) &&
			node->Test(SceneContentType::SpotLights, frustum.get_planes()))
		{
			auto leaf = node->GetLeaf();
			if (leaf && leaf->GetType() == SceneGraphLeaf::Type::SpotLight)
			{
				const auto* spotLight = alm::checked_cast<const alm::gfx::SceneSpotLight*>(leaf.get());
				if (testFrustum(node->GetWorldPosition(), spotLight->GetRange()))
				{
					visibleSpotLights.push_back(spotLight);
				}
			}
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	}
	m_SpotLightsVisibleCount = visibleSpotLights.size();

	if (m_SpotLightsVisibleCount == 0)
		return;

	uint32_t reqSize = m_SpotLightsVisibleCount * sizeof(interop::SpotLightData);
	m_SpotLightsVisibleBuffer.Grow(reqSize * 2);

	commandList->BeginMarker("SpotLights Visible Buffer");

	rhi::BufferHandle buffer = m_SpotLightsVisibleBuffer.GetCurrentBuffer();
	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

	// Copy to upload data
	UploadBuffer* uploadBuffer = m_DeviceManager->GetUploadBuffer();
	auto [data, offset] = uploadBuffer->RequestSpaceForBufferDataUpload(reqSize);

	auto* ptr = (interop::SpotLightData*)data;
	for (const auto* spotLight : visibleSpotLights)
	{
		ptr->viewSpacePosition = m_Camera->GetViewMatrix() * float4 { spotLight->GetNode()->GetWorldPosition(), 1.f };
		ptr->viewSpaceDirection = glm::normalize(m_Camera->GetViewMatrix() * float4 { spotLight->GetDirection(), 0.f });
		ptr->range = spotLight->GetRange();
		ptr->color = spotLight->GetColor();
		ptr->intensity = spotLight->GetIntensity();
		ptr->radius = spotLight->GetRadius();
		ptr->innerAngle = spotLight->GetInnerConeAngle();
		ptr->outerAngle = spotLight->GetOuterConeAngle();
		ptr++;
	}

	// Copy to buffer
	commandList->CopyBufferToBuffer(buffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, reqSize);

	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

	commandList->EndMarker();
}

void alm::gfx::RenderView::UpdateHeightmaps(const uint2& frameBufferSize, rhi::ICommandList* commandList)
{
	ZoneScoped;

	commandList->BeginMarker("Heightmaps");

	if (m_Camera)
	{
		for (auto& [_, instance] : m_HeightmapInstances)
		{
			instance->Update(m_Camera.get(), frameBufferSize, m_DeviceManager->GetGpuSceneBuffers(), m_Scene->GetGpuSceneBuffersHandle());
		}
	}

	commandList->EndMarker();
}

void alm::gfx::RenderView::GetVisibleSet(const VisibleSetContext& context, const std::span<const plane3f>& planes, SceneContentType primaryType,
	RenderSet& out_renderSet, aabox3f* opt_outPrimaryBounds,  SceneContentType secondaryType, aabox3f* opt_outSecondaryBounds) const
{
	ZoneScoped;

	assert(HasRenderableCategory(primaryType));
	const auto* gpuSceneBuffers = m_DeviceManager->GetGpuSceneBuffers();

	out_renderSet.Elements.clear();

	if (!m_Scene || !m_Scene->GetSceneGraph())
		return;
	
	std::vector<RenderableDrawInfo> drawInfos[(int)MaterialDomain::_Size][(int)rhi::CullMode::_Size];

	if (opt_outPrimaryBounds)
		opt_outPrimaryBounds->reset();
	if (opt_outSecondaryBounds)
		opt_outSecondaryBounds->reset();

	alm::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (has_any_flag(node->GetContentFlags(), ToFlag(primaryType)) && node->Test(primaryType, planes))
		{
			alm::weak<SceneGraphLeaf> leaf = node->GetLeaf();
			if (leaf && has_any_flag(leaf->GetRenderFlags(), SceneRenderFlags::Visible))
			{
				if (has_any_flag(leaf->GetContentFlags(), ToFlag(primaryType)))
				{
					const auto* renderable = leaf->AsRenderable();
					if(renderable)
					{
						std::vector<RenderableDrawInfo> renderableDrawInfos;
						renderable->CollectDrawInfos(context, gpuSceneBuffers, renderableDrawInfos);
						if (!renderableDrawInfos.empty())
						{
							for (const RenderableDrawInfo& drawInfo : renderableDrawInfos)
							{
								drawInfos[(int)drawInfo.MaterialDomain][(int)drawInfo.CullMode].push_back(drawInfo);
							}

							if (opt_outPrimaryBounds)
							{
								auto leafWorldBounds = leaf->GetBounds().transform(node->GetWorldTransform());
								opt_outPrimaryBounds->merge(leafWorldBounds);
							}

							if (opt_outSecondaryBounds && secondaryType != SceneContentType::_Size && has_any_flag(leaf->GetContentFlags(), ToFlag(secondaryType)))
							{
								auto leafWorldBounds = leaf->GetBounds().transform(node->GetWorldTransform());
								opt_outSecondaryBounds->merge(leafWorldBounds);
							}
						}
					}
				}
			}
			walker.Next();
		}
		else
		{
			walker.NextSibling();
		}
	}

	// Sort by mesh to be friendly with DrawIndirect
	for(int domain = 0; domain < (int)MaterialDomain::_Size; ++domain)
	{
		for(int cullMode = 0; cullMode < (int)rhi::CullMode::_Size; ++cullMode)
		{
			// Sort by BatchKey so consecutive draw infos sharing a key can be issued as
			// a single DrawInstanced call.
			//
			// Stable sort is required (not just sort): inside a batch, the draw infos are
			// expected to have consecutive InstanceIdx values (e.g. baseInstance..baseInstance+N-1),
			// because the shader will read instances[baseInstance + SV_InstanceID].
			// Reordering draw infos within the same BatchKey would break that invariant.
			//
			// This matters in particular for HeightmapInstance, which pre-sorts its leaf
			// nodes by mesh variant before allocating consecutive transient slots; any
			// reshuffling here would mismatch slots and shader instance accesses.
			std::ranges::stable_sort(drawInfos[domain][cullMode], [](const RenderableDrawInfo& a, const RenderableDrawInfo& b)
			{
				return a.BatchKey < b.BatchKey;
			});
		}
	}

	// Move to result
	for (int domain = 0; domain < (int)MaterialDomain::_Size; ++domain)
	{
		RenderSet::MaterialDomainSet domainSet{ (MaterialDomain)domain, {} };
		for (int cullMode = 0; cullMode < (int)rhi::CullMode::_Size; ++cullMode)
		{
			if (!drawInfos[domain][cullMode].empty())
			{
				domainSet.second.emplace_back((rhi::CullMode)cullMode, std::move(drawInfos[domain][cullMode]));
			}
		}
		if (!domainSet.second.empty())
		{
			out_renderSet.Elements.emplace_back(std::move(domainSet));
		}
	}
}

void alm::gfx::RenderView::UpdateVisibilityShaderBuffer(const RenderSet& renderSet, gfx::MultiBuffer& multiBuffer,
	rhi::ICommandList* commandList, const char* marker)
{
	UploadBuffer* uploadBuffer = m_DeviceManager->GetUploadBuffer();

	size_t reqSize = std::ranges::distance(renderSet.AllInstances()) * sizeof(uint32_t);
	if (reqSize == 0)
		return;

	multiBuffer.Grow(reqSize * 2);

	if(marker)
		commandList->BeginMarker(marker);

	rhi::BufferHandle buffer = multiBuffer.GetCurrentBuffer();
	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

	// Copy to upload data
	auto [data, offset] = uploadBuffer->RequestSpaceForBufferDataUpload(reqSize);

	uint32_t* ptr = (uint32_t*)data;
	for (const RenderableDrawInfo& drawInfo : renderSet.AllInstances())
	{
		*ptr = drawInfo.InstanceIdx;
		ptr++;
	}

	// Copy to buffer
	commandList->CopyBufferToBuffer(buffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, reqSize);

	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

	if (marker)
		commandList->EndMarker();
}

#if 0
alm::aabox3d alm::gfx::RenderView::BuildCloudsShadowVolume() const
{
	static constexpr std::pair<uint8_t, uint8_t> kFrustumEdges[] =
	{
		{0,1}, {1,3}, {3,2}, {2,0},   // near quad
		{4,5}, {5,7}, {7,6}, {6,4},   // far quad
		{0,4}, {1,5}, {2,6}, {3,7},   // connecting
	};

	const AtmosphereConfig* atmos = m_Scene->GetAtmosphereConfig();
	if (!atmos->CloudsSubsystemInitialized())
		return aabox3d::get_empty();

	const double3 earthCenter = atmos->EarthCenter;
	const double earthRadius = atmos->EarthRadius;
	const double innerRadius = atmos->EarthRadius + atmos->CloudsShape.CloudsLayerMinH;
	const double outerRadius = atmos->EarthRadius + atmos->CloudsShape.CloudsLayerMaxH;
	const double3 sunDir = atmos->GetSunDirection();
	const double3 cameraPos = m_Camera->GetPosition();
	const double maxShadowDist = atmos->Clouds.CloudsShadowMaxDistance * 100.f;
	const std::array<float3, 8> frustumCorners = m_Camera->GetWorldFrustumCorners();

	// Prepare point list to construct AABB
	std::vector<double3> pointsD;
	pointsD.reserve(8 * 6);

	auto addRaySphereShellIntersections = [&](const double3& p)
	{
		const double3 rayDir = -sunDir;

		if (auto hit = RayShellOutsideEarth(p, rayDir, earthCenter, earthRadius, innerRadius, outerRadius))
		{
			// Cap por longitud máxima — evita que el rayo vaya al horizonte lejano
			double tNear = std::max(hit->x, 0.0);
			double tFar = std::min(hit->y, maxShadowDist);

			if (tNear < tFar)
			{
				pointsD.push_back(p + rayDir * tNear);
				pointsD.push_back(p + rayDir * tFar);
			}
		}
	};

	auto addShellSegmentIntersections = [&](const double3& a, const double3& b)
	{
		const double3 d = b - a;
		const double len = glm::length(d);
		const double3 dir = d / len;

		if (auto hit = RayShellOutsideEarth(a, dir, earthCenter, earthRadius, innerRadius, outerRadius))
		{
			if (hit->x >= 0.0 && hit->x <= len)
				pointsD.push_back(a + dir * hit->x);
			if (hit->y >= 0.0 && hit->y <= len)
				pointsD.push_back(a + dir * hit->y);
		}
	};

	auto clipCorner = [&](const double3& corner) -> double3
	{
		double3 dir = corner - cameraPos;
		double len = glm::length(dir);
		double3 normDir = dir / len;
		double tMax = std::min(len, maxShadowDist);

		auto tOuter = RaySphereIntersection(cameraPos, normDir, earthCenter, outerRadius);
		if (tOuter && tOuter->y > 0.0 && tOuter->y < tMax)
			tMax = tOuter->y;

		auto tEarth = RaySphereIntersection(cameraPos, normDir, earthCenter, earthRadius);
		if (tEarth && tEarth->x > 0.0 && tEarth->x < tMax)
		{
			tMax = tEarth->x;
		}

		return cameraPos + normDir * tMax;
	};

	// Clip frustum corners
	std::array<double3, 8> clippedCorners;
	for (int i = 0; i < 8; ++i)
	{
		clippedCorners[i] = clipCorner(frustumCorners[i]);
	}

	// --- 1. Add frustum corners that lie inside the shell

	for (const auto& p : frustumCorners)
	{
		const double dist = glm::length(double3{ p } - earthCenter);
		if (dist >= innerRadius && dist <= outerRadius)
			pointsD.push_back(p);
	}

	// --- 2. Iterate 12 frustum edges, intersect each with inner and outer spheres,
	//		  add the segment-clipping intersection points. Also add frustum corners
	//        that lie inside the shell (vertices of the intersection region).

	for (auto [i, j] : kFrustumEdges)
	{
		addShellSegmentIntersections(double3{ clippedCorners[i] }, double3{ clippedCorners[j] });
	}

	// --- 3. Add clouds that are casters for the frustum (even if no geometry visible)
	
	for (const auto& p : clippedCorners)
	{
		addRaySphereShellIntersections(double3{ p });
	}

	aabox3d result{ aabox3d::InitEmpty };
	for (const auto& p : pointsD)
		result.merge(p);

	return result;
}
#else
alm::aabox3d alm::gfx::RenderView::BuildCloudsShadowVolume() const
{
	ZoneScoped;

	const AtmosphereConfig* atmos = m_Scene->GetAtmosphereConfig();
	if (!atmos->CloudsSubsystemInitialized())
		return aabox3d::get_empty();

	const double3 earthCenter = atmos->EarthCenter;
	const double earthRadius = atmos->EarthRadius;
	const double innerRadius = atmos->EarthRadius + atmos->CloudsShape.CloudsLayerMinH;
	const double outerRadius = atmos->EarthRadius + atmos->CloudsShape.CloudsLayerMaxH;
	const double3 sunDir = atmos->GetSunDirection();
	const double3 cameraPos = m_Camera->GetPosition();

	// Reach along the view direction: at least the cloud layer tangent over the
	// ground (beyond it no cloud can cast onto visible terrain), and never less
	// than the configured value

	// Pitagoras: Tangent to a sphere.
	// Leg: d2 = a^2 − b^2 = (a + b) * (a - b)
	const double tangentDist = glm::sqrt((outerRadius + earthRadius) * (outerRadius - earthRadius));
	const double viewDistCap = tangentDist * 1.05;

	// Camera ray directions out of the frustum corners. The far corners are used on
	// purpose: the near corners sit ~1cm from the camera, so subtracting the float
	// camera position would drown the direction in quantization noise.
	const std::array<float3, 8> frustumCorners = m_Camera->GetWorldFrustumCorners();
	std::array<double3, 4> cornerDirs{};
	for (int i = 0; i < 4; ++i)
		cornerDirs[i] = glm::normalize(double3{ frustumCorners[4 + i] } - cameraPos);

	// The frustum is sampled as a regular UV grid of camera rays.
	constexpr uint32_t kGridSize = 16;
	constexpr uint32_t kRaySampleCount = 12;

	std::vector<double3> pointsD;
	pointsD.reserve(kGridSize * kGridSize * (kRaySampleCount + 1) * 3);

	// Clip the ray against the Earth
	auto clipRay = [&](const double3& origin, const double3& dir, double maxT) -> double
	{
		double t = maxT;
		auto tEarth = alm::RaySphereIntersection(origin, dir, earthCenter, earthRadius);
		if (tEarth && tEarth->x > 0.0 && tEarth->x < t)
			t = tEarth->x;
		return t;
	};

	// Adds the sample itself (if within the layer's reach) plus its cloud envelope
	// towards the sun, so the volume's near plane ends up right above the cloud
	// layer instead of flying up with the sky-bound rays.
	auto pushSampleColumn = [&](const double3& p)
	{
		if (glm::length(p - earthCenter) <= outerRadius)
			pointsD.push_back(p);

		const double3 sunRayDir = -sunDir;
		if (auto hit = RayShellOutsideEarth(p, sunRayDir, earthCenter, earthRadius, innerRadius, outerRadius))
		{
			const double tNear = glm::max(hit->x, 0.0);
			const double tFar = glm::min(hit->y, viewDistCap);
			if (tNear < tFar)
			{
				pointsD.push_back(p + sunRayDir * tNear);
				pointsD.push_back(p + sunRayDir * tFar);
			}
		}
	};

	// Bilinear fan over the corner directions: corners come in order (-,-), (+,-), (-,+), (+,+) in clip space.
	for (uint32_t j = 0; j < kGridSize; ++j)
	{
		const double v = (double(j) + 0.5) / double(kGridSize);
		const double3 dirL = glm::normalize(glm::mix(cornerDirs[0], cornerDirs[2], v));
		const double3 dirR = glm::normalize(glm::mix(cornerDirs[1], cornerDirs[3], v));

		for (uint32_t i = 0; i < kGridSize; ++i)
		{
			const double u = (double(i) + 0.5) / double(kGridSize);
			const double3 dir = glm::normalize(glm::mix(dirL, dirR, u));

			// Depth samples along the ray up to its Earth hit (or the distance cap),
			// so the volume covers the receiver region this ray looks at.
			const double hitT = clipRay(cameraPos, dir, viewDistCap);
			for (uint32_t s = 0; s <= kRaySampleCount; ++s)
			{
				const double t = hitT * (double(s) / double(kRaySampleCount));
				pushSampleColumn(cameraPos + dir * t);
			}
		}
	}

	aabox3d result{ aabox3d::InitEmpty };
	for (const auto& p : pointsD)
		result.merge(p);

	return result;
}
#endif

void alm::gfx::RenderView::BuildSunSpaceShadowMatrices(const aabox3d& shadowVolumeWorld,
	float4x4* opt_out_worldToClip, float4x4* opt_out_viewToShadowClip, float4x4* opt_out_clipToTranslatedWorld,
	float3* opt_out_sunPos, float* opt_out_zNear) const
{
	const AtmosphereConfig* atmos = m_Scene->GetAtmosphereConfig();
	const double3 sunDir = atmos->GetSunDirection();

	// Initial sun view matrix (sunPos will be slid later)
	double3 sunPos = shadowVolumeWorld.center();
	const double3 sunUp = fabs(glm::dot(sunDir, { 0, 1, 0 })) > 0.999f ?
		double3(0, 0, 1) : double3(0, 1, 0);
	double4x4 sunViewMatrixD = glm::lookAtRH(sunPos, sunPos + sunDir, sunUp);

	// Transform shadow volume to sun-space
	aabox3d sceneBoundsSun = shadowVolumeWorld.transform(sunViewMatrixD);

	// Slide sunPos so zNear >= 0 (reversed-Z: max.z in sun-space <= 0)
	sunPos -= sunDir * sceneBoundsSun.max.z;
	sunViewMatrixD = glm::lookAtRH(sunPos, sunPos + sunDir, sunUp);
	sceneBoundsSun = shadowVolumeWorld.transform(sunViewMatrixD);
	// Safety net for residual numerical errors
	sceneBoundsSun.max.z = std::min(sceneBoundsSun.max.z, 0.0);

	const double zNearD = -sceneBoundsSun.max.z;
	const double zFarD = -sceneBoundsSun.min.z;
	assert(zNearD >= 0.0);
	assert(zFarD >= zNearD);

	// Ortho projection (reversed-Z)
	const double4x4 sunProjMatrixD = BuildOrthoInvZ_d(
		sceneBoundsSun.min.x, sceneBoundsSun.max.x,
		sceneBoundsSun.min.y, sceneBoundsSun.max.y,
		zNearD, zFarD);

	// Combined matrices (double -> float)
	const double4x4 sunWorldToClipD = sunProjMatrixD * sunViewMatrixD;
	const double4x4 cameraViewMatrixD = m_Camera->GetViewMatrix();
	const double4x4 viewToShadowMapClipD = sunWorldToClipD * glm::inverse(cameraViewMatrixD);

	if (opt_out_worldToClip)
	{
		*opt_out_worldToClip = float4x4{ sunWorldToClipD };
	}

	if (opt_out_viewToShadowClip)
	{
		*opt_out_viewToShadowClip = float4x4{ viewToShadowMapClipD };
	}

	if (opt_out_clipToTranslatedWorld)
	{
		float4x4 invView = glm::inverse(sunViewMatrixD);
		invView[3] = float4(0.f, 0.f, 0.f, 1.f);   // zero translation
		float4x4 invProj = glm::inverse(sunProjMatrixD);
		*opt_out_clipToTranslatedWorld = invView * invProj;
	}

	if (opt_out_sunPos)
	{
		*opt_out_sunPos = sunPos;
	}

	if (opt_out_zNear)
	{
		*opt_out_zNear = zNearD;
	}
}
