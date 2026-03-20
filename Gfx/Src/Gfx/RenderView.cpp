#include "Gfx/RenderView.h"
#include "Gfx/RenderStage.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "RHI/Device.h"
#include "Core/Log.h"
#include "Interop/RenderResources.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneLights.h"
#include "Gfx/UploadBuffer.h"
#include "Gfx/Util.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/RenderGraph.h"

st::gfx::RenderView::RenderView(DeviceManager* deviceManager, const char* debugName) :
	RenderView{ nullptr, deviceManager, debugName }
{}

st::gfx::RenderView::RenderView(ViewportSwapChainId viewportId, DeviceManager* deviceManager, const char* debugName) :
	m_ViewportSwapChainId{ viewportId },
	m_TimeDeltaSec{ 0.f },
	m_DebugName{ debugName },
	m_DeviceManager{ deviceManager }
{
	rhi::Device* device = m_DeviceManager->GetDevice();

	rhi::CommandListParams params{
		.queueType = rhi::QueueType::Graphics
	};
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
	{
		m_BeginCommandLists.push_back(device->CreateCommandList(
			params, std::format("{} - BeginCmdList[{}]", m_DebugName, i)));
		m_EndCommandLists.push_back(device->CreateCommandList(
			params, std::format("{} - EndCmdList[{}]", m_DebugName, i)));
	}

	m_SceneConstants.InitUniformBuffer(sizeof(interop::SceneConstants), m_DeviceManager, "SceneUniformBuffer");

	m_CameraVisibleBuffer.InitRaw(st::rhi::BufferShaderUsage::ReadOnly, 0, rhi::ResourceState::SHADER_RESOURCE,
		m_DeviceManager, "CameraVisibleIndices");
	m_ShadowMapVisibleBuffer.InitRaw(st::rhi::BufferShaderUsage::ReadOnly, 0, rhi::ResourceState::SHADER_RESOURCE,
		m_DeviceManager, "ShadowMapVisibleIndices");
	m_DirLightsVisibleBuffer.InitStructured(st::rhi::BufferShaderUsage::ReadOnly, 0, sizeof(interop::DirLightData),
		rhi::ResourceState::SHADER_RESOURCE, m_DeviceManager, "DirLightsVisibleBuffer");
	m_PointLightsVisibleBuffer.InitStructured(st::rhi::BufferShaderUsage::ReadOnly, 0, sizeof(interop::PointLightData),
		rhi::ResourceState::SHADER_RESOURCE, m_DeviceManager, "PointLightsVisibleBuffer");
	m_SpotLightsVisibleBuffer.InitStructured(st::rhi::BufferShaderUsage::ReadOnly, 0, sizeof(interop::SpotLightData),
		rhi::ResourceState::SHADER_RESOURCE, m_DeviceManager, "SpotLightsVisibleBuffer");

	m_RenderGraph = st::make_unique_with_weak<RenderGraph>(this, debugName);
}

st::gfx::RenderView::~RenderView()
{
	m_RenderGraph.reset();
}

void st::gfx::RenderView::SetScene(st::weak<Scene> scene)
{
	m_CameraVisibleBuffer.Reset();
	m_ShadowMapVisibleBuffer.Reset();
	m_DirLightsVisibleBuffer.Reset();
	m_PointLightsVisibleBuffer.Reset();
	m_SpotLightsVisibleBuffer.Reset();

	m_Scene = scene;

	m_RenderGraph->OnSceneChanged();
}

void st::gfx::RenderView::SetCamera(std::shared_ptr<st::gfx::Camera> camera)
{
	m_Camera = camera;
}

void st::gfx::RenderView::SetOffscreenFrameBuffer(st::rhi::FramebufferHandle frameBuffer)
{
	m_OffscreenFramebuffer = frameBuffer;
}

st::rhi::FramebufferHandle st::gfx::RenderView::GetFramebuffer()
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

st::rhi::TextureHandle st::gfx::RenderView::GetBackBuffer(int idx)
{
	return GetFramebuffer()->GetBackBuffer(idx);
}

st::rhi::BufferUniformView st::gfx::RenderView::GetSceneBufferUniformView()
{
	return m_SceneConstants.GetUniformView();
}

