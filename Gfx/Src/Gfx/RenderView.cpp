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

namespace
{

int GetActualSize(int desired, int ref)
{
	if (desired > 0)
	{
		return desired;
	}

	return ref / (-desired + 1);
}

st::rhi::ResourceState GetInitialState(st::gfx::RenderView::TextureResourceType type)
{
	switch (type)
	{
	case st::gfx::RenderView::TextureResourceType::RenderTarget:
		return st::rhi::ResourceState::RENDERTARGET;
	case st::gfx::RenderView::TextureResourceType::DepthStencil:
		return st::rhi::ResourceState::DEPTHSTENCIL;
	case st::gfx::RenderView::TextureResourceType::ShaderResource:
		return st::rhi::ResourceState::SHADER_RESOURCE;
	default:
		assert(0);
		return st::rhi::ResourceState::SHADER_RESOURCE;
	}
}

st::rhi::TextureShaderUsage GetTextureShaderUsage(st::gfx::RenderView::TextureResourceType type, bool needUAV)
{
	st::rhi::TextureShaderUsage usage;
	switch (type)
	{
	case st::gfx::RenderView::TextureResourceType::RenderTarget:
		usage = st::rhi::TextureShaderUsage::Sampled | st::rhi::TextureShaderUsage::ColorTarget;
		break;
	case st::gfx::RenderView::TextureResourceType::DepthStencil:
		usage = st::rhi::TextureShaderUsage::Sampled | st::rhi::TextureShaderUsage::DepthTarget;
		break;
	case st::gfx::RenderView::TextureResourceType::ShaderResource:
		usage = st::rhi::TextureShaderUsage::Sampled;
		break;
	default:		
		assert(0);
		usage = st::rhi::TextureShaderUsage::Sampled;
	}
	if (needUAV)
		usage |= st::rhi::TextureShaderUsage::Storage;
	
	return usage;
}

} // anonymous namespace

st::gfx::RenderView::RenderView(DeviceManager* deviceManager, const char* debugName) :
	RenderView{ nullptr, deviceManager, debugName }
{}

st::gfx::RenderView::RenderView(ViewportSwapChainId viewportId, DeviceManager* deviceManager, const char* debugName) :
	m_ViewportSwapChainId{ viewportId },
	m_SceneCB{ deviceManager->GetSwapchainBufferCount(), sizeof(interop::Scene), deviceManager->GetDevice(), "SceneCB" },
	m_TimeDeltaSec{ 0.f },
	m_IsDirty{ false },
	m_DebugName{ debugName },
	m_DeviceManager{ deviceManager }
{
	rhi::Device* device = m_DeviceManager->GetDevice();

	rhi::CommandListParams params{
		.queueType = rhi::QueueType::Graphics
	};
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
	{
		m_CommandLists.push_back(device->CreateCommandList(params, m_DebugName));

		m_BeginCommandLists.push_back(device->CreateCommandList(
			params, std::format("{} - BeginCmdList[{}]", m_DebugName, i)));
		m_EndCommandLists.push_back(device->CreateCommandList(
			params, std::format("{} - EndCmdList[{}]", m_DebugName, i)));

		m_SubmitFences.push_back(device->CreateFence(0, std::format("{} - Fence[{}]", m_DebugName, i)));
	}

	m_CameraVisibleBuffer.Init(st::rhi::BufferShaderUsage::ReadOnly, 0, rhi::ResourceState::SHADER_RESOURCE,
		m_DeviceManager, "CameraVisibleIndices");
	m_SunVisibleBuffer.Init(st::rhi::BufferShaderUsage::ReadOnly, 0, rhi::ResourceState::SHADER_RESOURCE,
		m_DeviceManager, "SunVisibleIndices");
	m_PointLightsVisibleBuffer.Init(st::rhi::BufferShaderUsage::ReadOnly, 0, rhi::ResourceState::SHADER_RESOURCE,
		m_DeviceManager, "PointLightsVisibleIndices");
}

st::gfx::RenderView::~RenderView()
{
	ClearRenderStages();
	for (int i = 0; i < m_CommandLists.size(); ++i)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(std::move(m_CommandLists[i]));
	}
}

void st::gfx::RenderView::SetScene(st::weak<Scene> scene)
{
	m_CameraVisibleBuffer.Reset();
	m_SunVisibleBuffer.Reset();
	m_PointLightsVisibleBuffer.Reset();

	m_Scene = scene;
	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->OnSceneChanged();
	}
}

void st::gfx::RenderView::SetCamera(std::shared_ptr<st::gfx::Camera> camera)
{
	m_Camera = camera;
}

void st::gfx::RenderView::SetOffscreenFrameBuffer(st::rhi::FramebufferHandle frameBuffer)
{
	m_OffscreenFramebuffer = frameBuffer;
}

void st::gfx::RenderView::SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages)
{
	ClearRenderStages();

	m_RenderStages.reserve(renderStages.size());
	for (auto& rs : renderStages)
	{
		auto rsd = new RenderStageData;
		rsd->renderStage = rs;
		rsd->timerQueries.reserve(m_DeviceManager->GetSwapchainBufferCount());
		rsd->cpuElapsed.resize(8, 0.f); // 8 samples
		m_RenderStages.emplace_back(rsd);
	}

	// Init timer queries
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
	{
		for (auto& rsd : m_RenderStages)
		{
			rsd->timerQueries.push_back(m_DeviceManager->GetDevice()->CreateTimerQuery(
				std::format("{} - TimerQuery[{}]", rsd->renderStage->GetDebugName(), i)));
		}
	}

	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->Attach(this);
	}

	m_IsDirty = true;
}

