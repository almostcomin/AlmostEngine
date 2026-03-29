#include "Gfx/RenderGraph.h"
#include "RHI/Texture.h"
#include "RHI/Device.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/RenderStage.h"
#include "Gfx/RenderGraphBuilder.h"
#include "Gfx/RenderView.h"
#include "RHI/TimerQuery.h"

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

	alm::rhi::ResourceState GetInitialState(alm::gfx::RenderGraph::TextureResourceType type)
	{
		switch (type)
		{
		case alm::gfx::RenderGraph::TextureResourceType::RenderTarget:
			return alm::rhi::ResourceState::RENDERTARGET;
		case alm::gfx::RenderGraph::TextureResourceType::DepthStencil:
			return alm::rhi::ResourceState::DEPTHSTENCIL;
		case alm::gfx::RenderGraph::TextureResourceType::ShaderResource:
			return alm::rhi::ResourceState::SHADER_RESOURCE;
		default:
			assert(0);
			return alm::rhi::ResourceState::SHADER_RESOURCE;
		}
	}

	alm::rhi::TextureShaderUsage GetTextureShaderUsage(alm::gfx::RenderGraph::TextureResourceType type, bool needUAV)
	{
		alm::rhi::TextureShaderUsage usage;
		switch (type)
		{
		case alm::gfx::RenderGraph::TextureResourceType::RenderTarget:
			usage = alm::rhi::TextureShaderUsage::Sampled | alm::rhi::TextureShaderUsage::ColorTarget;
			break;
		case alm::gfx::RenderGraph::TextureResourceType::DepthStencil:
			usage = alm::rhi::TextureShaderUsage::Sampled | alm::rhi::TextureShaderUsage::DepthTarget;
			break;
		case alm::gfx::RenderGraph::TextureResourceType::ShaderResource:
			usage = alm::rhi::TextureShaderUsage::Sampled;
			break;
		default:
			assert(0);
			usage = alm::rhi::TextureShaderUsage::Sampled;
		}
		if (needUAV)
			usage |= alm::rhi::TextureShaderUsage::Storage;

		return usage;
	}
} // anonymous namespace

alm::gfx::RenderGraph::RenderGraph(RenderView* renderView, const char* debugName) :
	m_DebugName{ debugName },
	m_RenderView{ renderView },
	m_DeviceManager{ renderView->GetDeviceManager() }
{
	rhi::Device* device = m_DeviceManager->GetDevice();

	rhi::CommandListParams params{
		.queueType = rhi::QueueType::Graphics
	};
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount(); ++i)
	{
		m_CommandLists.push_back(device->CreateCommandList(params, m_DebugName));
	}
}

alm::gfx::RenderGraph::~RenderGraph()
{
	Reset();
}

void alm::gfx::RenderGraph::Reset()
{
	for (auto& req : m_TexViewRequests)
	{
		delete req;
	}
	m_TexViewRequests.clear();

	for (auto& req : m_BufferViewRequests)
	{
		delete req;
	}
	m_BufferViewRequests.clear();

	m_CurrentRenderMode = {};
	m_RenderModes.clear();

	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->Detach();
	}
	m_RenderStages.clear();

	for (auto& dt : m_Textures)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(std::move(dt.second->texture));
	}
	m_Textures.clear();

	for (auto& dt : m_Buffers)
	{
		m_DeviceManager->GetDevice()->ReleaseQueued(std::move(dt.second->buffer));
	}
	m_Buffers.clear();
}