st::rhi::BufferReadOnlyView st::gfx::RenderView::GetCameraVisiblityBufferROView()
{
	return m_CameraVisibleBuffer.GetReadOnlyView();
}

st::rhi::BufferReadOnlyView st::gfx::RenderView::GetShadowMapVisibilityBufferROView()
{
	return m_ShadowMapVisibleBuffer.GetReadOnlyView();
}

void st::gfx::RenderView::OnWindowSizeChanged()
{
	// Actually we are only interested in this if we are rendering to the main swap chain BB
	if (!m_OffscreenFramebuffer && !m_ViewportSwapChainId)
	{
		const auto newSize = m_DeviceManager->GetWindowDimensions();
		m_RenderGraph->OnRenderTargetChanged(newSize);
	}
}

void st::gfx::RenderView::Render(float timeDeltaSec)
{
	st::rhi::FramebufferHandle frameBuffer = GetFramebuffer();
	if (!frameBuffer)
	{
		LOG_ERROR("No frame buffer specified. Nothing to render");
		return;
	}

	rhi::ICommandList* beginCommandList = m_BeginCommandLists[m_DeviceManager->GetFrameModuleIndex()].get();
	beginCommandList->Open();
	beginCommandList->BeginMarker(m_DebugName.c_str());
	beginCommandList->BeginMarker("Begin commands");

	// Update common data
	UpdateCameraVisibleSet(beginCommandList);
	UpdateShadowmapData(beginCommandList);
	UpdateDirLightsVisibleBuffer(beginCommandList);
	UpdatePointLightsVisibleBuffer(beginCommandList);
	UpdateSpotLightsVisibleBuffer(beginCommandList);

	UpdateSceneConstantBuffer();

	m_TimeDeltaSec = timeDeltaSec;

	// Back buffer is in COMMON state and need to be transitioned to RT
	beginCommandList->PushBarrier(rhi::Barrier::Texture(
		frameBuffer->GetDesc().ColorAttachments[0].texture.get(), rhi::ResourceState::PRESENT, rhi::ResourceState::RENDERTARGET));

	// Give the oportunity to the rendergraph to do some initial setup using beginCommandList
	m_RenderGraph->BeginRender(beginCommandList);

	// Done with beginCommandList
	beginCommandList->EndMarker();
	beginCommandList->Close();
	m_DeviceManager->GetDevice()->ExecuteCommandList(beginCommandList, st::rhi::QueueType::Graphics);

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
	m_DeviceManager->GetDevice()->ExecuteCommandList(endCommandList, st::rhi::QueueType::Graphics);
}