void st::gfx::RenderView::SetRenderMode(const std::string& name, const std::vector<RenderStage*>& renderStages)
{
	if (m_RenderModes.find(name) != m_RenderModes.end())
	{
		m_RenderModes.erase(name);
	}

	std::vector<RenderStageData*> stages;
	stages.reserve(renderStages.size());

	for (auto* rs : renderStages)
	{
		// Check that the stage actually belongs to this
		auto it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rs](const std::unique_ptr<RenderStageData>& rsd) -> bool
		{
			return rs == rsd->renderStage.get();
		});
		if (it == m_RenderStages.end())
		{
			LOG_ERROR("Render stage '{}' is not part of the render view '{}'", rs->GetDebugName(), m_DebugName);
			return;
		}
		stages.push_back(it->get());
	}

	m_RenderModes[name] = std::move(stages);

	if (m_CurrentRenderMode.empty())
		m_CurrentRenderMode = name;
}

void st::gfx::RenderView::SetCurrentRenderMode(const std::string& name)
{
	if (m_RenderModes.find(name) == m_RenderModes.end())
	{
		LOG_WARNING("Render mode '{}' not found", name);
		return;
	}
	m_CurrentRenderMode = name;
}

std::vector<std::string> st::gfx::RenderView::GetRenderModes() const
{
	std::vector<std::string> result;
	result.reserve(m_RenderModes.size());
	for (const auto& rm : m_RenderModes)
	{
		result.push_back(rm.first);
	}
	return result;
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

st::rhi::CommandListHandle st::gfx::RenderView::GetCommandList()
{
	return m_CommandLists[m_DeviceManager->GetFrameModuleIndex()].get_weak();
}

st::rhi::BufferUniformView st::gfx::RenderView::GetSceneBufferUniformView()
{
	return m_SceneCB.GetUniformView();
}

st::rhi::BufferReadOnlyView st::gfx::RenderView::GetCameraVisiblityBufferROView()
{
	return m_CameraVisibleBuffer.GetCurrentBuffer()->GetReadOnlyView();
}

st::rhi::BufferReadOnlyView st::gfx::RenderView::GetSunVisibilityBufferROView()
{
	return m_SunVisibleBuffer.GetCurrentBuffer()->GetReadOnlyView();
}

bool st::gfx::RenderView::CreateColorTarget(const char* id, int width, int height, int arraySize, rhi::Format format)
{
	return CreateTexture(id, TextureResourceType::RenderTarget, width, height, arraySize, format, false);
}

bool st::gfx::RenderView::CreateDepthTarget(const char* id, int width, int height, int arraySize, rhi::Format format)
{
	return CreateTexture(id, TextureResourceType::DepthStencil, width, height, arraySize, format, false);
}

bool st::gfx::RenderView::CreateTexture(const char* id, TextureResourceType type, int width, int height, int arraySize, rhi::Format format, bool needsUAV)
{
	// Check if that texture is already created
	auto it = m_DeclaredTextures.find(id);
	if (it != m_DeclaredTextures.end())
	{
		// No problem, this is allowed as far as the texture properties match
		const auto& desc = it->second->texture->GetDesc();
		if (it->second->requestedWidth == width && it->second->requestedHeight == height && it->second->type == type && desc.arraySize == arraySize &&
			desc.format == format && needsUAV == has_any_flag(desc.shaderUsage, rhi::TextureShaderUsage::Storage))
		{
			return true;
		}
		else
		{
			LOG_ERROR("Texture with id {} already created", id);
			return false;
		}
	}

	rhi::TextureDesc desc{
		.width = (uint32_t)GetActualSize(width, GetFramebuffer()->GetFramebufferInfo().width),
		.height = (uint32_t)GetActualSize(height, GetFramebuffer()->GetFramebufferInfo().height),
		.arraySize = (uint32_t)arraySize,
		.format = format,
		.shaderUsage = GetTextureShaderUsage(type, needsUAV) };

	rhi::TextureOwner texture = m_DeviceManager->GetDevice()->CreateTexture(desc, GetInitialState(type), id);
	m_DeclaredTextures.insert({ id, std::make_unique<DeclaredTexture>(std::move(texture), type, width, height) });

	return true;
}

bool st::gfx::RenderView::RecreateTexture(const char* id, int width, int height, int arraySize, rhi::Format format)
{
	// Check that texture has been already created
	auto it = m_DeclaredTextures.find(id);
	if (it == m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} does not exist", id);
		return false;
	}

	rhi::TextureDesc desc = it->second->texture->GetDesc();
	desc.width = width;
	desc.height = height;
	desc.arraySize = arraySize;
	desc.format = format;

	rhi::TextureOwner newTexture = m_DeviceManager->GetDevice()->CreateTexture(desc, GetInitialState(it->second->type), id);
	rhi::TextureOwner& oldTexture = it->second->texture;

	// Swap
	oldTexture->Swap(*newTexture.get());

	// newTexture (actually the old old since it has been swap-ed) would be released when the owner pointer gets out of scope
	// but lets do it explicitly
	m_DeviceManager->GetDevice()->ReleaseQueued(std::move(newTexture));

	it->second->requestedWidth = width;
	it->second->requestedHeight = height;

	return true;
}