void alm::gfx::RenderGraph::SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages)
{
	Reset();

	m_RenderStages.reserve(renderStages.size());
	for (auto& rs : renderStages)
	{
		auto rsd = new StageData;
		rsd->renderStage = rs;
		rsd->timerQueries.reserve(m_DeviceManager->GetSwapchainBufferCount() * 2);
		rsd->cpuElapsed.resize(8, 0.f); // 8 samples
		m_RenderStages.emplace_back(rsd);
	}

	// Init timer queries
	for (int i = 0; i < m_DeviceManager->GetSwapchainBufferCount() * 2; ++i)
	{
		for (auto& rsd : m_RenderStages)
		{
			rsd->timerQueries.push_back(m_DeviceManager->GetDevice()->CreateTimerQuery(
				std::format("{} - TimerQuery[{}]", rsd->renderStage->GetDebugName(), i)));
		}
	}

	for (auto& rs : m_RenderStages)
	{
		RenderGraphBuilder builder{ this, rs->renderStage.get() };
		rs->renderStage->Setup(builder);
		
		rs->textureReads = builder.GetTextureReadDependencies();
		rs->textureWrites = builder.GetTextureWriteDependencies();
		rs->bufferReads = builder.GetBufferReadDependencies();
		rs->bufferWrites = builder.GetBufferWriteDependencies();
	}

	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->Attach(this);
	}
}

void alm::gfx::RenderGraph::SetRenderMode(const std::string& name, const std::vector<RenderStage*>& renderStages)
{
	if (m_RenderModes.find(name) != m_RenderModes.end())
	{
		m_RenderModes.erase(name);
	}

	std::vector<StageData*> stages;
	stages.reserve(renderStages.size());

	for (auto* rs : renderStages)
	{
		// Check that the stage actually belongs to this
		auto it = std::find_if(m_RenderStages.begin(), m_RenderStages.end(), [rs](const std::unique_ptr<StageData>& rsd) -> bool
			{
				return rs == rsd->renderStage.get();
			});
		if (it == m_RenderStages.end())
		{
			LOG_ERROR("Render stage '{}' is not part of the render graph '{}'", rs->GetDebugName(), m_DebugName);
			return;
		}
		stages.push_back(it->get());
	}

	m_RenderModes[name] = std::move(stages);

	if (m_CurrentRenderMode.empty())
		m_CurrentRenderMode = name;
}

void alm::gfx::RenderGraph::SetCurrentRenderMode(const std::string& name)
{
	if (m_RenderModes.find(name) == m_RenderModes.end())
	{
		LOG_WARNING("Render mode '{}' not found", name);
		return;
	}
	m_CurrentRenderMode = name;
}

std::vector<std::string> alm::gfx::RenderGraph::GetRenderModes() const
{
	std::vector<std::string> result;
	result.reserve(m_RenderModes.size());
	for (const auto& rm : m_RenderModes)
	{
		result.push_back(rm.first);
	}
	return result;
}

void alm::gfx::RenderGraph::Compile()
{
}

bool alm::gfx::RenderGraph::BeginRender(rhi::ICommandList* /*commandList*/)
{
	return false; // no-operation needed
}

bool alm::gfx::RenderGraph::EndRender(rhi::ICommandList* commandList)
{
	// Frame End. All the resources need to go back to its initial state.
	std::vector<rhi::Barrier> barriers;
	for (auto& texState : m_TexturesState)
	{
		DeclaredTexture* declTex = GetDeclTex(texState.first);
		rhi::ResourceState initialState = GetInitialState(declTex->type);
		rhi::ResourceState currentState = texState.second;
		if (initialState != currentState)
		{
			barriers.push_back(rhi::Barrier::Texture(declTex->texture.get(), currentState, initialState));
		}
	}
	
	for (auto& bufferState : m_BuffersState)
	{
		rhi::ResourceState initialState = rhi::ResourceState::SHADER_RESOURCE;
		rhi::ResourceState currentState = bufferState.second;
		if (initialState != currentState)
		{
			DeclaredBuffer* declBuffer = GetDeclBuffer(bufferState.first);
			barriers.push_back(rhi::Barrier::Buffer(declBuffer->buffer.get(), currentState, initialState));
		}
	}

	if (!barriers.empty())
	{
		commandList->PushBarriers(barriers);
		return true;
	}
	return false;
}

