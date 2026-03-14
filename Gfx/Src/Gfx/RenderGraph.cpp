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

	st::rhi::ResourceState GetInitialState(st::gfx::RenderGraph::TextureResourceType type)
	{
		switch (type)
		{
		case st::gfx::RenderGraph::TextureResourceType::RenderTarget:
			return st::rhi::ResourceState::RENDERTARGET;
		case st::gfx::RenderGraph::TextureResourceType::DepthStencil:
			return st::rhi::ResourceState::DEPTHSTENCIL;
		case st::gfx::RenderGraph::TextureResourceType::ShaderResource:
			return st::rhi::ResourceState::SHADER_RESOURCE;
		default:
			assert(0);
			return st::rhi::ResourceState::SHADER_RESOURCE;
		}
	}

	st::rhi::TextureShaderUsage GetTextureShaderUsage(st::gfx::RenderGraph::TextureResourceType type, bool needUAV)
	{
		st::rhi::TextureShaderUsage usage;
		switch (type)
		{
		case st::gfx::RenderGraph::TextureResourceType::RenderTarget:
			usage = st::rhi::TextureShaderUsage::Sampled | st::rhi::TextureShaderUsage::ColorTarget;
			break;
		case st::gfx::RenderGraph::TextureResourceType::DepthStencil:
			usage = st::rhi::TextureShaderUsage::Sampled | st::rhi::TextureShaderUsage::DepthTarget;
			break;
		case st::gfx::RenderGraph::TextureResourceType::ShaderResource:
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

st::gfx::RenderGraph::RenderGraph(RenderView* renderView, const char* debugName) :
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

st::gfx::RenderGraph::~RenderGraph()
{
	Reset();
}

void st::gfx::RenderGraph::Reset()
{
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

void st::gfx::RenderGraph::SetRenderStages(const std::vector<std::shared_ptr<RenderStage>>& renderStages)
{
	Reset();

	m_RenderStages.reserve(renderStages.size());
	for (auto& rs : renderStages)
	{
		auto rsd = new StageData;
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
		RenderGraphBuilder builder{ this, rs->renderStage.get() };
		rs->renderStage->Setup(builder);
		
		rs->textureReads = builder.GetTextureReadDependencies();
		rs->textureWrites = builder.GetTextureReadDependencies();
		rs->bufferReads = builder.GetBufferReadDependencies();
		rs->bufferWrites = builder.GetBufferWriteDependencies();
	}

	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->Attach(this);
	}
}

void st::gfx::RenderGraph::SetRenderMode(const std::string& name, const std::vector<RenderStage*>& renderStages)
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

void st::gfx::RenderGraph::SetCurrentRenderMode(const std::string& name)
{
	if (m_RenderModes.find(name) == m_RenderModes.end())
	{
		LOG_WARNING("Render mode '{}' not found", name);
		return;
	}
	m_CurrentRenderMode = name;
}

std::vector<std::string> st::gfx::RenderGraph::GetRenderModes() const
{
	std::vector<std::string> result;
	result.reserve(m_RenderModes.size());
	for (const auto& rm : m_RenderModes)
	{
		result.push_back(rm.first);
	}
	return result;
}

void st::gfx::RenderGraph::Compile()
{
}

bool st::gfx::RenderGraph::BeginRender(rhi::ICommandList* /*commandList*/)
{
	return false; // no-operation needed
}

bool st::gfx::RenderGraph::EndRender(rhi::ICommandList* commandList)
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

void st::gfx::RenderGraph::Render(st::rhi::FramebufferHandle /*frameBuffer*/)
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
			rs->timerQueries[m_DeviceManager->GetFrameModuleIndex()]->Reset();
			stageCommandList->BeginTimerQuery(rs->timerQueries[m_DeviceManager->GetFrameModuleIndex()].get());

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

			stageCommandList->EndTimerQuery(rs->timerQueries[m_DeviceManager->GetFrameModuleIndex()].get());
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
	m_DeviceManager->GetDevice()->ExecuteCommandList(stageCommandList, st::rhi::QueueType::Graphics);

}

void st::gfx::RenderGraph::OnSceneChanged()
{
	for (auto& rs : m_RenderStages)
	{
		rs->renderStage->OnSceneChanged();
	}
}

void st::gfx::RenderGraph::OnRenderTargetChanged(const int2& newSize)
{
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

st::gfx::RGTextureHandle st::gfx::RenderGraph::CreateTexture(RenderStage* renderStage, const std::string& id, TextureResourceType type,
																	   int width, int height, int arraySize, st::rhi::Format format, bool needsUAV)
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

st::gfx::RGBufferHandle st::gfx::RenderGraph::CreateBuffer(RenderStage* renderStage, const std::string& id, const st::rhi::BufferDesc& desc)
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

bool st::gfx::RenderGraph::RecreateTexture(RGTextureHandle handle, int width, int height, int arraySize, rhi::Format format)
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

bool st::gfx::RenderGraph::RecreateBuffer(RGBufferHandle handle, const rhi::BufferDesc& desc)
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

void st::gfx::RenderGraph::EnableTexture(RGTextureHandle handle)
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

void st::gfx::RenderGraph::DisableTexture(RGTextureHandle handle)
{
	auto* declTex = GetDeclTex(handle);
	declTex->texture.reset();
}

st::gfx::RGTextureHandle st::gfx::RenderGraph::GetTextureHandle(const std::string& id)
{
	auto it = m_Textures.find(id);
	if (it != m_Textures.end())
	{
		return { it->second.get() };
	}
	return { nullptr };
}

st::gfx::RGBufferHandle st::gfx::RenderGraph::GetBufferHandle(const std::string& id)
{
	auto it = m_Buffers.find(id);
	if (it != m_Buffers.end())
	{
		return { it->second.get() };
	}
	return { nullptr };
}

st::rhi::TextureHandle st::gfx::RenderGraph::GetTexture(RGTextureHandle handle)
{
	return GetDeclTex(handle)->texture.get_weak();
}

st::rhi::BufferHandle st::gfx::RenderGraph::GetBuffer(RGBufferHandle handle)
{
	return GetDeclBuffer(handle)->buffer.get_weak();
}

st::rhi::TextureHandle st::gfx::RenderGraph::GetTexture(const std::string& id)
{
	return GetTexture(GetTextureHandle(id));
}

st::rhi::BufferHandle st::gfx::RenderGraph::GetBuffer(const std::string& id)
{
	return GetBuffer(GetBufferHandle(id));
}

const std::string& st::gfx::RenderGraph::GetId(RGTextureHandle handle)
{
	return GetDeclTex(handle)->id;
}

const std::string& st::gfx::RenderGraph::GetId(RGBufferHandle handle)
{
	return GetDeclBuffer(handle)->id;
}

st::rhi::TextureSampledView st::gfx::RenderGraph::GetTextureSampledView(RGTextureHandle handle)
{
	return GetTexture(handle)->GetSampledView();
}

st::rhi::TextureStorageView st::gfx::RenderGraph::GetTextureStorageView(RGTextureHandle handle)
{
	return GetTexture(handle)->GetStorageView();
}

st::rhi::BufferUniformView st::gfx::RenderGraph::GetBufferUniformView(RGBufferHandle handle)
{
	return GetBuffer(handle)->GetUniformView();
}

st::rhi::BufferReadOnlyView st::gfx::RenderGraph::GetBufferReadOnlyView(RGBufferHandle handle)
{
	return GetBuffer(handle)->GetReadOnlyView();
}

st::rhi::BufferReadWriteView st::gfx::RenderGraph::GetBufferReadWriteView(RGBufferHandle handle)
{
	return GetBuffer(handle)->GetReadWriteView();
}

st::rhi::CommandListHandle st::gfx::RenderGraph::GetCommandList()
{
	return m_CommandLists[m_DeviceManager->GetFrameModuleIndex()].get_weak();
}

size_t st::gfx::RenderGraph::GetNumRenderStages(const std::string& mode) const
{
	auto it = m_RenderModes.find(mode.empty() ? m_CurrentRenderMode : mode);
	if (it == m_RenderModes.end())
	{
		LOG_WARNING("Render mode requested '{}' does not exists", m_CurrentRenderMode);
		return 0;
	}
	return it->second.size();
}

const st::gfx::RenderGraph::StageData* st::gfx::RenderGraph::GetRenderStage(uint32_t idx, const std::string& mode) const
{
	auto it = m_RenderModes.find(mode.empty() ? m_CurrentRenderMode : mode);
	if (it == m_RenderModes.end())
	{
		LOG_WARNING("Render mode requested '{}' does not exists", m_CurrentRenderMode);
		return 0;
	}

	return it->second.at(idx);
}

st::gfx::RGTextureViewTicket st::gfx::RenderGraph::RequestTextureView(RenderStage* rs, AccessMode accessMode, RGTextureHandle handle)
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

st::gfx::RGBufferViewTicket st::gfx::RenderGraph::RequestBufferView(RenderStage* rs, AccessMode accessMode, RGBufferHandle handle)
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

void st::gfx::RenderGraph::ReleaseTextureView(RGTextureViewTicket ticket)
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

void st::gfx::RenderGraph::ReleaseBufferView(RGBufferViewTicket ticket)
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

st::rhi::TextureHandle st::gfx::RenderGraph::GetTextureView(RGTextureViewTicket ticket)
{
	auto it = std::find(m_TexViewRequests.begin(), m_TexViewRequests.end(), ticket.ptr);
	if (it == m_TexViewRequests.end())
		return nullptr;

	return (*it)->tex.get_weak();
}

st::rhi::BufferHandle st::gfx::RenderGraph::GetBufferView(RGBufferViewTicket ticket)
{
	auto it = std::find(m_BufferViewRequests.begin(), m_BufferViewRequests.end(), ticket.ptr);
	if (it == m_BufferViewRequests.end())
		return nullptr;

	return (*it)->buffer.get_weak();
}

st::rhi::FramebufferHandle st::gfx::RenderGraph::GetFramebuffer()
{
	return m_RenderView->GetFramebuffer();
}

st::gfx::RenderGraph::DeclaredTexture* st::gfx::RenderGraph::GetDeclTex(RGTextureHandle handle)
{
	return reinterpret_cast<DeclaredTexture*>(handle.ptr);
}

st::gfx::RenderGraph::DeclaredBuffer* st::gfx::RenderGraph::GetDeclBuffer(RGBufferHandle handle)
{
	return reinterpret_cast<DeclaredBuffer*>(handle.ptr);
}

st::gfx::RGTextureHandle st::gfx::RenderGraph::GetHandle(DeclaredTexture* declTex)
{
	return RGTextureHandle{ declTex };
}

st::gfx::RGBufferHandle st::gfx::RenderGraph::GetHandle(DeclaredBuffer* declBuffer)
{
	return RGBufferHandle{ declBuffer };
}

std::vector<st::gfx::RenderGraph::TextureViewRequest*> st::gfx::RenderGraph::GetTexViewRequests(RenderStage* rs, AccessMode accessMode)
{
	std::vector<st::gfx::RenderGraph::TextureViewRequest*> ret;
	for (auto& entry : m_TexViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode)
		{
			ret.push_back(entry);
		}
	}
	return ret;
}

void st::gfx::RenderGraph::UpdateRequestedTextureViews(st::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
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
			if (accessMode == st::gfx::RenderGraph::AccessMode::Read)
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

std::vector<st::gfx::RenderGraph::BufferViewRequest*> st::gfx::RenderGraph::GetBufferViewRequests(RenderStage* rs, AccessMode accessMode)
{
	std::vector<st::gfx::RenderGraph::BufferViewRequest*> ret;
	for (auto& entry : m_BufferViewRequests)
	{
		if (entry->rs == rs && entry->accessMode == accessMode)
		{
			ret.push_back(entry);
		}
	}
	return ret;
}

void st::gfx::RenderGraph::UpdateRequestedBufferViews(st::rhi::ICommandList* commandList, RenderStage* rs, AccessMode accessMode,
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
			if (accessMode == st::gfx::RenderGraph::AccessMode::Read)
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