bool st::gfx::RenderView::ReleaseTexture(const char* id)
{
	// Check that the texture exists
	auto it = m_DeclaredTextures.find(id);
	if (it == m_DeclaredTextures.end())
	{
		LOG_ERROR("Texture with id {} does not exist", id);
		return false;
	}

	m_DeclaredTextures.erase(it);
	return true;
}

bool st::gfx::RenderView::CreateBuffer(const std::string& id, const rhi::BufferDesc& desc)
{
	// Check that texture has not been already created
	auto it = m_DeclaredBuffers.find(id);
	if (it != m_DeclaredBuffers.end())
	{
		LOG_ERROR("Buffer with id {} already created", id);
		return false;
	}

	rhi::BufferOwner buffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE, id);
	m_DeclaredBuffers.insert({ id, std::make_unique<DeclaredBuffer>(std::move(buffer)) });

	return true;
}

bool st::gfx::RenderView::RecreateBuffer(const std::string& id, const rhi::BufferDesc& desc)
{
	// Check that texture has not been already created
	auto it = m_DeclaredBuffers.find(id);
	if (it == m_DeclaredBuffers.end())
	{
		LOG_ERROR("Buffer with id {} does not exists", id);
		return false;
	}

	// Create new buffer
	rhi::BufferOwner newBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE, id);
	rhi::BufferOwner& oldBuffer = it->second->buffer;

	// Swap
	oldBuffer->Swap(*newBuffer.get());

	return true;
}

bool st::gfx::RenderView::ReleaseBuffer(const std::string& id)
{
	// Check that texture has not been already created
	auto it = m_DeclaredBuffers.find(id);
	if (it == m_DeclaredBuffers.end())
	{
		LOG_ERROR("Buffer with id {} does not exists", id);
		return false;
	}

	m_DeclaredBuffers.erase(it);
	return true;
}

bool st::gfx::RenderView::RequestTextureAccess(RenderStage* rs, AccessMode accessMode, const std::string& id,
	rhi::ResourceState inputState, rhi::ResourceState outputState)
{
	// Find render pass deps
	auto rs_it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rs](const auto& entry) -> bool
		{
			return entry->renderStage.get() == rs;
		});
	if (rs_it == m_RenderStages.end())
	{
		LOG_ERROR("Render pass with debug name {} not registered", rs->GetDebugName());
		return false;
	}

	// Remove prev dependency if exists
	auto removeDeps = [&id](std::vector<RenderStageResourceDep>& deps)
	{
		auto it = std::find_if(deps.begin(), deps.end(), [&id](const RenderStageResourceDep& dep)
			{ return dep.id == id; });
		if (it != deps.end())
		{
			deps.erase(it);
		}
	};
	removeDeps((*rs_it)->textureReads);
	removeDeps((*rs_it)->textureWrites);

	// Add new dependency
	if (accessMode == AccessMode::Read)
	{
		(*rs_it)->textureReads.emplace_back(id, inputState, outputState);
	}
	else
	{
		(*rs_it)->textureWrites.emplace_back(id, inputState, outputState);
	}

	return true;
}

bool st::gfx::RenderView::RequestBufferAccess(RenderStage* rs, AccessMode accessMode, const std::string& id, 
	rhi::ResourceState inputState, rhi::ResourceState outputState)
{
	// Find render pass deps
	auto rs_it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rs](const auto& entry) -> bool
		{
			return entry->renderStage.get() == rs;
		});
	if (rs_it == m_RenderStages.end())
	{
		LOG_ERROR("Render pass with debug name {} not registered", rs->GetDebugName());
		return false;
	}

	// Remove prev dependency if exists
	auto removeDeps = [&id](std::vector<RenderStageResourceDep>& deps)
	{
		auto it = std::find_if(deps.begin(), deps.end(), [&id](const RenderStageResourceDep& dep)
			{ return dep.id == id; });
		if (it != deps.end())
		{
			deps.erase(it);
		}
	};
	removeDeps((*rs_it)->bufferReads);
	removeDeps((*rs_it)->bufferWrites);

	// Add new dependency
	if (accessMode == AccessMode::Read)
	{
		(*rs_it)->bufferReads.emplace_back(id, inputState, outputState);
	}
	else
	{
		(*rs_it)->bufferWrites.emplace_back(id, inputState, outputState);
	}

	return true;
}

st::rhi::TextureHandle st::gfx::RenderView::GetTexture(const std::string& id) const
{
	auto texture_it = m_DeclaredTextures.find(id);
	if (texture_it != m_DeclaredTextures.end())
	{
		return texture_it->second->texture.get_weak();
	}
	return nullptr;
}

st::rhi::BufferHandle st::gfx::RenderView::GetBuffer(const std::string& id) const
{
	auto buffer_it = m_DeclaredBuffers.find(id);
	if (buffer_it != m_DeclaredBuffers.end())
	{
		return buffer_it->second->buffer.get_weak();
	}
	return nullptr;
}

st::rhi::TextureSampledView st::gfx::RenderView::GetTextureSampledView(const std::string& id)
{
	auto tex = GetTexture(id);
	if (tex)
	{
		return tex->GetSampledView();
	}
	return {};
}

st::rhi::TextureStorageView st::gfx::RenderView::GetTextureStorageView(const std::string& id)
{
	auto tex = GetTexture(id);
	if (tex)
	{
		return tex->GetStorageView();
	}
	return {};
}