void st::gfx::RenderView::UpdateSceneConstantBuffer()
{
	interop::SceneConstants* sceneShaderConstant = (interop::SceneConstants*)m_SceneConstants.Map();
	*sceneShaderConstant = {};

	// Screen resolution
	sceneShaderConstant->invScreenResolution = float2{
		1.f / GetFramebuffer()->GetFramebufferInfo().width,
		1.f / GetFramebuffer()->GetFramebufferInfo().height };
	
	// Camera
	if (m_Camera)
	{
		sceneShaderConstant->camViewProjMatrix = m_Camera->GetViewProjectionMatrix();
		sceneShaderConstant->camViewMatrix = m_Camera->GetViewMatrix();
		sceneShaderConstant->camProjMatrix = m_Camera->GetProjectionMatrix();
		sceneShaderConstant->camWorldPos = float4{ m_Camera->GetPosition(), 0.f };
	}
	else
	{
		sceneShaderConstant->camViewProjMatrix = float4x4{ 1.f };
		sceneShaderConstant->camViewMatrix = float4x4{ 1.f };
		sceneShaderConstant->camProjMatrix = float4x4{ 1.f };
		sceneShaderConstant->camWorldPos = float4{ 0.f };
	}
	sceneShaderConstant->invCamViewProjMatrix = glm::inverse(sceneShaderConstant->camViewProjMatrix);
	sceneShaderConstant->invCamViewMatrix = glm::inverse(sceneShaderConstant->camViewMatrix);
	sceneShaderConstant->invCamProjMatrix = glm::inverse(sceneShaderConstant->camProjMatrix);

	// Scene data
	if (m_Scene)
	{
		// ShadowMap matrices
		{
			sceneShaderConstant->shadowMapWorldToClipMatrix = m_ShadowMapWoldToClipMatrix;
			sceneShaderConstant->shadowMapViewToClipMatrix = m_ViewToShadowMapClipMatrix;
		}

		// Ambient
		{
			const Scene::AmbientParams& ambientParams = m_Scene->GetAmbientParams();
			sceneShaderConstant->ambientTop = float4{ ambientParams.SkyColor * ambientParams.Intensity, 0.f };
			sceneShaderConstant->ambientBottom = float4{ ambientParams.GroundColor * ambientParams.Intensity, 0.f };
		}

		// Lights
		{
			const Scene::SunParams& sunParams = m_Scene->GetSunParams();
			const float3 sunDir = st::ElevationAzimuthRadToDir(
				glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg));

			sceneShaderConstant->mainDirLight.viewSpaceDirection = m_Camera->GetViewMatrix() * float4 { sunDir, 0.f };
			sceneShaderConstant->mainDirLight.irradiance = sunParams.Irradiance;
			sceneShaderConstant->mainDirLight.color = sunParams.Color;
			sceneShaderConstant->mainDirLight.halfAngularSize = glm::radians(sunParams.AngularSizeDeg) / 2.f;

			sceneShaderConstant->dirLightCount = m_DirLightsVisibleCount;
			sceneShaderConstant->dirLightsDataDI = m_DirLightsVisibleBuffer.GetReadOnlyView();
			sceneShaderConstant->pointLightCount = m_PointLightsVisibleCount;
			sceneShaderConstant->pointLightsDataDI = m_PointLightsVisibleBuffer.GetReadOnlyView();
			sceneShaderConstant->spotLightCount = m_SpotLightsVisibleCount;
			sceneShaderConstant->spotLightsDataDI = m_SpotLightsVisibleBuffer.GetReadOnlyView();;
		}

		// Data buffers
		sceneShaderConstant->instanceBufferDI = m_Scene->GetInstancesBufferView();
		sceneShaderConstant->meshesBufferDI = m_Scene->GetMeshesBufferView();
		sceneShaderConstant->materialsBufferDI = m_Scene->GetMaterialsBufferView();
	}

	m_SceneConstants.Unmap();
}

void st::gfx::RenderView::UpdateCameraVisibleSet(rhi::ICommandList* commandList)
{
	m_CameraVisibleSet.Elements.clear();
	m_CameraVisibleBounds.reset();
	if (!m_Camera ||
		!m_Scene ||
		!m_Scene->GetSceneGraph())
	{
		return;
	}

	GetVisibleSet(m_Camera->GetFrustum().get_planes(), m_CameraVisibleSet, &m_CameraVisibleBounds);
	UpdateVisibilityShaderBuffer(m_CameraVisibleSet, m_CameraVisibleBuffer, commandList);
}

void st::gfx::RenderView::UpdateShadowmapData(rhi::ICommandList* commandList)
{
	m_ShadowMapVisibleSet.Elements.clear();
	m_ShadowMapWoldToClipMatrix = {};
	m_ViewToShadowMapClipMatrix = {};

	if (!m_CameraVisibleBounds.valid())
		return;

	const Scene::SunParams& sunParams = m_Scene->GetSunParams();
	const float3 sunDir = st::ElevationAzimuthRadToDir(
		glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg));

	const st::math::aabox3f& worldBounds = m_Scene->GetWorldBounds(BoundsType::Mesh);
	const st::math::aabox3f& visibleSceneBounds = m_CameraVisibleBounds;
	const float3 worldCenter = worldBounds.center();
	const float3 worldExtents = worldBounds.extents();

	// View matrix
	const float3 sunPos = worldCenter - (sunDir * glm::length(worldExtents) / 2.f);
	const float3 sunUp = fabs(glm::dot(sunDir, { 0, 1, 0 })) > 0.99f ? float3(0, 0, 1) : float3(0, 1, 0);
	const float4x4 sunViewMatrix = glm::lookAtRH(sunPos, sunPos + sunDir, sunUp);

	// Transform scene (visible set) bounds to local camera axis
	auto sceneBoundsSun = visibleSceneBounds.transform(sunViewMatrix);
	//assert(sceneBoundsSun.min.z <= 0.f); // Note that front is -z
	//assert(sceneBoundsSun.max.z <= 0.f);
	assert(sceneBoundsSun.min.z <= sceneBoundsSun.max.z);

	// Extends the bounds depth to the direction of the sun top cover entire scene
	{
		auto worldBoundsSun = worldBounds.transform(sunViewMatrix);
		//assert(worldBoundsSun.min.z <= 0.f);
		//assert(worldBoundsSun.max.z <= 0.f);
		assert(worldBoundsSun.min.z <= sceneBoundsSun.max.z);

		sceneBoundsSun.min.z = worldBoundsSun.min.z;
		sceneBoundsSun.max.z = worldBoundsSun.max.z;
	}

	// Calc projection matrix

	float zNear = -sceneBoundsSun.max.z;
	float zFar = -sceneBoundsSun.min.z;
	assert(zNear >= 0.0f);
	assert(zFar >= zNear);

	const float4x4 sunProjMatrix = BuildOrthoInvZ(
		sceneBoundsSun.min.x, sceneBoundsSun.max.x, sceneBoundsSun.min.y, sceneBoundsSun.max.y, zNear, zFar);