void alm::gfx::RenderGraph::Render(alm::rhi::FramebufferHandle /*frameBuffer*/)
{
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

	const std::vector<StageData*>& renderStages = it->second;

	// Get initial textures state
	m_TexturesState.clear();
	for (auto& entry : m_Textures)
	{
		if(entry.second->texture)
			m_TexturesState.emplace(GetHandle(entry.second.get()), GetInitialState(entry.second->type));
	}
	// Get initial buffers state
	m_BuffersState.clear();
	for (auto& entry : m_Buffers)
	{
		if (entry.second->buffer)
			m_BuffersState.emplace(GetHandle(entry.second.get()), rhi::ResourceState::SHADER_RESOURCE);
	}

	// Stages render
	auto stageCommandList = GetCommandList().get();
	stageCommandList->Open();

	stageCommandList->BeginMarker("Render stages");
	for (auto* rs : renderStages)
	{
		// Update view of reads
		UpdateRequestedTextureViews(stageCommandList, rs->renderStage.get(), AccessMode::Read, m_TexturesState);
		UpdateRequestedBufferViews(stageCommandList, rs->renderStage.get(), AccessMode::Read, m_BuffersState);

		if (rs->renderStage->IsEnabled())
		{
			stageCommandList->BeginMarker(rs->renderStage->GetDebugName());

			// GPU time query
			const int timeQueryIndex = m_DeviceManager->GetFrameIndex() % rs->timerQueries.size();
			rs->timerQueries[timeQueryIndex]->Reset();
			stageCommandList->BeginTimerQuery(rs->timerQueries[timeQueryIndex].get());

			// Entry barriers
			{
				std::vector<rhi::Barrier> barriers;
				auto getTextureBarriers = [&barriers, this](const std::vector<TextureDependency>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = m_TexturesState.find(dep.handle);
						if (state_it != m_TexturesState.end() && state_it->second != dep.inputState)
						{
							barriers.push_back(rhi::Barrier::Texture(
								GetTexture(dep.handle).get(), state_it->second, dep.inputState));
							state_it->second = dep.inputState;
						}
					}
				};
				auto getBufferBarriers = [&barriers, this](const std::vector<BufferDependency>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = m_BuffersState.find(dep.handle);
						if (state_it != m_BuffersState.end() && state_it->second != dep.inputState)
						{
							barriers.push_back(rhi::Barrier::Buffer(
								GetBuffer(dep.handle).get(), state_it->second, dep.inputState));
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

				rs->renderStage->Render(GetCommandList());

				std::chrono::steady_clock::time_point tend = std::chrono::steady_clock::now();
				float ms = std::chrono::duration<float, std::milli>(tend - tbegin).count();
				rs->cpuElapsed[m_DeviceManager->GetFrameIndex() % rs->cpuElapsed.size()] = ms;
			}

			stageCommandList->EndTimerQuery(rs->timerQueries[timeQueryIndex].get());
			stageCommandList->EndMarker();

			// Update the resource states
			{
				auto updateTextureStates = [this](const std::vector<TextureDependency>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = m_TexturesState.find(dep.handle);
						if (state_it != m_TexturesState.end() && state_it->second != dep.outputState)
						{
							state_it->second = dep.outputState;
						}
					}
				};
				auto updateBufferStates = [this](const std::vector<BufferDependency>& deps)
				{
					for (const auto& dep : deps)
					{
						auto state_it = m_BuffersState.find(dep.handle);
						if (state_it != m_BuffersState.end() && state_it->second != dep.outputState)
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
		UpdateRequestedTextureViews(stageCommandList, rs->renderStage.get(), AccessMode::Write, m_TexturesState);
		UpdateRequestedBufferViews(stageCommandList, rs->renderStage.get(), AccessMode::Write, m_BuffersState);
	} // end render stages iteration

	stageCommandList->EndMarker();
	stageCommandList->Close();
	m_DeviceManager->GetDevice()->ExecuteCommandList(stageCommandList, alm::rhi::QueueType::Graphics);

}

void alm::gfx::RenderGraph::OnSceneChanged()
{
	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->OnSceneChanged();
	}
}

void alm::gfx::RenderGraph::OnRenderTargetChanged(const int2& newSize)
{
	alm::unique_vector<RGFramebufferHandle> framebuffersToUpdate;

	// Update all the textures whose size is dependant on BB size
	for (auto& it : m_Textures)
	{
		auto& declTex = it.second;
		if (declTex->texture && (declTex->requestedWidth <= 0 || declTex->requestedHeight <= 0))
		{
			rhi::TextureDesc newDesc = declTex->texture->GetDesc();
			newDesc.width = GetActualSize(declTex->requestedWidth, newSize.x);
			newDesc.height = GetActualSize(declTex->requestedHeight, newSize.y);

			m_DeviceManager->GetDevice()->ReleaseImmediately(std::move(declTex->texture));
			declTex->texture = m_DeviceManager->GetDevice()->CreateTexture(
				newDesc, GetInitialState(declTex->type), declTex->id);

			for (auto& fbHandle : declTex->framebuffers)
			{
				framebuffersToUpdate.insert(fbHandle);
			}
		}
	}

	// Update framebuffers
	for (auto fbHandle : framebuffersToUpdate)
	{
		RequestedFramebufferData* fbData = GetReqFb(fbHandle);

		rhi::FramebufferDesc desc;
		for (RGTextureHandle handle : fbData->colorTargets)
		{
			desc.AddColorAttachment(GetTexture(handle));
		}
		if (fbData->depthTarget)
		{
			desc.SetDepthAttachment(GetTexture(fbData->depthTarget));
		}
		auto fb = m_DeviceManager->GetDevice()->CreateFramebuffer(desc, fbData->fb->GetDebugName());
		fbData->fb = std::move(fb);
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

alm::gfx::RGTextureHandle alm::gfx::RenderGraph::CreateTexture(RenderStage* renderStage, const std::string& id, TextureResourceType type,
																	   int width, int height, int arraySize, alm::rhi::Format format, bool needsUAV)
{	
	// Check if that texture is already created
	auto it = m_Textures.find(id);
	if (it != m_Textures.end())
	{
		// This is allowed as far as the texture properties match
		const auto& desc = it->second->texture->GetDesc();
		if (it->second->requestedWidth == width && it->second->requestedHeight == height && it->second->type == type && desc.arraySize == arraySize &&
			desc.format == format && needsUAV == has_any_flag(desc.shaderUsage, rhi::TextureShaderUsage::Storage))
		{
			return GetHandle(it->second.get());
		}
		else
		{
			LOG_ERROR("Texture with id {} already created and properties does not match", id);
			return { nullptr };
		}
	}

	rhi::TextureDesc desc{
		.width = (uint32_t)GetActualSize(width, m_RenderView->GetFramebuffer()->GetFramebufferInfo().width),
		.height = (uint32_t)GetActualSize(height, m_RenderView->GetFramebuffer()->GetFramebufferInfo().height),
		.arraySize = (uint32_t)arraySize,
		.format = format,
		.shaderUsage = GetTextureShaderUsage(type, needsUAV) };

	rhi::TextureOwner texture = m_DeviceManager->GetDevice()->CreateTexture(desc, GetInitialState(type), id);
	DeclaredTexture* declTex = new DeclaredTexture{
		.id = id,
		.texture = std::move(texture),
		.type = type,
		.requestedWidth = width,
		.requestedHeight = height,
		.arraySize = arraySize,
		.format = format,
		.needsUAV = needsUAV,
		.owner = renderStage
	};

	auto result = m_Textures.insert({ id, std::unique_ptr<DeclaredTexture>{declTex} });
	return { result.first->second.get() };
}

alm::gfx::RGBufferHandle alm::gfx::RenderGraph::CreateBuffer(RenderStage* renderStage, const std::string& id, const alm::rhi::BufferDesc& desc)
{
	// Check that texture has not been already created
	auto it = m_Buffers.find(id);
	if (it != m_Buffers.end())
	{
		LOG_ERROR("Buffer with id {} already created", id);
		return { nullptr };
	}

	rhi::BufferOwner buffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE, id);
	auto result = m_Buffers.insert({ id, std::make_unique<DeclaredBuffer>(id, std::move(buffer), renderStage) });

	return { result.first->second.get() };
}

bool alm::gfx::RenderGraph::RecreateTexture(RGTextureHandle handle, int width, int height, int arraySize, rhi::Format format)
{
	// Check that texture has been already created
	auto* declTex = GetDeclTex(handle);

	declTex->requestedWidth = width;
	declTex->requestedHeight = height;
	declTex->arraySize = arraySize;
	declTex->format = format;

	if (declTex->texture)
	{
		rhi::TextureDesc desc{
			.width = (uint32_t)GetActualSize(width, m_RenderView->GetFramebuffer()->GetFramebufferInfo().width),
			.height = (uint32_t)GetActualSize(height, m_RenderView->GetFramebuffer()->GetFramebufferInfo().height),
			.arraySize = (uint32_t)arraySize,
			.format = format,
			.shaderUsage = GetTextureShaderUsage(declTex->type, declTex->needsUAV) };

		rhi::TextureOwner newTexture = m_DeviceManager->GetDevice()->CreateTexture(desc, GetInitialState(declTex->type), declTex->id);
		rhi::TextureOwner& oldTexture = declTex->texture;

		// Swap
		oldTexture->Swap(*newTexture.get());

		// newTexture (actually the old old since it has been swap-ed) would be released when the owner pointer gets out of scope
		// but lets do it explicitly
		m_DeviceManager->GetDevice()->ReleaseQueued(std::move(newTexture));
	}

	return true;
}

bool alm::gfx::RenderGraph::RecreateBuffer(RGBufferHandle handle, const rhi::BufferDesc& desc)
{
	// Check that texture has not been already created
	auto* declBuffer = GetDeclBuffer(handle);

	// Create new buffer
	rhi::BufferOwner newBuffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE, declBuffer->id);
	rhi::BufferOwner& oldBuffer = declBuffer->buffer;

	// Swap
	oldBuffer->Swap(*newBuffer.get());

	return true;
}

void alm::gfx::RenderGraph::EnableTexture(RGTextureHandle handle)
{
	auto* declTex = GetDeclTex(handle);
	if (!declTex->texture)
	{
		rhi::TextureDesc desc{
			.width = (uint32_t)GetActualSize(declTex->requestedWidth, m_RenderView->GetFramebuffer()->GetFramebufferInfo().width),
			.height = (uint32_t)GetActualSize(declTex->requestedHeight, m_RenderView->GetFramebuffer()->GetFramebufferInfo().height),
			.arraySize = (uint32_t)declTex->arraySize,
			.format = declTex->format,
			.shaderUsage = GetTextureShaderUsage(declTex->type, declTex->needsUAV) };

		declTex->texture = m_DeviceManager->GetDevice()->CreateTexture(desc, GetInitialState(declTex->type), declTex->id);
	}
}

void alm::gfx::RenderGraph::DisableTexture(RGTextureHandle handle)
{
	auto* declTex = GetDeclTex(handle);
	declTex->texture.reset();
}

alm::gfx::RGTextureHandle alm::gfx::RenderGraph::GetTextureHandle(const std::string& id)
{
	auto it = m_Textures.find(id);
	if (it != m_Textures.end())
	{
		return { it->second.get() };
	}
	return { nullptr };
}

alm::gfx::RGBufferHandle alm::gfx::RenderGraph::GetBufferHandle(const std::string& id)
{
	auto it = m_Buffers.find(id);
	if (it != m_Buffers.end())
	{
		return { it->second.get() };
	}
	return { nullptr };
}

alm::rhi::TextureHandle alm::gfx::RenderGraph::GetTexture(RGTextureHandle handle)
{
	return GetDeclTex(handle)->texture.get_weak();
}

alm::rhi::BufferHandle alm::gfx::RenderGraph::GetBuffer(RGBufferHandle handle)
{
	return GetDeclBuffer(handle)->buffer.get_weak();
}

alm::rhi::TextureHandle alm::gfx::RenderGraph::GetTexture(const std::string& id)
{
	return GetTexture(GetTextureHandle(id));
}

alm::rhi::BufferHandle alm::gfx::RenderGraph::GetBuffer(const std::string& id)
{
	return GetBuffer(GetBufferHandle(id));
}

const std::string& alm::gfx::RenderGraph::GetId(RGTextureHandle handle)
{
	return GetDeclTex(handle)->id;
}

const std::string& alm::gfx::RenderGraph::GetId(RGBufferHandle handle)
{
	return GetDeclBuffer(handle)->id;
}

alm::gfx::RGFramebufferHandle alm::gfx::RenderGraph::RequestFramebuffer(const std::vector<RGTextureHandle>& colorTargets, RGTextureHandle depthTarget)
{
	auto it = std::ranges::find_if(m_Framebuffers, [&](const auto& v)
	{
		return v->colorTargets == colorTargets && v->depthTarget == depthTarget;
	});

	RGFramebufferHandle result;
	if (it != m_Framebuffers.end())
	{
		(*it)->refCount++;
		result = { it->get() };
	}
	else
	{
		rhi::FramebufferDesc desc;
		for (RGTextureHandle handle : colorTargets)
		{
			desc.AddColorAttachment(GetTexture(handle));
		}
		if (depthTarget)
		{
			desc.SetDepthAttachment(GetTexture(depthTarget));
		}
		auto fb = m_DeviceManager->GetDevice()->CreateFramebuffer(desc, std::format("RGFramebuffer[{}]", m_Framebuffers.size()));
		auto entry = std::make_unique<RequestedFramebufferData>(colorTargets, depthTarget, std::move(fb), 1);

		// Dependencies
		for (RGTextureHandle handle : colorTargets)
		{
			GetDeclTex(handle)->framebuffers.push_back(entry.get());
		}
		if (depthTarget)
		{
			GetDeclTex(depthTarget)->framebuffers.push_back(entry.get());
		}

		result = { entry.get() };
		m_Framebuffers.insert(std::move(entry));
	}
	return result;
}

alm::rhi::FramebufferHandle alm::gfx::RenderGraph::GetFrameBuffer(RGFramebufferHandle handle)
{
	return reinterpret_cast<RequestedFramebufferData*>(handle.ptr)->fb.get_weak();
}

alm::rhi::TextureSampledView alm::gfx::RenderGraph::GetTextureSampledView(RGTextureHandle handle)
{
	auto tex = GetTexture(handle);
	return tex ? tex->GetSampledView() : rhi::c_InvalidDescriptorIndex;
}

alm::rhi::TextureStorageView alm::gfx::RenderGraph::GetTextureStorageView(RGTextureHandle handle)
{
	auto tex = GetTexture(handle);
	return tex ? tex->GetStorageView() : rhi::c_InvalidDescriptorIndex;
}

alm::rhi::BufferUniformView alm::gfx::RenderGraph::GetBufferUniformView(RGBufferHandle handle)
{
	auto buffer = GetBuffer(handle);
	return buffer ? buffer->GetUniformView() : rhi::c_InvalidDescriptorIndex;
}

alm::rhi::BufferReadOnlyView alm::gfx::RenderGraph::GetBufferReadOnlyView(RGBufferHandle handle)
{
	auto buffer = GetBuffer(handle);
	return buffer ? buffer->GetReadOnlyView() : rhi::c_InvalidDescriptorIndex;
}

alm::rhi::BufferReadWriteView alm::gfx::RenderGraph::GetBufferReadWriteView(RGBufferHandle handle)
{
	auto buffer = GetBuffer(handle);
	return buffer ? buffer->GetReadWriteView() : rhi::c_InvalidDescriptorIndex;
}

alm::rhi::CommandListHandle alm::gfx::RenderGraph::GetCommandList()
{
	return m_CommandLists[m_DeviceManager->GetFrameModuleIndex()].get_weak();
}

size_t alm::gfx::RenderGraph::GetNumRenderStages(const std::string& mode) const
{
	auto it = m_RenderModes.find(mode.empty() ? m_CurrentRenderMode : mode);
	if (it == m_RenderModes.end())
	{
		LOG_WARNING("Render mode requested '{}' does not exists", m_CurrentRenderMode);
		return 0;
	}
	return it->second.size();
}

const alm::gfx::RenderGraph::StageData* alm::gfx::RenderGraph::GetRenderStage(uint32_t idx, const std::string& mode) const
{
	auto it = m_RenderModes.find(mode.empty() ? m_CurrentRenderMode : mode);
	if (it == m_RenderModes.end())
	{
		LOG_WARNING("Render mode requested '{}' does not exists", m_CurrentRenderMode);
		return 0;
	}

	return it->second.at(idx);
}

alm::gfx::RGTextureViewTicket alm::gfx::RenderGraph::RequestTextureView(RenderStage* rs, AccessMode accessMode, RGTextureHandle handle)
{
	for (auto& entry : m_TexViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode && entry->handle == handle)
		{
			++entry->refCount;
			return RGTextureViewTicket{ entry };
		}
	}

	auto* ticket = new TextureViewRequest{ rs, accessMode, handle, 1, rhi::TextureOwner{} };
	m_TexViewRequests.push_back(ticket);
	return RGTextureViewTicket{ ticket };
}

alm::gfx::RGBufferViewTicket alm::gfx::RenderGraph::RequestBufferView(RenderStage* rs, AccessMode accessMode, RGBufferHandle handle)
{
	for (auto& entry : m_BufferViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode && entry->handle == handle)
		{
			++entry->refCount;
			return RGBufferViewTicket{ entry };
		}
	}

	auto* ticket = new BufferViewRequest{ rs, accessMode, handle, 1, rhi::BufferOwner{} };
	m_BufferViewRequests.push_back(ticket);
	return RGBufferViewTicket{ ticket };
}

void alm::gfx::RenderGraph::ReleaseTextureView(RGTextureViewTicket ticket)
{
	auto it = std::find(m_TexViewRequests.begin(), m_TexViewRequests.end(), ticket.ptr);
	if (it != m_TexViewRequests.end())
	{
		if (--((*it)->refCount) == 0)
		{
			delete* it;
			m_TexViewRequests.erase(it);
		}
	}
}

void alm::gfx::RenderGraph::ReleaseBufferView(RGBufferViewTicket ticket)
{
	auto it = std::find(m_BufferViewRequests.begin(), m_BufferViewRequests.end(), ticket.ptr);
	if (it != m_BufferViewRequests.end())
	{
		if (--((*it)->refCount) == 0)
		{
			delete* it;
			m_BufferViewRequests.erase(it);
		}
	}
}

alm::rhi::TextureHandle alm::gfx::RenderGraph::GetTextureView(RGTextureViewTicket ticket)
{
	auto it = std::find(m_TexViewRequests.begin(), m_TexViewRequests.end(), ticket.ptr);
	if (it == m_TexViewRequests.end())
		return nullptr;

	return (*it)->tex.get_weak();
}

alm::rhi::BufferHandle alm::gfx::RenderGraph::GetBufferView(RGBufferViewTicket ticket)
{
	auto it = std::find(m_BufferViewRequests.begin(), m_BufferViewRequests.end(), ticket.ptr);
	if (it == m_BufferViewRequests.end())
		return nullptr;

	return (*it)->buffer.get_weak();
}

alm::rhi::FramebufferHandle alm::gfx::RenderGraph::GetFramebuffer()
{
	return m_RenderView->GetFramebuffer();
}

alm::gfx::RenderGraph::DeclaredTexture* alm::gfx::RenderGraph::GetDeclTex(RGTextureHandle handle)
{
	return reinterpret_cast<DeclaredTexture*>(handle.ptr);
}

alm::gfx::RenderGraph::DeclaredBuffer* alm::gfx::RenderGraph::GetDeclBuffer(RGBufferHandle handle)
{
	return reinterpret_cast<DeclaredBuffer*>(handle.ptr);
}

alm::gfx::RenderGraph::RequestedFramebufferData* alm::gfx::RenderGraph::GetReqFb(RGFramebufferHandle handle)
{
	return reinterpret_cast<RequestedFramebufferData*>(handle.ptr);
}

alm::gfx::RGTextureHandle alm::gfx::RenderGraph::GetHandle(DeclaredTexture* declTex)
{
	return RGTextureHandle{ declTex };
}

alm::gfx::RGBufferHandle alm::gfx::RenderGraph::GetHandle(DeclaredBuffer* declBuffer)
{
	return RGBufferHandle{ declBuffer };
}

std::vector<alm::gfx::RenderGraph::TextureViewRequest*> alm::gfx::RenderGraph::GetTexViewRequests(RenderStage* rs, AccessMode accessMode)
{
	std::vector<alm::gfx::RenderGraph::TextureViewRequest*> ret;
	for (auto& entry : m_TexViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode)
		{
			ret.push_back(entry);
		}
	}
	return ret;
}