st::rhi::BufferUniformView st::gfx::RenderView::GetBufferUniformView(const std::string& id)
{
	auto buffer = GetBuffer(id);
	if (buffer)
	{
		return buffer->GetUniformView();
	}
	return {};
}

st::rhi::BufferReadOnlyView st::gfx::RenderView::GetBufferReadOnlyView(const std::string& id)
{
	auto buffer = GetBuffer(id);
	if (buffer)
	{
		return buffer->GetReadOnlyView();
	}
	return {};
}

st::rhi::BufferReadWriteView st::gfx::RenderView::GetBufferReadWriteView(const std::string& id)
{
	auto buffer = GetBuffer(id);
	if (buffer)
	{
		return buffer->GetReadWriteView();
	}
	return {};
}

void st::gfx::RenderView::OnWindowSizeChanged()
{
	// Actually we are only interested in this if we are rendering to the swap chain BB
	if (!m_OffscreenFramebuffer)
	{
		const auto newSize = m_DeviceManager->GetWindowDimensions();
		// Update all the textures whose size is dependant on BB size
		for (auto& it : m_DeclaredTextures)
		{
			auto& declTex = it.second;
			if (declTex->requestedWidth <= 0 || declTex->requestedHeight <= 0)
			{
				rhi::TextureDesc newDesc = declTex->texture->GetDesc();
				newDesc.width = GetActualSize(declTex->requestedWidth, newSize.x);
				newDesc.height = GetActualSize(declTex->requestedHeight, newSize.y);
				
				m_DeviceManager->GetDevice()->ReleaseImmediately(std::move(declTex->texture));
				declTex->texture = m_DeviceManager->GetDevice()->CreateTexture(newDesc, 
					GetInitialState(declTex->type), it.first);
			}
		}

		// Forward to render stages
		for (auto& rs : m_RenderStages)
		{
			rs->renderStage->OnBackbufferResize();
		}

		// Recreate texture view requests
		for (auto* req : m_TexViewRequests)
		{
			m_DeviceManager->GetDevice()->ReleaseImmediately(std::move(req->tex));
		}
	}

	// TODO: Check if we are resized (or changed) the offscreen BB
}