#ifdef _DEBUG
	//*** TEST
	{
		float4 pNear = sunProjMatrix * float4{ 0.f, 0.f, -zNear, 1.f };
		float4 pFar = sunProjMatrix * float4{ 0.f, 0.f, -zFar, 1.f };
		float zn = pNear.z / pNear.w;
		float zf = pFar.z / pFar.w;
		assert(zn > zf);
		// Ortho proj, W is 1, so p.w should be 1
		assert(AlmostEqual(zn, 1.f) && AlmostEqual(pFar.z, 0.f));
		assert(AlmostEqual(zf, 0.f) && AlmostEqual(pFar.w, 1.f));
	}
#endif

	m_ShadowMapWoldToClipMatrix = sunProjMatrix * sunViewMatrix;

	// view -> world -> sun_view -> sun_clip
	m_ViewToShadowMapClipMatrix = sunProjMatrix * sunViewMatrix * glm::inverse(m_Camera->GetViewMatrix());

	// Visible set from sun
	{
		math::aabox3f aabb = sceneBoundsSun.transform(glm::inverse(sunViewMatrix));
		std::vector<math::plane3f> sunClipPlanes{
			{{ 1.f, 0.f, 0.f }, -aabb.min.x },	// left
			{{ -1.f, 0.f, 0.f }, aabb.max.x },	// right
			{{ 0.f, -1.f, 0.f }, aabb.max.y },	// top
			{{ 0.f, 1.f, 0.f }, -aabb.min.y },	// bottom
			{{ 0.f, 0.f, 1.f }, -aabb.min.z },	// near
			{{ 0.f, 0.f, -1.f }, aabb.max.z },	// far		
		};

		GetVisibleSet(sunClipPlanes, m_ShadowMapVisibleSet, nullptr);
		UpdateVisibilityShaderBuffer(m_ShadowMapVisibleSet, m_ShadowMapVisibleBuffer, commandList);
	}
}

void st::gfx::RenderView::UpdateDirLightsVisibleBuffer(rhi::ICommandList* commandList)
{
	m_DirLightsVisibleCount = 0;

	if (!m_Scene || !m_Scene->GetSceneGraph())
		return;

	const auto& visibleDirLights = m_Scene->GetSceneGraph()->GetSceneDirLights();
	m_DirLightsVisibleCount = visibleDirLights.size();

	if (m_DirLightsVisibleCount == 0)
		return;

	uint32_t reqSize = m_DirLightsVisibleCount * sizeof(interop::DirLightData);
	m_DirLightsVisibleBuffer.Grow(reqSize * 2);

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
}

