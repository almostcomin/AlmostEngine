#include "RenderAPI/dx12/GpuDevice.h"
#include "RenderAPI/dx12/DescriptorHeap.h"
#include "RenderAPI/dx12/Buffer.h"
#include "RenderAPI/dx12/Texture.h"
#include "RenderAPI/dx12/Framebuffer.h"
#include "RenderAPI/dx12/CommandList.h"
#include "RenderAPI/dx12/Fence.h"
#include "RenderAPI/dx12/Shader.h"
#include "RenderAPI/dx12/DxgiFormat.h"
#include "RenderAPI/dx12/ResourceState.h"
#include "RenderAPI/dx12/PipelineState.h"
#include "RenderAPI/dx12/Utils.h"
#include "Core/Util.h"
#include "Core/Log.h"
#include <array>

#define HR_RETURN_NULL(hr)			\
	do {							\
		if(FAILED(hr)) {			\
			LOG_ERROR("HRESULT error code = {:#x} in function '{}'", hr,  __FUNCTION__); \
			return nullptr;			\
		}							\
	} while(0)

namespace
{
	void WaitForFence(ID3D12Fence* fence, uint64_t value, HANDLE event)
	{
		// Test if the fence has been reached
		if (fence->GetCompletedValue() < value)
		{
			// If it's not, wait for it to finish using an event
			ResetEvent(event);
			fence->SetEventOnCompletion(value, event);
			WaitForSingleObject(event, INFINITE);
		}
	}

	D3D12_CLEAR_VALUE ConvertTextureClearValue(const st::rapi::TextureDesc& d)
	{
		const auto& formatMapping = st::rapi::dx12::GetDxgiFormatMapping(d.format);
		const auto& formatInfo = st::rapi::GetFormatInfo(d.format);
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = formatMapping.rtvFormat;
		if (formatInfo.hasDepth || formatInfo.hasStencil)
		{
			clearValue.DepthStencil.Depth = d.clearValue.x;
			clearValue.DepthStencil.Stencil = UINT8(d.clearValue.y);
		}
		else
		{
			clearValue.Color[0] = d.clearValue.x;
			clearValue.Color[1] = d.clearValue.y;
			clearValue.Color[2] = d.clearValue.z;
			clearValue.Color[3] = d.clearValue.w;
		}

		return clearValue;
	}
} // anonymous namespace

std::unique_ptr<st::rapi::Device> st::rapi::dx12::CreateDevice(const st::rapi::dx12::DeviceDesc& desc)
{
	return std::make_unique<st::rapi::dx12::GpuDevice>(desc);
}