void st::gfx::RenderView::Render(float timeDeltaSec)
{
	st::rhi::FramebufferHandle frameBuffer = GetFramebuffer();
	if (!frameBuffer)
	{
		LOG_ERROR("No frame buffer specified. Nothing to render");
		return;
	}

	if (m_IsDirty)
	{
		Refresh();
	}

	// Get render mode
	if (m_RenderModes.empty() || m_CurrentRenderMode.empty())
	{
		LOG_WARNING("Render mode not set");
		return;
	}
	auto it = m_RenderModes.find(m_CurrentRenderMode);
	if (it == m_RenderModes.end())
	{
		LOG_ERROR("Render mode set '{}' not defined", m_CurrentRenderMode);
		return;
	}
	const std::vector<RenderStageData*>& renderStages = it->second;

	rhi::ICommandList* beginCommandList = m_BeginCommandLists[m_DeviceManager->GetFrameModuleIndex()].get();
	beginCommandList->Open();
	beginCommandList->BeginMarker(m_DebugName.c_str());
	beginCommandList->BeginMarker("Begin commands");

	// Update common data
	UpdateCameraVisibleSet(beginCommandList);
	UpdateShadowmapData(beginCommandList);
	UpdateLightsVisibleSet(beginCommandList);

	UpdateSceneConstantBuffer();

	m_TimeDeltaSec = timeDeltaSec;

	// Back buffer is in COMMON state and need to be transitioned to RT
	beginCommandList->PushBarrier(rhi::Barrier::Texture(
		frameBuffer->GetDesc().ColorAttachments[0].texture.get(), rhi::ResourceState::PRESENT, rhi::ResourceState::RENDERTARGET));

	// Done with beginCommandList
	beginCommandList->EndMarker();
	beginCommandList->Close();
	m_DeviceManager->GetDevice()->ExecuteCommandList(beginCommandList, st::rhi::QueueType::Graphics);

	// Get initial textures state
	std::map<std::string, rhi::ResourceState> texturesState;
	for (auto& entry : m_DeclaredTextures)
		texturesState.emplace(entry.first, GetInitialState(entry.second->type));
	// Get initial buffers state
	std::map<std::string, rhi::ResourceState> buffersState;
	for (auto& entry : m_DeclaredBuffers)
		buffersState.emplace(entry.first, rhi::ResourceState::SHADER_RESOURCE);

	// Stages render
	auto stageCommandList = GetCommandList().get();
	stageCommandList->Open();

	stageCommandList->BeginMarker("Render stages");
	for (auto* rs : renderStages)
	{
		// Update view of reads
		UpdateRequestedTextureViews(stageCommandList, rs->renderStage.get(), AccessMode::Read, texturesState);
		UpdateRequestedBufferViews(stageCommandList, rs->renderStage.get(), AccessMode::Read, buffersState);

		if (rs->renderStage->IsEnabled())
		{
			stageCommandList->BeginMarker(rs->renderStage->GetDebugName());
				
			// GPU time query
			rs->timerQueries[m_DeviceManager->GetFrameModuleIndex()]->Reset();
			stageCommandList->BeginTimerQuery(rs->timerQueries[m_DeviceManager->GetFrameModuleIndex()].get());

			// Entry barriers
			{
				std::vector<rhi::Barrier> barriers;
				auto getTextureBarriers = [&texturesState, &barriers, this](const std::vector<RenderStageResourceDep>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = texturesState.find(dep.id);
						if (state_it != texturesState.end() && state_it->second != dep.inputState)
						{
							barriers.push_back(rhi::Barrier::Texture(
								GetTexture(dep.id).get(), state_it->second, dep.inputState));
							state_it->second = dep.inputState;
						}
					}
				};
				auto getBufferBarriers = [&buffersState, &barriers, this](const std::vector< RenderStageResourceDep>& deps)
				{
					for(const auto& dep : deps)
					{
						auto state_it = buffersState.find(dep.id);
						if (state_it != buffersState.end() && state_it->second != dep.inputState)
						{
							barriers.push_back(rhi::Barrier::Buffer(
								GetBuffer(dep.id).get(), state_it->second, dep.inputState));
							state_it->second = dep.inputState;
						}
					}
				};
				getTextureBarriers(rs->textureReads);
				getTextureBarriers(rs->textureWrites);
				getBufferBarriers(rs->bufferReads);
				getBufferBarriers(rs->bufferWrites);

				if (!barriers.empty())
				{
					std::string markerName = rs->renderStage->GetDebugName();
					markerName.append(" - Entry barriers");
					stageCommandList->BeginMarker(markerName.c_str());
					stageCommandList->PushBarriers(barriers);
					stageCommandList->EndMarker();
				}
			} // end entry barriers

			// Render
			{
				std::chrono::steady_clock::time_point tbegin = std::chrono::steady_clock::now();

				rs->renderStage->Render();

				std::chrono::steady_clock::time_point tend = std::chrono::steady_clock::now();
				float ms = std::chrono::duration<float, std::milli>(tend - tbegin).count();
				rs->cpuElapsed[m_DeviceManager->GetFrameIndex() % rs->cpuElapsed.size()] = ms;
			}

			stageCommandList->EndTimerQuery(rs->timerQueries[m_DeviceManager->GetFrameModuleIndex()].get());
			stageCommandList->EndMarker();

			// Update the resource states
			{
				auto updateTextureStates = [&texturesState, this](const std::vector<RenderStageResourceDep>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = texturesState.find(dep.id);
						if (state_it != texturesState.end() && state_it->second != dep.outputState)
						{
							state_it->second = dep.outputState;
						}
					}
				};
				auto updateBufferStates = [&buffersState, this](const std::vector<RenderStageResourceDep>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = buffersState.find(dep.id);
						if (state_it != buffersState.end() && state_it->second != dep.outputState)
						{
							state_it->second = dep.outputState;
						}
					}
				};
				updateTextureStates(rs->textureReads);
				updateTextureStates(rs->textureWrites);
				updateBufferStates(rs->bufferReads);
				updateBufferStates(rs->bufferWrites);
			}
		} // if (rs->renderStage->IsEnabled())

		// Update view of writes
		UpdateRequestedTextureViews(stageCommandList, rs->renderStage.get(), AccessMode::Write, texturesState);
		UpdateRequestedBufferViews(stageCommandList, rs->renderStage.get(), AccessMode::Write, buffersState);
	} // end render stages iteration

	stageCommandList->EndMarker();
	stageCommandList->Close();
	m_DeviceManager->GetDevice()->ExecuteCommandList(stageCommandList, st::rhi::QueueType::Graphics);

	rhi::ICommandList* endCommandList = m_EndCommandLists[m_DeviceManager->GetFrameModuleIndex()].get();
	endCommandList->Open();
	endCommandList->BeginMarker("End commands");

	// Frame End. All the resources need to go back to its initial state.
	{
		std::vector<rhi::Barrier> barriers;
		for (auto& tex : m_DeclaredTextures)
		{
			rhi::ResourceState initialState = GetInitialState(tex.second->type);
			rhi::ResourceState currentState = texturesState.find(tex.first)->second;
			if (initialState != currentState)
			{
				barriers.push_back(rhi::Barrier::Texture(tex.second->texture.get(), currentState, initialState));
			}
		}
		for (auto& buffer : m_DeclaredBuffers)
		{
			rhi::ResourceState initialState = rhi::ResourceState::SHADER_RESOURCE;
			rhi::ResourceState currentState = buffersState.find(buffer.first)->second;
			if (initialState != currentState)
			{
				barriers.push_back(rhi::Barrier::Buffer(buffer.second->buffer.get(), currentState, initialState));
			}
		}
		if (!barriers.empty())
		{
			endCommandList->PushBarriers(barriers);
		}
	}

	// Back buffer to common so it can be presented
	endCommandList->PushBarrier(rhi::Barrier().Texture(
		frameBuffer->GetDesc().ColorAttachments[0].texture.get(), rhi::ResourceState::RENDERTARGET, rhi::ResourceState::PRESENT));

	endCommandList->EndMarker();
	endCommandList->EndMarker();
	endCommandList->Close();

	// Execute!
	m_DeviceManager->GetDevice()->ExecuteCommandList(endCommandList, st::rhi::QueueType::Graphics);
}

size_t st::gfx::RenderView::GetNumRenderStages(const std::string& mode) const
{ 
	auto it = m_RenderModes.find(mode.empty() ? m_CurrentRenderMode : mode);
	if(it == m_RenderModes.end())
	{
		LOG_WARNING("Render mode requested '{}' does not exists", m_CurrentRenderMode);
		return 0;
	}
	return it->second.size();
}