void st::gfx::RenderView::UpdatePointLightsVisibleBuffer(rhi::ICommandList* commandList)
{
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

	std::vector<const st::gfx::ScenePointLight*> visiblePointLights;
	st::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (has_any_flag(node->GetContentFlags(), SceneContentFlags::PointLights) &&
			node->HasBounds(BoundsType::Light) && node->Test(BoundsType::Light, frustum.get_planes()))
		{
			auto leaf = node->GetLeaf();
			if (leaf && leaf->GetType() == SceneGraphLeaf::Type::PointLight)
			{
				const auto* pointLight = st::checked_cast<const st::gfx::ScenePointLight*>(leaf.get());
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
}

void st::gfx::RenderView::UpdateSpotLightsVisibleBuffer(rhi::ICommandList* commandList)
{
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

	std::vector<const st::gfx::SceneSpotLight*> visibleSpotLights;
	st::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (has_any_flag(node->GetContentFlags(), SceneContentFlags::SpotLights) &&
			node->HasBounds(BoundsType::Light) && node->Test(BoundsType::Light, frustum.get_planes()))
		{
			auto leaf = node->GetLeaf();
			if (leaf && leaf->GetType() == SceneGraphLeaf::Type::SpotLight)
			{
				const auto* spotLight = st::checked_cast<const st::gfx::SceneSpotLight*>(leaf.get());
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
}

void st::gfx::RenderView::GetVisibleSet(const std::span<const math::plane3f>& planes, RenderSet& out_renderSet, math::aabox3f* opt_outBounds) const
{
	out_renderSet.Elements.clear();

	if (!m_Scene || !m_Scene->GetSceneGraph())
		return;
	
	std::vector<const st::gfx::MeshInstance*> instances[(int)MaterialDomain::_Size][(int)rhi::CullMode::_Size];

	if (opt_outBounds)
		opt_outBounds->reset();

	st::gfx::SceneGraph::Walker walker{ *m_Scene->GetSceneGraph() };
	while (walker)
	{
		auto node = *walker;
		if (has_any_flag(node->GetContentFlags(), SceneContentFlags::OpaqueMeshes) &&
			node->HasBounds(BoundsType::Mesh) && node->Test(BoundsType::Mesh, planes))
		{
			auto leaf = node->GetLeaf();
			if (leaf && leaf->GetType() == SceneGraphLeaf::Type::MeshInstance)
			{
				const auto* meshInstance = st::checked_cast<const st::gfx::MeshInstance*>(leaf.get());
				assert(meshInstance && meshInstance->GetMesh() && meshInstance->GetMesh()->GetMaterial());

				instances[(int)meshInstance->GetMaterialDomain()][(int)meshInstance->GetCullMode()].push_back(meshInstance);

				if(opt_outBounds)
					opt_outBounds->merge(node->GetWorldBounds(BoundsType::Mesh));
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
			std::ranges::sort(instances[domain][cullMode], [](const st::gfx::MeshInstance* a, const st::gfx::MeshInstance* b)
			{
				return a->GetMesh().get() < b->GetMesh().get();
			});
		}
	}

	// Move to result
	for (int domain = 0; domain < (int)MaterialDomain::_Size; ++domain)
	{
		RenderSet::MaterialDomainSet domainSet{ (MaterialDomain)domain, {} };
		for (int cullMode = 0; cullMode < (int)rhi::CullMode::_Size; ++cullMode)
		{
			if (!instances[domain][cullMode].empty())
			{
				domainSet.second.emplace_back((rhi::CullMode)cullMode, std::move(instances[domain][cullMode]));
			}
		}
		if (!domainSet.second.empty())
		{
			out_renderSet.Elements.emplace_back(std::move(domainSet));
		}
	}
}

void st::gfx::RenderView::UpdateVisibilityShaderBuffer(const RenderSet& renderSet, gfx::MultiBuffer& multiBuffer,
	rhi::ICommandList* commandList)
{
	UploadBuffer* uploadBuffer = m_DeviceManager->GetUploadBuffer();

	size_t reqSize = std::ranges::distance(renderSet.AllInstances()) * sizeof(uint32_t);
	if (reqSize == 0)
		return;

	multiBuffer.Grow(reqSize * 2);

	rhi::BufferHandle buffer = multiBuffer.GetCurrentBuffer();
	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

	// Copy to upload data
	auto [data, offset] = uploadBuffer->RequestSpaceForBufferDataUpload(reqSize);

	uint32_t* ptr = (uint32_t*)data;
	for (const st::gfx::MeshInstance* inst : renderSet.AllInstances())
	{
		*ptr = inst->GetLeafSceneIndex();
		ptr++;
	}

	// Copy to buffer
	commandList->CopyBufferToBuffer(buffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, reqSize);

	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));
}