st::rapi::dx12::GpuDevice::GpuDevice(const st::rapi::dx12::DeviceDesc& desc) :
	m_DepthStencilViewHeap{ desc.pDevice },
	m_RenderTargetViewHeap{ desc.pDevice },
	m_ShaderResourceViewHeap{ desc.pDevice },
	m_SamplerHeap{ desc.pDevice },
	m_CurrentFrameIdx{ 1 },
	m_Desc{ desc }
{
	m_D3d12Device = desc.pDevice;

	if (desc.pGraphicsCommandQueue)
	{
		m_Queues[int(QueueType::Graphics)].queue = desc.pGraphicsCommandQueue;
		m_D3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Queues[int(QueueType::Graphics)].fence));
	}
	if (desc.pComputeCommandQueue)
	{
		m_Queues[int(QueueType::Compute)].queue = desc.pGraphicsCommandQueue;
		m_D3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Queues[int(QueueType::Compute)].fence));
	}
	if (desc.pCopyCommandQueue)
	{
		m_Queues[int(QueueType::Copy)].queue = desc.pGraphicsCommandQueue;
		m_D3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Queues[int(QueueType::Copy)].fence));
	}

	m_DepthStencilViewHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, desc.depthStencilViewHeapSize, false);
	m_RenderTargetViewHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, desc.renderTargetViewHeapSize, false);
	m_ShaderResourceViewHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
		std::min(desc.shaderResourceViewHeapSize, (uint32_t)D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2), true);
	m_SamplerHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 
		std::min(desc.samplerHeapSize, (uint32_t)D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE), true);

	m_D3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &m_Options, sizeof(m_Options));
	m_D3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &m_Options1, sizeof(m_Options1));
	bool hasOptions5 = SUCCEEDED(m_D3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &m_Options5, sizeof(m_Options5)));
	bool hasOptions6 = SUCCEEDED(m_D3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &m_Options6, sizeof(m_Options6)));
	bool hasOptions7 = SUCCEEDED(m_D3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &m_Options7, sizeof(m_Options7)));

	if (SUCCEEDED(m_D3d12Device->QueryInterface(IID_PPV_ARGS(&m_D3d12Device2))) && hasOptions7)
	{
		m_MeshletsSupported = m_Options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
	}

	if (SUCCEEDED(m_D3d12Device->QueryInterface(IID_PPV_ARGS(&m_D3d12Device5))) && hasOptions5)
	{
		m_RayTracingSupported = m_Options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
		m_TraceRayInlineSupported = m_Options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1;
	}

	if (SUCCEEDED(m_D3d12Device->QueryInterface(IID_PPV_ARGS(&m_D3d12Device8))) && hasOptions7)
	{
		m_SamplerFeedbackSupported = m_Options7.SamplerFeedbackTier >= D3D12_SAMPLER_FEEDBACK_TIER_0_9;
	}

	if (hasOptions6)
	{
		m_VariableRateShadingSupported = m_Options6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2;
	}

	{
		D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };
		bool hasShaderModel = SUCCEEDED(m_D3d12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

		m_HeapDirectlyIndexedSupported = m_Options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3 &&
			hasShaderModel && shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6;
	}

	m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);

	CreateBindlessRootSignature();

	m_StaleResources.resize(m_Desc.swapChainFrames);
};

st::rapi::dx12::GpuDevice::~GpuDevice()
{
	WaitForIdle();

	if (m_FenceEvent)
	{
		CloseHandle(m_FenceEvent);
		m_FenceEvent = nullptr;
	}
}

st::rapi::ShaderHandle st::rapi::dx12::GpuDevice::CreateShader(const ShaderDesc& desc, const WeakBlob& bytecode)
{
	return InsertNewResource<IShader>(new Shader{desc, bytecode});
}

st::rapi::BufferHandle st::rapi::dx12::GpuDevice::CreateBuffer(const BufferDesc& desc, ResourceState initialState)
{
	auto storageReq = GetStorageRequirements(desc);
	BufferDesc fixedDesc = std::move(desc);
	fixedDesc.sizeBytes = storageReq.size;

	D3D12_RESOURCE_DESC d3d12Desc = {};
	d3d12Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d3d12Desc.Alignment = storageReq.alignment;
	d3d12Desc.Width = fixedDesc.sizeBytes;
	d3d12Desc.Height = 1;
	d3d12Desc.DepthOrArraySize = 1;
	d3d12Desc.MipLevels = 1;
	d3d12Desc.Format = DXGI_FORMAT_UNKNOWN;
	d3d12Desc.SampleDesc.Count = 1;
	d3d12Desc.SampleDesc.Quality = 0;
	d3d12Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3d12Desc.Flags = fixedDesc.allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	// TODO: D3D12MA
	D3D12_HEAP_PROPERTIES heapProps = {};
	switch (fixedDesc.memoryAccess)
	{
	case MemoryAccess::Default:
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case MemoryAccess::Upload:
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		break;
	case MemoryAccess::Readback:
		heapProps.Type = D3D12_HEAP_TYPE_READBACK;
		break;
	}
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ComPtr<ID3D12Resource> d3d12Buffer;
	HRESULT hr = m_D3d12Device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &d3d12Desc, MapResourceState(initialState), nullptr, IID_PPV_ARGS(&d3d12Buffer));
	HR_RETURN_NULL(hr);

	auto ws_nmame = ToWide(fixedDesc.debugName.c_str());
	d3d12Buffer->SetName(ws_nmame.c_str());

	return InsertNewResource<IBuffer>(new Buffer{ fixedDesc, d3d12Buffer.Get(), this });
}