const st::gfx::RenderView::RenderStageData* st::gfx::RenderView::GetRenderStage(uint32_t idx, const std::string& mode) const
{
	auto it = m_RenderModes.find(mode.empty() ? m_CurrentRenderMode : mode);
	if (it == m_RenderModes.end())
	{
		LOG_WARNING("Render mode requested '{}' does not exists", m_CurrentRenderMode);
		return 0;
	}

	return it->second.at(idx);
}

st::gfx::RenderView::TextureViewTicket st::gfx::RenderView::RequestTextureView(RenderStage* rs, AccessMode accessMode, const std::string& id)
{
	for (auto& entry : m_TexViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode && entry->id == id)
		{
			++entry->refCount;
			return entry;
		}
	}

	auto* ticket = new TextureViewRequest{ rs, accessMode, id, 1 };
	m_TexViewRequests.push_back(ticket);
	return ticket;
}

st::gfx::RenderView::BufferViewTicket st::gfx::RenderView::RequestBufferView(RenderStage* rs, AccessMode accessMode, const std::string& id)
{
	for (auto& entry : m_BufferViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode && entry->id == id)
		{
			++entry->refCount;
			return entry;
		}
	}

	auto* ticket = new BufferViewRequest{ rs, accessMode, id, 1 };
	m_BufferViewRequests.push_back(ticket);
	return ticket;
}

void st::gfx::RenderView::ReleaseTextureView(TextureViewTicket ticket)
{
	auto it = std::find(m_TexViewRequests.begin(), m_TexViewRequests.end(), ticket);
	if (it != m_TexViewRequests.end())
	{
		if (--((*it)->refCount) == 0)
		{
			delete *it;
			m_TexViewRequests.erase(it);
		}
	}
}

void st::gfx::RenderView::ReleaseBufferView(BufferViewTicket ticket)
{
	auto it = std::find(m_BufferViewRequests.begin(), m_BufferViewRequests.end(), ticket);
	if (it != m_BufferViewRequests.end())
	{
		if (--((*it)->refCount) == 0)
		{
			delete* it;
			m_BufferViewRequests.erase(it);
		}
	}
}

st::rhi::TextureHandle st::gfx::RenderView::GetTextureView(TextureViewTicket ticket)
{
	auto it = std::find(m_TexViewRequests.begin(), m_TexViewRequests.end(), ticket);
	if (it == m_TexViewRequests.end())
		return nullptr;

	return (*it)->tex.get_weak();
}

st::rhi::BufferHandle st::gfx::RenderView::GetBufferView(BufferViewTicket ticket)
{
	auto it = std::find(m_BufferViewRequests.begin(), m_BufferViewRequests.end(), ticket);
	if (it == m_BufferViewRequests.end())
		return nullptr;

	return (*it)->buffer.get_weak();
}

void st::gfx::RenderView::Refresh()
{
	m_IsDirty = false;
}

void st::gfx::RenderView::ClearRenderStages()
{
	m_RenderModes.clear();

	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->Detach();
	}
	m_RenderStages.clear();

	for (auto& dt : m_DeclaredTextures)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(std::move(dt.second->texture));
	}
	m_DeclaredTextures.clear();
}

void st::gfx::RenderView::UpdateSceneConstantBuffer()
{
	interop::Scene* sceneShaderConstant = (interop::Scene*)m_SceneCB.GetNextPtrRaw();
	*sceneShaderConstant = {};

	// Camera
	if (m_Camera)
	{
		sceneShaderConstant->camViewProjMatrix = m_Camera->GetViewProjectionMatrix();
		sceneShaderConstant->camViewMatrix = m_Camera->GeViewMatrix();
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
		// Sun directional light
		const Scene::SunParams& sunParams = m_Scene->GetSunParams();
		const float3 sunDir = st::ElevationAzimuthRadToDir(
			glm::radians(sunParams.ElevationDeg), glm::radians(sunParams.AzimuthDeg));

		sceneShaderConstant->sunDirection = sunDir;
		sceneShaderConstant->sunIrradiance = sunParams.Irradiance;
		sceneShaderConstant->sunColor = float4{ sunParams.Color, 0.f };
		sceneShaderConstant->sunAngularSizeRad = glm::radians(sunParams.AngularSizeDeg);
		sceneShaderConstant->sunWorldToClipMatrix = m_SunWoldToClipMatrix;
		sceneShaderConstant->sunViewToClipMatrix = m_ViewToSunClipMatrix;

		// Ambient 
		const Scene::AmbientParams& ambientParams = m_Scene->GetAmbientParams();
		sceneShaderConstant->ambientTop = float4{ ambientParams.SkyColor * ambientParams.Intensity, 0.f };
		sceneShaderConstant->ambientBottom = float4{ ambientParams.GroundColor * ambientParams.Intensity, 0.f };

		// Lights
		sceneShaderConstant->dirLightCount = 0;
		sceneShaderConstant->dirLightIndicesDI = {};
		sceneShaderConstant->pointLightCount = m_PointLightsVisibleCount;
		sceneShaderConstant->pointLightIndicesDI = m_PointLightsVisibleBuffer.GetCurrentBuffer() ? m_PointLightsVisibleBuffer.GetCurrentBuffer()->GetReadOnlyView() : INVALID_DESCRIPTOR_INDEX;
		sceneShaderConstant->spotLightCount = 0;
		sceneShaderConstant->spotLightIndicesDI = {};

		// Data buffers
		sceneShaderConstant->instanceBufferDI = m_Scene->GetInstancesBufferView();
		sceneShaderConstant->meshesBufferDI = m_Scene->GetMeshesBufferView();
		sceneShaderConstant->materialsBufferDI = m_Scene->GetMaterialsBufferView();
		sceneShaderConstant->dirLightsBufferDI = {};
		sceneShaderConstant->pointLightsBufferDI = m_Scene->GetPointLightsBufferView();
		sceneShaderConstant->spotLightsBufferDI = {};
	}
}