void alm::gfx::RenderGraph::UpdateRequestedTextureViews(alm::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
	const std::map<RGTextureHandle, rhi::ResourceState> resourceStates)
{
	auto requests = GetTexViewRequests(rs, accessMode);
	for (auto req : requests)
	{
		rhi::TextureHandle sourceTex = GetDeclTex(req->handle)->texture.get_weak();
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
			if (accessMode == alm::gfx::RenderGraph::AccessMode::Read)
				debugName << "Read";
			else
				debugName << "Write";
			debugName << " - " << sourceTex->GetDebugName();

			req->tex = m_DeviceManager->GetDevice()->CreateTexture(desc, rhi::ResourceState::SHADER_RESOURCE, debugName.str().c_str());
			assert(req->tex);
		}

		rhi::ResourceState srcTexState = resourceStates.find(req->handle)->second;

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

std::vector<alm::gfx::RenderGraph::BufferViewRequest*> alm::gfx::RenderGraph::GetBufferViewRequests(RenderStage* rs, AccessMode accessMode)
{
	std::vector<alm::gfx::RenderGraph::BufferViewRequest*> ret;
	for (auto& entry : m_BufferViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode)
		{
			ret.push_back(entry);
		}
	}
	return ret;
}

void alm::gfx::RenderGraph::UpdateRequestedBufferViews(alm::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
	const std::map<RGBufferHandle, rhi::ResourceState> resourceStates)
{
	auto requests = GetBufferViewRequests(rs, accessMode);
	for (auto req : requests)
	{
		rhi::BufferHandle srcBuffer = GetDeclBuffer(req->handle)->buffer.get_weak();
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
			if (accessMode == alm::gfx::RenderGraph::AccessMode::Read)
				debugName << "Read";
			else
				debugName << "Write";
			debugName << " - " << srcBuffer->GetDebugName();

			req->buffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::COMMON, debugName.str().c_str());
			assert(req->buffer);
		}

		rhi::ResourceState srcBufferState = resourceStates.find(req->handle)->second;

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