st::rapi::TextureHandle st::rapi::dx12::GpuDevice::CreateTexture(const TextureDesc& desc, ResourceState initialState)
{
	D3D12_RESOURCE_DESC d3d12Desc = BuildD3d12Desc(desc);

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;

	D3D12_CLEAR_VALUE clearValue;
	if (desc.useClearValue)
	{
		clearValue = ConvertTextureClearValue(desc);
	}

	ComPtr<ID3D12Resource> d3d12Texture;
	HRESULT hr = m_D3d12Device->CreateCommittedResource(
		&heapProps,
		heapFlags,
		&d3d12Desc,
		MapResourceState(initialState),
		desc.useClearValue ? &clearValue : nullptr,
		IID_PPV_ARGS(&d3d12Texture));
	HR_RETURN_NULL(hr);

	auto ws_nmame = ToWide(desc.debugName.c_str());
	d3d12Texture->SetName(ws_nmame.c_str());

	return InsertNewResource<ITexture>(new Texture{ desc, d3d12Texture, this });
}

st::rapi::TextureHandle st::rapi::dx12::GpuDevice::CreateHandleForNativeTexture(void* obj, const TextureDesc& desc)
{
	if (obj == nullptr)
	{
		return nullptr;
	}

	ID3D12Resource* d3d12Resource = static_cast<ID3D12Resource*>(obj);

	auto ws_nmame = ToWide(desc.debugName.c_str());
	d3d12Resource->SetName(ws_nmame.c_str());

	return InsertNewResource<ITexture>(new Texture{ desc, d3d12Resource, this });
}

st::rapi::FramebufferHandle st::rapi::dx12::GpuDevice::CreateFramebuffer(const FramebufferDesc& desc)
{
	auto* fb = new Framebuffer;
	fb->desc = desc;
	fb->framebufferInfo = FramebufferInfo{ desc };

	if (!desc.ColorAttachments.empty())
	{
		Texture* texture = checked_cast<Texture*>(desc.ColorAttachments[0].texture);
		fb->rtWidth = texture->GetDesc().width;
		fb->rtHeight = texture->GetDesc().height;
	}
	else if (desc.DepthAttachment.Valid())
	{
		Texture* texture = checked_cast<Texture*>(desc.DepthAttachment.texture);
		fb->rtWidth = texture->GetDesc().width;
		fb->rtHeight = texture->GetDesc().height;
	}

	for (size_t rt = 0; rt < desc.ColorAttachments.size(); rt++)
	{
		auto& attachment = desc.ColorAttachments[rt];

		Texture* texture = checked_cast<Texture*>(attachment.texture);
		assert(texture->GetDesc().width == fb->rtWidth);
		assert(texture->GetDesc().height == fb->rtHeight);

		DescriptorIndex index = m_RenderTargetViewHeap.AllocateDescriptor();

		const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_RenderTargetViewHeap.GetCpuHandle(index);
		texture->CreateRTV(descriptorHandle, attachment.format, attachment.subresources);

		fb->RTVs.push_back(index);

		st::rapi::TextureHandle handle = st::checked_pointer_cast<ITexture>(texture->weak_from_this());
		fb->rtvTextures.push_back(handle);
	}

	if (desc.DepthAttachment.Valid())
	{
		Texture* texture = checked_cast<Texture*>(desc.DepthAttachment.texture);
		assert(texture->GetDesc().width == fb->rtWidth);
		assert(texture->GetDesc().height == fb->rtHeight);

		DescriptorIndex index = m_DepthStencilViewHeap.AllocateDescriptor();

		const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_DepthStencilViewHeap.GetCpuHandle(index);
		texture->CreateDSV(descriptorHandle, desc.DepthAttachment.subresources, desc.DepthAttachment.isReadOnly);

		fb->DSV = index;

		st::rapi::TextureHandle handle = st::checked_pointer_cast<ITexture>(texture->weak_from_this());
		fb->dsvTexture = handle;
	}

	return InsertNewResource<IFramebuffer>(fb);
}