void st::gfx::RenderView::UpdateCameraVisibleSet(rhi::ICommandList* commandList)
{
	m_CameraVisibleSet.clear();
	m_CameraVisibleBounds.reset();
	if (!m_Camera || !m_Scene || !m_Scene->GetSceneGraph())
		return;

	m_CameraVisibleSet = GetVisibleSet(m_Camera->GetFrustum().get_planes(), &m_CameraVisibleBounds);
	UpdateVisibilityShaderBuffer(m_CameraVisibleSet, m_CameraVisibleBuffer, commandList);
}

void st::gfx::RenderView::UpdateShadowmapData(rhi::ICommandList* commandList)
{
	m_SunVisibleSet.clear();
	m_SunWoldToClipMatrix = {};
	m_ViewToSunClipMatrix = {};

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

	m_SunWoldToClipMatrix = sunProjMatrix * sunViewMatrix;

	// view -> world -> sun_view -> sun_clip
	m_ViewToSunClipMatrix = sunProjMatrix * sunViewMatrix * glm::inverse(m_Camera->GeViewMatrix());

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

		m_SunVisibleSet = GetVisibleSet(sunClipPlanes);
		UpdateVisibilityShaderBuffer(m_SunVisibleSet, m_SunVisibleBuffer, commandList);
	}
}

void st::gfx::RenderView::UpdateLightsVisibleSet(rhi::ICommandList* commandList)
{
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

	uint32_t reqSize = m_PointLightsVisibleCount * sizeof(uint32_t);
	m_PointLightsVisibleBuffer.Grow(reqSize * 2);

	rhi::BufferHandle buffer = m_PointLightsVisibleBuffer.GetCurrentBuffer();
	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

	// Copy to upload data
	UploadBuffer* uploadBuffer = m_DeviceManager->GetUploadBuffer();
	auto [data, offset] = uploadBuffer->RequestSpaceForBufferDataUpload(reqSize);

	uint32_t* ptr = (uint32_t*)data;
	for (const auto* pointLight : visiblePointLights)
	{
		*ptr = pointLight->GetLeafSceneIndex();
		ptr++;
	}

	// Copy to buffer
	commandList->CopyBufferToBuffer(buffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, reqSize);

	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

}

std::vector<st::gfx::RenderView::TextureViewRequest*> st::gfx::RenderView::GetTexViewRequests(RenderStage* rs, AccessMode accessMode)
{
	std::vector<st::gfx::RenderView::TextureViewRequest*> ret;
	for (auto& entry : m_TexViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode)
		{
			ret.push_back(entry);
		}
	}
	return ret;
}

void st::gfx::RenderView::UpdateRequestedTextureViews(st::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
	const std::map<std::string, rhi::ResourceState> resourceStates)
{
	auto requests = GetTexViewRequests(rs, accessMode);
	for (auto req : requests)
	{
		auto it = m_DeclaredTextures.find(req->id);
		if (it == m_DeclaredTextures.end())
			continue;

		rhi::TextureHandle sourceTex = it->second->texture.get_weak();
		if (!sourceTex)
			continue;

		const rhi::TextureDesc& sourceTexDesc = sourceTex->GetDesc();

		// Create the target texture if does not exists
		// TODO: If the source texture changed (resized for example) we need to re-create de texture
		if (!req->tex)
		{
			rhi::TextureDesc desc{
				.width = sourceTexDesc.width,
				.height = sourceTexDesc.height,
				.depth = sourceTexDesc.depth,
				.arraySize = sourceTexDesc.arraySize,
				.mipLevels = sourceTexDesc.mipLevels,
				.format = sourceTexDesc.format,//rhi::Format::RGBA8_UNORM,
				.shaderUsage = rhi::TextureShaderUsage::Sampled
			};

			std::stringstream debugName;
			debugName << rs->GetDebugName() << " - ";
			if (accessMode == st::gfx::RenderView::AccessMode::Read)
				debugName << "Read";
			else
				debugName << "Write";
			debugName << " - " << sourceTex->GetDebugName();

			req->tex = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::SHADER_RESOURCE, debugName.str().c_str());
			assert(req->tex);
		}

		rhi::ResourceState srcTexState = resourceStates.find(req->id)->second;

		rhi::Barrier entryBarriers[] = {
			rhi::Barrier::Texture(sourceTex.get(), srcTexState, rhi::ResourceState::COPY_SRC),
			rhi::Barrier::Texture(req->tex.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST) };
		commandList->PushBarriers(entryBarriers);

		commandList->CopyTextureToTexture(req->tex.get(), rhi::AllSubresources, sourceTex.get(), rhi::AllSubresources);
		
		rhi::Barrier exitBarriers[] = {
			rhi::Barrier::Texture(sourceTex.get(), rhi::ResourceState::COPY_SRC, srcTexState),
			rhi::Barrier::Texture(req->tex.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE) };
		commandList->PushBarriers(exitBarriers);
	}
}

std::vector<st::gfx::RenderView::BufferViewRequest*> st::gfx::RenderView::GetBufferViewRequests(RenderStage* rs, AccessMode accessMode)
{
	std::vector<st::gfx::RenderView::BufferViewRequest*> ret;
	for (auto& entry : m_BufferViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode)
		{
			ret.push_back(entry);
		}
	}
	return ret;
}

void st::gfx::RenderView::UpdateRequestedBufferViews(st::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
	const std::map<std::string, rhi::ResourceState> resourceStates)
{
	auto requests = GetBufferViewRequests(rs, accessMode);
	for (auto req : requests)
	{
		auto it = m_DeclaredBuffers.find(req->id);
		if (it == m_DeclaredBuffers.end())
			continue;

		rhi::BufferHandle srcBuffer = it->second->buffer.get_weak();
		if (!srcBuffer)
			continue;

		// Create the target buffer if does not exists
		// TODO: If the source buffer changed we need to re-create it
		if (!req->buffer)
		{
			const rhi::BufferDesc& srcBufferDesc = srcBuffer->GetDesc();

			rhi::BufferDesc desc{
				.memoryAccess = rhi::MemoryAccess::Readback,
				.shaderUsage = rhi::BufferShaderUsage::None,
				.sizeBytes = srcBufferDesc.sizeBytes,
				.format = rhi::Format::UNKNOWN,
				.stride = 0 };

			std::stringstream debugName;
			debugName << rs->GetDebugName() << " - ";
			if (accessMode == st::gfx::RenderView::AccessMode::Read)
				debugName << "Read";
			else
				debugName << "Write";
			debugName << " - " << srcBuffer->GetDebugName();

			req->buffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COMMON, debugName.str().c_str());
			assert(req->buffer);
		}

		rhi::ResourceState srcBufferState = resourceStates.find(req->id)->second;

		rhi::Barrier entryBarriers[] = {
			rhi::Barrier::Buffer(srcBuffer.get(), srcBufferState, rhi::ResourceState::COPY_SRC),
			rhi::Barrier::Buffer(req->buffer.get(), rhi::ResourceState::COMMON, rhi::ResourceState::COPY_DST) };
		commandList->PushBarriers(entryBarriers);

		commandList->CopyBufferToBuffer(req->buffer.get(), 0, srcBuffer.get(), 0, req->buffer->GetDesc().sizeBytes);

		rhi::Barrier exitBarriers[] = {
			rhi::Barrier::Buffer(srcBuffer.get(), rhi::ResourceState::COPY_SRC, srcBufferState),
			rhi::Barrier::Buffer(req->buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::COMMON) };
		commandList->PushBarriers(exitBarriers);
	}
}

st::gfx::RenderView::RenderSet st::gfx::RenderView::GetVisibleSet(const std::span<const math::plane3f>& planes, math::aabox3f* opt_outBounds) const
{
	if (!m_Scene || !m_Scene->GetSceneGraph())
		return {};
	
	std::vector<const st::gfx::MeshInstance*> cullBase[(int)rhi::CullMode::_Size];

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

				cullBase[(int)meshInstance->GetMesh()->GetMaterial()->GetCullMode()].push_back(meshInstance);

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
	for(int i = 0; i < (int)rhi::CullMode::_Size; ++i)
	{
		std::sort(cullBase[i].begin(), cullBase[i].end(), [](const st::gfx::MeshInstance* a, const st::gfx::MeshInstance* b)
		{
			return a->GetMesh().get() < b->GetMesh().get();
		});
	}

	// Move to result
	std::vector<std::pair<st::rhi::CullMode, std::vector<const st::gfx::MeshInstance*>>> result;
	for (int i = 0; i < (int)rhi::CullMode::_Size; ++i)
	{
		result.emplace_back((rhi::CullMode)i, std::move(cullBase[i]));
	}
	return result;
}

void st::gfx::RenderView::UpdateVisibilityShaderBuffer(const RenderSet& renderSet, gfx::MultiBuffer& multiBuffer,
	rhi::ICommandList* commandList)
{
	UploadBuffer* uploadBuffer = m_DeviceManager->GetUploadBuffer();

	size_t reqSize = 0;
	for(const auto& instances : renderSet)
		reqSize += instances.second.size() * sizeof(uint32_t);

	multiBuffer.Grow(reqSize * 2);

	rhi::BufferHandle buffer = multiBuffer.GetCurrentBuffer();
	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::COPY_DST));

	// Copy to upload data
	auto [data, offset] = uploadBuffer->RequestSpaceForBufferDataUpload(reqSize);

	uint32_t* ptr = (uint32_t*)data;
	for (const auto& instances : renderSet)
	{
		for (const st::gfx::MeshInstance* inst : instances.second)
		{
			*ptr = inst->GetLeafSceneIndex();
			ptr++;
		}
	}

	// Copy to buffer
	commandList->CopyBufferToBuffer(buffer.get(), 0, uploadBuffer->GetBuffer().get(), offset, reqSize);

	commandList->PushBarrier(
		rhi::Barrier::Buffer(buffer.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));
}