st::rapi::CommandListHandle st::rapi::dx12::GpuDevice::CreateCommandList(const CommandListParams& params)
{
	if (!m_Queues[(int)params.queueType].queue)
	{
		return nullptr;
	}

	D3D12_COMMAND_LIST_TYPE d3dCommandListType;
	switch (params.queueType)
	{
	case QueueType::Graphics:
		d3dCommandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;
		break;
	case QueueType::Compute:
		d3dCommandListType = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		break;
	case QueueType::Copy:
		d3dCommandListType = D3D12_COMMAND_LIST_TYPE_COPY;
		break;
	case QueueType::_Count:
	default:
		assert(0);
		return nullptr;
	}

	ComPtr<ID3D12CommandAllocator> d3d12CommandAllocator;
	HRESULT hr = m_D3d12Device->CreateCommandAllocator(d3dCommandListType, IID_PPV_ARGS(&d3d12CommandAllocator));
	HR_RETURN_NULL(hr);
	ComPtr<CommandList::NativeCommandListType> d3d12CommandList;
	hr = m_D3d12Device->CreateCommandList(0, d3dCommandListType, d3d12CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&d3d12CommandList));
	HR_RETURN_NULL(hr);

	auto ws_nmame = ToWide(params.debugName.c_str());
	d3d12CommandList->SetName(ws_nmame.c_str());

	d3d12CommandList->Close(); // Start closed

	return InsertNewResource<ICommandList>(new CommandList{ d3d12CommandList.Get(), d3d12CommandAllocator.Get(), params.queueType, this, params.debugName });
}

st::rapi::GraphicsPipelineStateHandle st::rapi::dx12::GpuDevice::CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, const FramebufferInfo& fbInfo)
{
	// TODO: cache

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12Desc = {};
	d3d12Desc.pRootSignature = m_BindlessRootSignature.Get();
	d3d12Desc.VS = desc.VS ? D3D12_SHADER_BYTECODE{ desc.VS->GetBytecode().data(), desc.VS->GetBytecode().size() } : D3D12_SHADER_BYTECODE{};
	d3d12Desc.PS = desc.PS ? D3D12_SHADER_BYTECODE{ desc.PS->GetBytecode().data(), desc.PS->GetBytecode().size() } : D3D12_SHADER_BYTECODE{};
	d3d12Desc.DS = desc.DS ? D3D12_SHADER_BYTECODE{ desc.DS->GetBytecode().data(), desc.DS->GetBytecode().size() } : D3D12_SHADER_BYTECODE{};
	d3d12Desc.HS = desc.HS ? D3D12_SHADER_BYTECODE{ desc.HS->GetBytecode().data(), desc.HS->GetBytecode().size() } : D3D12_SHADER_BYTECODE{};
	d3d12Desc.GS = desc.GS ? D3D12_SHADER_BYTECODE{ desc.GS->GetBytecode().data(), desc.GS->GetBytecode().size() } : D3D12_SHADER_BYTECODE{};
	d3d12Desc.BlendState = GetBlendDesc(desc.blendState);
	d3d12Desc.SampleMask = ~0u;
	d3d12Desc.RasterizerState = GetRasterizerState(desc.rasterState);
	d3d12Desc.DepthStencilState = GetDepthStencilState(desc.depthStencilState);
	d3d12Desc.PrimitiveTopologyType = GetPrimitiveType(desc.primTopo);
	d3d12Desc.NumRenderTargets = fbInfo.colorFormats.size();
	for (uint32_t i = 0; i < d3d12Desc.NumRenderTargets; ++i)
		d3d12Desc.RTVFormats[i] = GetDxgiFormatMapping(fbInfo.colorFormats[i]).rtvFormat;
	d3d12Desc.DSVFormat = GetDxgiFormatMapping(fbInfo.depthFormat).rtvFormat;
	d3d12Desc.SampleDesc.Count = fbInfo.sampleCount;
	d3d12Desc.SampleDesc.Quality = fbInfo.sampleQuality;

	ComPtr<ID3D12PipelineState> d3d12PSO;
	HRESULT hr = m_D3d12Device->CreateGraphicsPipelineState(&d3d12Desc, IID_PPV_ARGS(&d3d12PSO));
	HR_RETURN_NULL(hr);

	auto ws_nmame = ToWide(desc.debugName.c_str());
	d3d12PSO->SetName(ws_nmame.c_str());

	return InsertNewResource<IGraphicsPipelineState>(new GraphicsPipelineState{ d3d12PSO, d3d12Desc, desc });
}

st::rapi::FenceHandle st::rapi::dx12::GpuDevice::CreateFence(uint64_t initialVale, const char* debugName)
{
	ComPtr<ID3D12Fence> d3d12Fence;
	HRESULT hr = m_D3d12Device->CreateFence(initialVale, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence));
	HR_RETURN_NULL(hr);

	auto ws_nmame = ToWide(debugName);
	d3d12Fence->SetName(ws_nmame.c_str());

	return InsertNewResource<IFence>(new Fence{ d3d12Fence.Get(), debugName ? debugName : "{null}" });
}

st::rapi::StorageRequirements st::rapi::dx12::GpuDevice::GetStorageRequirements(const BufferDesc& desc)
{
	StorageRequirements ret = { 
		.size = desc.sizeBytes, 
		.alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT };

	if (hasFlag(desc.shaderUsage, BufferShaderUsage::ConstantBuffer))
	{
		ret.size = AlignUp(desc.sizeBytes, (size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	}

	return ret;
}

st::rapi::StorageRequirements st::rapi::dx12::GpuDevice::GetStorageRequirements(const TextureDesc& desc)
{
	D3D12_RESOURCE_DESC d3d12Desc = BuildD3d12Desc(desc);	
	D3D12_RESOURCE_ALLOCATION_INFO allocInfo = m_D3d12Device->GetResourceAllocationInfo(0, 1, &d3d12Desc);	
	return StorageRequirements{ .size = allocInfo.SizeInBytes, .alignment = allocInfo.Alignment };
}

st::rapi::StorageRequirements st::rapi::dx12::GpuDevice::GetCopyableRequirements(const BufferDesc& desc)
{
	return StorageRequirements{ .size = desc.sizeBytes, .alignment = GetCopyDataAlignment(CopyMethod::Buffer2Buffer) };
}

st::rapi::StorageRequirements st::rapi::dx12::GpuDevice::GetCopyableRequirements(const TextureDesc& desc)
{
	st::rapi::StorageRequirements ret = {};
	D3D12_RESOURCE_DESC d3d12Desc = BuildD3d12Desc(desc);

	m_D3d12Device->GetCopyableFootprints(&d3d12Desc, 0, d3d12Desc.MipLevels * d3d12Desc.DepthOrArraySize, 0, nullptr, nullptr, nullptr, &ret.size);
	ret.alignment = GetCopyDataAlignment(CopyMethod::Buffer2Texture);

	return ret;
}

st::rapi::SubresourceCopyableRequirements st::rapi::dx12::GpuDevice::GetSubresourceCopyableRequirements(const TextureDesc& desc, uint32_t mipLevel, uint32_t arraySlice)
{
	D3D12_RESOURCE_DESC d3d12Desc = BuildD3d12Desc(desc);

	SubresourceCopyableRequirements copyReq = {};

	auto subresidx = D3D12CalcSubresource(mipLevel, arraySlice, 0, desc.mipLevels, desc.arraySize);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresInfo = {};
	uint32_t numRows = 0;
	uint64_t rowSizeBytes = 0;

	m_D3d12Device->GetCopyableFootprints(
		&d3d12Desc, subresidx, 1, 0, &subresInfo, &numRows, &rowSizeBytes, nullptr);

	copyReq.offset = subresInfo.Offset;
	copyReq.numRows = numRows;
	copyReq.rowSizeBytes = rowSizeBytes;
	copyReq.rowStride = subresInfo.Footprint.RowPitch;

	return copyReq;
}

size_t st::rapi::dx12::GpuDevice::GetCopyDataAlignment(CopyMethod method)
{
	switch (method)
	{
	case CopyMethod::Buffer2Buffer:
		return 4;
	case CopyMethod::Buffer2Texture:
		return D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	default:
		assert(0);
		return 1;
	}
}

void st::rapi::dx12::GpuDevice::ExecuteCommandLists(std::span<ICommandList*> commandLists, QueueType type, IFence* signal, uint64_t value)
{
	if (!m_Queues[(int)type].queue)
	{
		LOG_ERROR("Queue type '%d' not initialized", (int)type);
		return;
	}

	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(commandLists.size());
	for (auto cl : commandLists)
		d3d12CommandLists.push_back(cl->GetNativeResource());

	m_Queues[(int)type].queue->ExecuteCommandLists(d3d12CommandLists.size(), d3d12CommandLists.data());

	if (signal)
	{
		m_Queues[(int)type].queue->Signal(signal->GetNativeResource(), value);
	}
}

void st::rapi::dx12::GpuDevice::ExecuteCommandList(ICommandList* commandList, QueueType type, IFence* signal, uint64_t value)
{
	ExecuteCommandLists(std::span<ICommandList*, 1>{&commandList, 1}, type, signal, value);
}

void st::rapi::dx12::GpuDevice::NextFrame()
{
	uint64_t nextFrameModule = (m_CurrentFrameIdx + 1) % m_Desc.swapChainFrames;

	// Remove the stale resources
	{
		std::scoped_lock lock{ m_LivingResourcesMutex };
		for (IResource* pres : m_StaleResources[nextFrameModule])
		{
			ReleaseResource(pres);
			m_LivingResources.erase(pres);
		}
	}

	m_StaleResources[nextFrameModule].clear();
	++m_CurrentFrameIdx;
}

void st::rapi::dx12::GpuDevice::Shutdown()
{
	for(int i = 0; i < m_StaleResources.size(); ++i)
	{
		for(int j = 0; j < m_StaleResources[i].size(); ++j)
		{
			IResource* pres = m_StaleResources[i][j];
			ReleaseResource(pres);
			m_LivingResources.erase(pres);
		}
	}

	if (!m_LivingResources.empty())
	{
		for (auto& res : m_LivingResources)
		{
			LOG_ERROR("Resource addr [0x{:08x}] of type {}, debug name '{}' not released!", 
				(uintptr_t)res.get(), ResourceTypeToString(res->GetResourceType()), res->GetDebugName());
			ReleaseResource(res.get());
		}
		m_LivingResources.clear();
	}
}

void st::rapi::dx12::GpuDevice::ReleaseImmediatelyInternal(IResource* resource)
{
	ReleaseResource(resource);

	std::scoped_lock lock{ m_LivingResourcesMutex };
	m_LivingResources.erase(resource);
}

void st::rapi::dx12::GpuDevice::ReleaseQueuedInternal(IResource* resource)
{
	if (!resource) 
		return;

	std::scoped_lock lock{ m_StaleResourcesMutex };
	m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].push_back(resource);
}

void st::rapi::dx12::GpuDevice::WaitForIdle()
{
	// Wait for every queue to reach its last submitted instance
	for (auto& queue : m_Queues)
	{
		if (!queue.queue)
			continue;

		if (queue.UpdateLastCompletedInstance() < queue.lastSubmittedInstance)
		{
			WaitForFence(queue.fence.Get(), queue.lastSubmittedInstance, m_FenceEvent);
		}
	}
}

void st::rapi::dx12::GpuDevice::CreateBindlessRootSignature()
{
	// Need to map BindlessRS.hlsli

	// --- Static samplers

	D3D12_STATIC_SAMPLER_DESC staticSamplers[10] = {};

	staticSamplers[0] = {
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		0,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[1] = {
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		1,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[2] = {
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		2,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[3] = {
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		3,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[4] = {
		D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		4,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[5] = {
		D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		5,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[6] = {
		D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		6,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[7] = {
		D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		7,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[8] = {
		D3D12_FILTER_ANISOTROPIC,				// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// U
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// V
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// W
		0.0f,									// mipLODBias
		16,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		8,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	staticSamplers[9] = {
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,		// U
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,		// V
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,		// W
		0.0f,									// mipLODBias
		0,										// maxAnisotropy
		D3D12_COMPARISON_FUNC_ALWAYS,			// comparisonFunc
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,	// borderColor
		0.0f,									// minLOD
		100.0f,									// maxLOD
		9,										// shaderRegister
		0,										// registerSpace
		D3D12_SHADER_VISIBILITY_ALL				// shaderVisibility
	};

	// --- Root Constants (64 x 32-bit = 256 bytes)

	D3D12_ROOT_PARAMETER rootParams[1] = {};
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParams[0].Constants.Num32BitValues = 64;
	rootParams[0].Constants.RegisterSpace = 0;
	rootParams[0].Constants.ShaderRegister = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// --- Create root signature

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = 1;
	desc.pParameters = rootParams;
	desc.NumStaticSamplers = _countof(staticSamplers);
	desc.pStaticSamplers = staticSamplers;
	desc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
		D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorsBlob;

	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorsBlob);
	assert(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		LPCSTR errors = (LPCSTR)errorsBlob->GetBufferPointer();
		LOG_FATAL("Failed serializing root signature:\n{}", errors);
	}

	hr = m_D3d12Device->CreateRootSignature(
		0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_BindlessRootSignature));

	assert(SUCCEEDED(hr));
}

D3D12_RESOURCE_DESC st::rapi::dx12::GpuDevice::BuildD3d12Desc(const TextureDesc& desc)
{
	const DxgiFormatMapping& formatMap = GetDxgiFormatMapping(desc.format);
	const FormatInfo& formatInfo = GetFormatInfo(desc.format);

	D3D12_RESOURCE_DESC d3d12Desc = {};

	switch (desc.dimension)
	{
	case TextureDimension::Texture1D:
	case TextureDimension::Texture1DArray:
		d3d12Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
		d3d12Desc.DepthOrArraySize = desc.arraySize;
		break;
	case TextureDimension::Texture2D:
	case TextureDimension::Texture2DArray:
	case TextureDimension::TextureCube:
	case TextureDimension::TextureCubeArray:
	case TextureDimension::Texture2DMS:
	case TextureDimension::Texture2DMSArray:
		d3d12Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		d3d12Desc.DepthOrArraySize = desc.arraySize;
		break;
	case TextureDimension::Texture3D:
		d3d12Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		d3d12Desc.DepthOrArraySize = desc.depth;
		break;
	default:
		assert(!"Invalid Enumeration Value");
	}
	d3d12Desc.Alignment = 0; // D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
	d3d12Desc.Width = desc.width;
	d3d12Desc.Height = desc.height;
	d3d12Desc.MipLevels = desc.mipLevels;
	d3d12Desc.Format = formatMap.resourceFormat;
	d3d12Desc.SampleDesc.Count = desc.sampleCount;
	d3d12Desc.SampleDesc.Quality = desc.sampleQuality;
	if (!hasFlag(desc.shaderUsage, TextureShaderUsage::ShaderResource))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	if (hasFlag(desc.shaderUsage, TextureShaderUsage::RenderTarget))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (hasFlag(desc.shaderUsage, TextureShaderUsage::DepthStencil))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (hasFlag(desc.shaderUsage, TextureShaderUsage::UnorderedAccess))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (desc.isTiled)
		d3d12Desc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;

	return d3d12Desc;
}