#include "RHI/RHI_PCH.h"

#include "RHI/DxgiFormatMapping.h"
#include "RHI/dx12/GpuDevice.h"
#include "RHI/dx12/DescriptorHeap.h"
#include "RHI/dx12/Buffer.h"
#include "RHI/dx12/Texture.h"
#include "RHI/dx12/Framebuffer.h"
#include "RHI/dx12/CommandList.h"
#include "RHI/dx12/Fence.h"
#include "RHI/dx12/Shader.h"
#include "RHI/dx12/ResourceState.h"
#include "RHI/dx12/PipelineState.h"
#include "RHI/dx12/TimerQuery.h"
#include "RHI/dx12/Utils.h"

#define HR_RETURN_NULL(hr)			\
	do {							\
		if(FAILED(hr)) {			\
			LOG_ERROR("HRESULT error code = {:#x} in function '{}'", hr,  __FUNCTION__); \
			return nullptr;			\
		}							\
	} while(0)

namespace
{
	D3D12_CLEAR_VALUE ConvertTextureClearValue(const st::rhi::TextureDesc& d)
	{
		const auto& formatMapping = st::rhi::GetDxgiFormatMapping(d.format);
		const auto& formatInfo = st::rhi::GetFormatInfo(d.format);
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

std::unique_ptr<st::rhi::Device> st::rhi::dx12::CreateDevice(const st::rhi::dx12::DeviceDesc& desc)
{
	return std::make_unique<st::rhi::dx12::GpuDevice>(desc);
}

void st::rhi::dx12::CheckDRED(ID3D12Device* device)
{
	ID3D12DeviceRemovedExtendedData2* pDred;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&pDred))))
	{
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 dredAutoBreadcrumbsOutput;
		D3D12_DRED_PAGE_FAULT_OUTPUT2 dredPageFaultOutput;

		D3D12_DRED_DEVICE_STATE deviceState = pDred->GetDeviceState();
		(deviceState);

		if (SUCCEEDED(pDred->GetAutoBreadcrumbsOutput1(&dredAutoBreadcrumbsOutput)))
		{
			(dredAutoBreadcrumbsOutput);
			// Each 'Nodes' is a command queue.
			// 'pLastCompletedOp' is the last complemente command
		}

		if (SUCCEEDED(pDred->GetPageFaultAllocationOutput2(&dredPageFaultOutput)))
		{
			(dredAutoBreadcrumbsOutput);
		}
	}
}

st::rhi::dx12::GpuDevice::GpuDevice(const st::rhi::dx12::DeviceDesc& desc) :
	m_DepthStencilViewHeap{ desc.pDevice },
	m_RenderTargetViewHeap{ desc.pDevice },
	m_ShaderResourceViewHeap{ desc.pDevice },
	m_SamplerHeap{ desc.pDevice },
	m_TimerQueries{ desc.maxTimerQueries, true },
	m_CurrentFrameIdx{ 1 },
	m_Desc{ desc }
{
	m_D3d12Device = desc.pDevice;

	//
	// Create command queues
	//
	if (desc.pGraphicsCommandQueue)
	{
		m_Queues[int(QueueType::Graphics)].d3d12Queue = desc.pGraphicsCommandQueue;
		m_D3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Queues[int(QueueType::Graphics)].d3d12Fence));
	}
	if (desc.pComputeCommandQueue)
	{
		m_Queues[int(QueueType::Compute)].d3d12Queue = desc.pGraphicsCommandQueue;
		m_D3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Queues[int(QueueType::Compute)].d3d12Fence));
	}
	if (desc.pCopyCommandQueue)
	{
		m_Queues[int(QueueType::Copy)].d3d12Queue = desc.pGraphicsCommandQueue;
		m_D3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Queues[int(QueueType::Copy)].d3d12Fence));
	}

	//
	// Create descripor heaps
	//
	m_DepthStencilViewHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, desc.depthStencilViewHeapSize, false);
	m_RenderTargetViewHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, desc.renderTargetViewHeapSize, false);
	m_ShaderResourceViewHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
		std::min(desc.shaderResourceViewHeapSize, (uint32_t)D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2), true);
	m_SamplerHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 
		std::min(desc.samplerHeapSize, (uint32_t)D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE), true);

	//
	// Check features supported
	//
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

	//
	// Create Query Heap
	//
	{
		D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		queryHeapDesc.Count = uint32_t(m_TimerQueries.GetCapacity()) * 2; // Use 2 D3D12 queries per 1 TimerQuery
		m_D3d12Device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_TimerQueryHeap));

		D3D12_RESOURCE_DESC d3d12Desc = {};
		d3d12Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d3d12Desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		d3d12Desc.Width = queryHeapDesc.Count * 4;
		d3d12Desc.Height = 1;
		d3d12Desc.DepthOrArraySize = 1;
		d3d12Desc.MipLevels = 1;
		d3d12Desc.Format = DXGI_FORMAT_UNKNOWN;
		d3d12Desc.SampleDesc.Count = 1;
		d3d12Desc.SampleDesc.Quality = 0;
		d3d12Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		d3d12Desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		// TODO: D3D12MA
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_READBACK;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		HRESULT hr = m_D3d12Device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &d3d12Desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_TimerQueryResolveBuffer));
		assert(SUCCEEDED(hr));
		m_TimerQueryResolveBuffer->SetName(L"TimerQuery Resolve Buffer");
	}

	// Used for WaitForIdle only. Probably unneeded
	m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);

	// Bindless RS
	CreateBindlessRootSignature();

	// Stale resource vectors
	m_StaleResources.resize(m_Desc.swapChainFrames);

	m_LastStats = {};
	m_CurrentStats = {};
};

st::rhi::dx12::GpuDevice::~GpuDevice()
{
	WaitForIdle();

	if (m_FenceEvent)
	{
		CloseHandle(m_FenceEvent);
		m_FenceEvent = nullptr;
	}
}

st::rhi::ShaderOwner st::rhi::dx12::GpuDevice::CreateShader(const ShaderDesc& desc, const WeakBlob& bytecode, const std::string& debugName)
{
	return InsertNewResource<IShader>(new Shader{desc, bytecode, this, debugName});
}

st::rhi::BufferOwner st::rhi::dx12::GpuDevice::CreateBuffer(const BufferDesc& desc, ResourceState initialState, const std::string& debugName)
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
	d3d12Desc.Flags = (fixedDesc.shaderUsage & BufferShaderUsage::ReadWrite) == 0 ?
		D3D12_RESOURCE_FLAG_NONE : D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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

	auto ws_nmame = ToWide(debugName.c_str());
	d3d12Buffer->SetName(ws_nmame.c_str());

	return InsertNewResource<IBuffer>(new Buffer{ fixedDesc, d3d12Buffer.Get(), this, debugName });
}

st::rhi::TextureOwner st::rhi::dx12::GpuDevice::CreateTexture(const TextureDesc& desc, ResourceState initialState, const std::string& debugName)
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

	auto ws_nmame = ToWide(debugName.c_str());
	d3d12Texture->SetName(ws_nmame.c_str());

	return InsertNewResource<ITexture>(new Texture{ desc, d3d12Texture, this, debugName });
}

st::rhi::TextureOwner st::rhi::dx12::GpuDevice::CreateHandleForNativeTexture(void* obj, const TextureDesc& desc, const std::string& debugName)
{
	if (obj == nullptr)
	{
		return nullptr;
	}

	ID3D12Resource* d3d12Resource = static_cast<ID3D12Resource*>(obj);

	auto ws_nmame = ToWide(debugName.c_str());
	d3d12Resource->SetName(ws_nmame.c_str());

	return InsertNewResource<ITexture>(new Texture{ desc, d3d12Resource, this, debugName });
}

st::rhi::FramebufferOwner st::rhi::dx12::GpuDevice::CreateFramebuffer(const FramebufferDesc& desc, const std::string& debugName)
{
	auto* fb = new Framebuffer{ this, debugName };
	fb->desc = desc;
	fb->framebufferInfo = FramebufferInfo{ desc };

	if (!desc.ColorAttachments.empty())
	{
		Texture* texture = checked_cast<Texture*>(desc.ColorAttachments[0].texture.get());
		fb->rtWidth = texture->GetDesc().width;
		fb->rtHeight = texture->GetDesc().height;
	}
	else if (desc.DepthAttachment.Valid())
	{
		Texture* texture = checked_cast<Texture*>(desc.DepthAttachment.texture.get());
		fb->rtWidth = texture->GetDesc().width;
		fb->rtHeight = texture->GetDesc().height;
	}

	for (size_t rt = 0; rt < desc.ColorAttachments.size(); rt++)
	{
		auto& attachment = desc.ColorAttachments[rt];

		Texture* texture = checked_cast<Texture*>(attachment.texture.get());
		assert(texture->GetDesc().width == fb->rtWidth);
		assert(texture->GetDesc().height == fb->rtHeight);

		DescriptorIndex index = m_RenderTargetViewHeap.AllocateDescriptor();

		const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_RenderTargetViewHeap.GetCpuHandle(index);
		CreateTextureRTV(texture, descriptorHandle, attachment.format, attachment.subresources);

		fb->RTVs.push_back(index);

		st::rhi::TextureHandle handle = st::checked_pointer_cast<ITexture>(texture->weak_from_this());
		fb->rtvTextures.push_back(handle);
	}

	if (desc.DepthAttachment.Valid())
	{
		Texture* texture = checked_cast<Texture*>(desc.DepthAttachment.texture.get());
		assert(texture->GetDesc().width == fb->rtWidth);
		assert(texture->GetDesc().height == fb->rtHeight);

		DescriptorIndex index = m_DepthStencilViewHeap.AllocateDescriptor();

		const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_DepthStencilViewHeap.GetCpuHandle(index);
		CreateTextureDSV(texture, descriptorHandle, desc.DepthAttachment.subresources, desc.DepthAttachment.isReadOnly);

		fb->DSV = index;

		st::rhi::TextureHandle handle = st::checked_pointer_cast<ITexture>(texture->weak_from_this());
		fb->dsvTexture = handle;
	}

	return InsertNewResource<IFramebuffer>(fb);
}

st::rhi::CommandListOwner st::rhi::dx12::GpuDevice::CreateCommandList(const CommandListParams& params, const std::string& debugName)
{
	if (!m_Queues[(int)params.queueType].d3d12Queue)
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

	auto ws_nmame = ToWide(debugName.c_str());
	d3d12CommandList->SetName(ws_nmame.c_str());

	d3d12CommandList->Close(); // Start closed

	return InsertNewResource<ICommandList>(new CommandList{ d3d12CommandList.Get(), d3d12CommandAllocator.Get(), params.queueType, this, debugName });
}

st::rhi::GraphicsPipelineStateOwner st::rhi::dx12::GpuDevice::CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, 
	const FramebufferInfo& fbInfo, const std::string& debugName)
{
	// TODO: cache

	// Create regular PSO
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

	auto ws_nmame = ToWide(debugName.c_str());
	d3d12PSO->SetName(ws_nmame.c_str());

	return InsertNewResource<IGraphicsPipelineState>(new GraphicsPipelineState{ d3d12PSO, d3d12Desc, desc, this, debugName });
}

st::rhi::ComputePipelineStateOwner st::rhi::dx12::GpuDevice::CreateComputePipelineState(const ComputePipelineStateDesc& desc, const std::string& debugName)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12Desc = {};
	d3d12Desc.pRootSignature = m_BindlessRootSignature.Get();
	d3d12Desc.CS = desc.CS ? D3D12_SHADER_BYTECODE{ desc.CS->GetBytecode().data(), desc.CS->GetBytecode().size() } : D3D12_SHADER_BYTECODE{};

	ComPtr<ID3D12PipelineState> d3d12PSO;
	HRESULT hr = m_D3d12Device->CreateComputePipelineState(&d3d12Desc, IID_PPV_ARGS(&d3d12PSO));
	HR_RETURN_NULL(hr);

	auto ws_nmame = ToWide(debugName.c_str());
	d3d12PSO->SetName(ws_nmame.c_str());

	return InsertNewResource<IComputePipelineState>(new ComputePipelineState{ d3d12PSO, d3d12Desc, this, debugName });
}

st::rhi::FenceOwner st::rhi::dx12::GpuDevice::CreateFence(uint64_t initialVale, const std::string& debugName)
{
	ComPtr<ID3D12Fence> d3d12Fence;
	HRESULT hr = m_D3d12Device->CreateFence(initialVale, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence));
	HR_RETURN_NULL(hr);

	auto ws_nmame = ToWide(debugName.c_str());
	d3d12Fence->SetName(ws_nmame.c_str());

	return InsertNewResource<IFence>(new Fence{ d3d12Fence.Get(), this, debugName });
}

st::rhi::TimerQueryOwner st::rhi::dx12::GpuDevice::CreateTimerQuery(const std::string& debugName)
{
	int queryIndex = m_TimerQueries.Allocate();
	if (queryIndex < 0)
	{
		LOG_ERROR("Not enough space in Timer Query Heap");
		return nullptr;
	}

	return InsertNewResource<ITimerQuery>(new TimerQuery{ (uint32_t)queryIndex * 2, ((uint32_t)queryIndex * 2) + 1, this, debugName });
}

st::rhi::StorageRequirements st::rhi::dx12::GpuDevice::GetStorageRequirements(const BufferDesc& desc)
{
	StorageRequirements ret = { 
		.size = desc.sizeBytes, 
		.alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT };

	if (has_any_flag(desc.shaderUsage, BufferShaderUsage::Uniform))
	{
		ret.size = AlignUp(desc.sizeBytes, (size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	}

	return ret;
}

st::rhi::StorageRequirements st::rhi::dx12::GpuDevice::GetStorageRequirements(const TextureDesc& desc)
{
	D3D12_RESOURCE_DESC d3d12Desc = BuildD3d12Desc(desc);	
	D3D12_RESOURCE_ALLOCATION_INFO allocInfo = m_D3d12Device->GetResourceAllocationInfo(0, 1, &d3d12Desc);	
	return StorageRequirements{ .size = allocInfo.SizeInBytes, .alignment = allocInfo.Alignment };
}

st::rhi::StorageRequirements st::rhi::dx12::GpuDevice::GetCopyableRequirements(const BufferDesc& desc)
{
	return StorageRequirements{ .size = desc.sizeBytes, .alignment = GetCopyDataAlignment(CopyMethod::Buffer2Buffer) };
}

st::rhi::StorageRequirements st::rhi::dx12::GpuDevice::GetCopyableRequirements(const TextureDesc& desc, const rhi::TextureSubresourceSet& subresources)
{
	st::rhi::StorageRequirements ret = {};
	D3D12_RESOURCE_DESC d3d12Desc = BuildD3d12Desc(desc);

	uint32_t firstSubresource = D3D12CalcSubresource(
		subresources.baseMipLevel,
		subresources.baseArraySlice,
		0, // PlaneSlice
		desc.mipLevels,
		desc.arraySize
	);

	uint32_t numSubresources = subresources.GetNumMipLevels(desc) * subresources.GetNumArraySlices(desc);

	m_D3d12Device->GetCopyableFootprints(&d3d12Desc, firstSubresource, numSubresources, 0, nullptr, nullptr, nullptr, &ret.size);
	ret.alignment = GetCopyDataAlignment(CopyMethod::Buffer2Texture);

	return ret;
}

st::rhi::SubresourceCopyableRequirements st::rhi::dx12::GpuDevice::GetSubresourceCopyableRequirements(const TextureDesc& desc, uint32_t mipLevel, uint32_t arraySlice)
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

size_t st::rhi::dx12::GpuDevice::GetCopyDataAlignment(CopyMethod method)
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

st::rhi::TextureSampledView st::rhi::dx12::GpuDevice::CreateTextureSampledView(ITexture* texture, const TextureSubresourceSet& subresources,
	Format format, TextureDimension dimension)
{
	const auto& desc = texture->GetDesc();

	if (!has_any_flag(desc.shaderUsage, TextureShaderUsage::Sampled))
	{
		LOG_ERROR("Can't create SRV: Texture not create with TextureShaderUsage::ShaderResource");
		return {};
	}
	st::rhi::DescriptorIndex di = m_ShaderResourceViewHeap.AllocateDescriptor();
	assert(di != c_InvalidDescriptorIndex);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_ShaderResourceViewHeap.GetCpuHandle(di);
	CreateTextureSRV(texture, descriptorHandle, format, subresources, dimension);

	return TextureSampledView{ di };
}

st::rhi::TextureStorageView st::rhi::dx12::GpuDevice::CreateTextureStorageView(ITexture* texture, const TextureSubresourceSet& subresources,
	Format format, TextureDimension dimension)
{
	const auto& desc = texture->GetDesc();

	if (!has_any_flag(desc.shaderUsage, TextureShaderUsage::Storage))
	{
		LOG_ERROR("Can't create UAV: Texture not create with TextureShaderUsage::UnorderedAccess");
		return {};
	}
	st::rhi::DescriptorIndex di = m_ShaderResourceViewHeap.AllocateDescriptor();
	assert(di != c_InvalidDescriptorIndex);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_ShaderResourceViewHeap.GetCpuHandle(di);
	CreateTextureUAV(texture, descriptorHandle, format, subresources, dimension);

	return TextureStorageView{ di };
}

st::rhi::TextureColorTargetView st::rhi::dx12::GpuDevice::CreateTextureColorTargetView(ITexture* texture, Format format, TextureSubresourceSet subresources)
{
	const auto& desc = texture->GetDesc();

	if (!has_any_flag(desc.shaderUsage, TextureShaderUsage::ColorTarget))
	{
		LOG_ERROR("Can't create UAV: Texture not create with TextureShaderUsage::UnorderedAccess");
		return {};
	}
	st::rhi::DescriptorIndex di = m_RenderTargetViewHeap.AllocateDescriptor();
	assert(di != c_InvalidDescriptorIndex);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_RenderTargetViewHeap.GetCpuHandle(di);
	CreateTextureRTV(texture, descriptorHandle, format, subresources);

	return TextureColorTargetView{ di };
}

st::rhi::TextureDepthTargetView st::rhi::dx12::GpuDevice::CreateTextureDepthTargetView(ITexture* texture, TextureSubresourceSet subresources, bool isReadOnly)
{
	const auto& desc = texture->GetDesc();

	if (!has_any_flag(desc.shaderUsage, TextureShaderUsage::DepthTarget))
	{
		LOG_ERROR("Can't create UAV: Texture not create with TextureShaderUsage::UnorderedAccess");
		return {};
	}
	st::rhi::DescriptorIndex di = m_DepthStencilViewHeap.AllocateDescriptor();
	assert(di != c_InvalidDescriptorIndex);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_DepthStencilViewHeap.GetCpuHandle(di);
	CreateTextureDSV(texture, descriptorHandle, subresources, isReadOnly);

	return TextureDepthTargetView{ di };
}

void st::rhi::dx12::GpuDevice::ReleaseTextureSampledView(TextureSampledView& v, bool immediate)
{
	if (v.IsValid())
	{
		if (immediate)
		{
			m_ShaderResourceViewHeap.ReleaseDescriptor(v.GetIdx());
		}
		else
		{
			std::scoped_lock lock{ m_StaleDescriptorsMutex };
			m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Descriptors.emplace_back(
				v.GetIdx(), &m_ShaderResourceViewHeap);
		}
		v.Invalidate();
	}
}

void st::rhi::dx12::GpuDevice::ReleaseTextureStorageView(TextureStorageView& v, bool immediate)
{
	if (v.IsValid())
	{
		if (immediate)
		{
			m_ShaderResourceViewHeap.ReleaseDescriptor(v.GetIdx());
		}
		else
		{
			std::scoped_lock lock{ m_StaleDescriptorsMutex };
			m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Descriptors.emplace_back(
				v.GetIdx(), &m_ShaderResourceViewHeap);
		}
		v.Invalidate();
	}
}

void st::rhi::dx12::GpuDevice::ReleaseTextureColorTargetView(TextureColorTargetView& v, bool immediate)
{
	if (v.IsValid())
	{
		if (immediate)
		{
			m_RenderTargetViewHeap.ReleaseDescriptor(v.GetIdx());
		}
		else
		{
			std::scoped_lock lock{ m_StaleDescriptorsMutex };
			m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Descriptors.emplace_back(
				v.GetIdx(), &m_RenderTargetViewHeap);
		}
		v.Invalidate();
	}
}

void st::rhi::dx12::GpuDevice::ReleaseTextureDepthTargetView(TextureDepthTargetView& v, bool immediate)
{
	if (v.IsValid())
	{
		if (immediate)
		{
			m_DepthStencilViewHeap.ReleaseDescriptor(v.GetIdx());
		}
		else
		{
			std::scoped_lock lock{ m_StaleDescriptorsMutex };
			m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Descriptors.emplace_back(
				v.GetIdx(), &m_DepthStencilViewHeap);
		}
		v.Invalidate();
	}
}

st::rhi::BufferUniformView st::rhi::dx12::GpuDevice::CreateBufferUniformView(IBuffer* buffer, uint32_t start, int size)
{
	const auto& desc = buffer->GetDesc();
	if (!has_any_flag(desc.shaderUsage, BufferShaderUsage::Uniform))
	{
		LOG_ERROR("Can't create CBV: Buffer not created with BufferShaderUsage::Uniform");
		return {};
	}

	DescriptorIndex di = m_ShaderResourceViewHeap.AllocateDescriptor();
	const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_ShaderResourceViewHeap.GetCpuHandle(di);
	CreateBufferCBV(buffer, descriptorHandle, start, size < 0 ? (uint32_t)buffer->GetDesc().sizeBytes : (uint32_t)size);

	return BufferUniformView{ di };
}

st::rhi::BufferReadOnlyView st::rhi::dx12::GpuDevice::CreateBufferReadOnlyView(IBuffer* buffer, uint32_t start, int size)
{
	const auto& desc = buffer->GetDesc();
	if (!has_any_flag(desc.shaderUsage, BufferShaderUsage::ReadOnly))
	{
		LOG_ERROR("Can't create CBV: Buffer not created with BufferShaderUsage::ReadOnly");
		return {};
	}

	DescriptorIndex di = m_ShaderResourceViewHeap.AllocateDescriptor();
	const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_ShaderResourceViewHeap.GetCpuHandle(di);
	CreateBufferSRV(buffer, descriptorHandle, start, size < 0 ? (uint32_t)buffer->GetDesc().sizeBytes : (uint32_t)size);

	return BufferReadOnlyView{ di };
}

st::rhi::BufferReadWriteView st::rhi::dx12::GpuDevice::CreateBufferReadWriteView(IBuffer* buffer, uint32_t start, int size)
{
	const auto& desc = buffer->GetDesc();
	if (!has_any_flag(desc.shaderUsage, BufferShaderUsage::ReadWrite))
	{
		LOG_ERROR("Can't create CBV: Buffer not created with BufferShaderUsage::ReadOnly");
		return {};
	}

	DescriptorIndex di = m_ShaderResourceViewHeap.AllocateDescriptor();
	const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_ShaderResourceViewHeap.GetCpuHandle(di);
	CreateBufferUAV(buffer, descriptorHandle, start, size < 0 ? (uint32_t)buffer->GetDesc().sizeBytes : (uint32_t)size);

	return BufferReadWriteView{ di };
}

void st::rhi::dx12::GpuDevice::ReleaseBufferUniformView(BufferUniformView& v, bool immediate)
{
	if (v.IsValid())
	{
		if (immediate)
		{
			m_ShaderResourceViewHeap.ReleaseDescriptor(v.GetIdx());
		}
		else
		{
			std::scoped_lock lock{ m_StaleDescriptorsMutex };
			m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Descriptors.emplace_back(
				v.GetIdx(), &m_ShaderResourceViewHeap);
		}
		v.Invalidate();
	}
}

void st::rhi::dx12::GpuDevice::ReleaseBufferReadOnlyView(BufferReadOnlyView& v, bool immediate)
{
	if (v.IsValid())
	{
		if (immediate)
		{
			m_ShaderResourceViewHeap.ReleaseDescriptor(v.GetIdx());
		}
		else
		{
			std::scoped_lock lock{ m_StaleDescriptorsMutex };
			m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Descriptors.emplace_back(
				v.GetIdx(), &m_ShaderResourceViewHeap);
		}
		v.Invalidate();
	}
}

void st::rhi::dx12::GpuDevice::ReleaseBufferReadWriteView(BufferReadWriteView& v, bool immediate)
{
	if (v.IsValid())
	{
		if (immediate)
		{
			m_ShaderResourceViewHeap.ReleaseDescriptor(v.GetIdx());
		}
		else
		{
			std::scoped_lock lock{ m_StaleDescriptorsMutex };
			m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Descriptors.emplace_back(
				v.GetIdx(), &m_ShaderResourceViewHeap);
		}
		v.Invalidate();
	}
}

void st::rhi::dx12::GpuDevice::ExecuteCommandLists(std::span<ICommandList* const> commandLists, QueueType type, IFence* signal, uint64_t value)
{
	auto& queue = m_Queues[(int)type];
	if (!queue.d3d12Queue)
	{
		LOG_ERROR("Queue type '%d' not initialized", (int)type);
		return;
	}

	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(commandLists.size());
	for (auto cl : commandLists)
	{
		d3d12CommandLists.push_back(cl->GetNativeResource());
	}

	queue.d3d12Queue->ExecuteCommandLists(d3d12CommandLists.size(), d3d12CommandLists.data());

	if (signal)
	{
		queue.d3d12Queue->Signal(signal->GetNativeResource(), value);
	}

	queue.d3d12Queue->Signal(queue.d3d12Fence.Get(), ++queue.lastSubmittedInstance);

	for (auto cl : commandLists)
	{
		auto* dx12cl = st::checked_cast<dx12::CommandList*>(cl);
		// Gather stats
		m_CurrentStats.DrawCalls += dx12cl->m_DrawCalls;
		m_CurrentStats.PrimitiveCount += dx12cl->m_PrimitiveCount;

		dx12cl->OnExecuted(m_Queues[(int)type]);
	}
}

void st::rhi::dx12::GpuDevice::ExecuteCommandList(ICommandList* commandList, QueueType type, IFence* signal, uint64_t value)
{
	ExecuteCommandLists(std::span<ICommandList*, 1>{&commandList, 1}, type, signal, value);
}

void st::rhi::dx12::GpuDevice::NextFrame()
{
	ReleaseStaleResources((m_CurrentFrameIdx + 1) % m_Desc.swapChainFrames);

	m_LastStats = m_CurrentStats;
	m_CurrentStats = {};

	++m_CurrentFrameIdx;
}

void st::rhi::dx12::GpuDevice::Shutdown()
{
	for(int i = 0; i < m_StaleResources.size(); ++i)
	{
		ReleaseStaleResources(i);
	}

	if (!m_LivingResources.empty())
	{
		for (auto& res : m_LivingResources)
		{
			LOG_ERROR("Resource addr [0x{:08x}] of type {}, debug name '{}' not released!", 
				(uintptr_t)res, ResourceTypeToString(res->GetResourceType()), res->GetDebugName());
			ReleaseResource(res);
		}
		m_LivingResources.clear();
	}
}

void st::rhi::dx12::GpuDevice::ReleaseImmediatelyInternal(IResource* resource)
{
	ReleaseResource(resource);

	std::scoped_lock lock{ m_LivingResourcesMutex };
	m_LivingResources.erase(resource);
}

void st::rhi::dx12::GpuDevice::ReleaseQueuedInternal(IResource* resource)
{
	if (!resource) 
		return;

	std::scoped_lock lock{ m_StaleResourcesMutex };
	m_StaleResources[m_CurrentFrameIdx % m_Desc.swapChainFrames].Resources.push_back(resource);
}

void st::rhi::dx12::GpuDevice::WaitForIdle()
{
	// Wait for every queue to reach its last submitted instance
	for (auto& queue : m_Queues)
	{
		if (!queue.d3d12Queue)
			continue;

		if (queue.GetLastCompletedInstance() < queue.lastSubmittedInstance)
		{
			WaitForFence(queue.d3d12Fence.Get(), queue.lastSubmittedInstance, m_FenceEvent);
		}
	}
}

void st::rhi::dx12::GpuDevice::CreateBindlessRootSignature()
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

D3D12_RESOURCE_DESC st::rhi::dx12::GpuDevice::BuildD3d12Desc(const TextureDesc& desc)
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
	d3d12Desc.Format = formatMap.srvFormat;
	d3d12Desc.SampleDesc.Count = desc.sampleCount;
	d3d12Desc.SampleDesc.Quality = desc.sampleQuality;
	if (!has_any_flag(desc.shaderUsage, TextureShaderUsage::Sampled))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	if (has_any_flag(desc.shaderUsage, TextureShaderUsage::Storage))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (has_any_flag(desc.shaderUsage, TextureShaderUsage::ColorTarget))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (has_any_flag(desc.shaderUsage, TextureShaderUsage::DepthTarget))
		d3d12Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (desc.isTiled)
		d3d12Desc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;

	return d3d12Desc;
}

void st::rhi::dx12::GpuDevice::CreateTextureSRV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources,
	TextureDimension dimension)
{
	const auto& desc = texture->GetDesc();
	subresources.Resolve(desc);

	if (dimension == TextureDimension::Unknown)
		dimension = desc.dimension;

	D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};

	viewDesc.Format = GetDxgiFormatMapping(format == Format::UNKNOWN ? desc.format : format).srvFormat;
	viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	uint32_t planeSlice = (viewDesc.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT) ? 1 : 0;

	switch (dimension)
	{
	case TextureDimension::Texture1D:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		viewDesc.Texture1D.MostDetailedMip = subresources.baseMipLevel;
		viewDesc.Texture1D.MipLevels = subresources.numMipLevels;
		break;
	case TextureDimension::Texture1DArray:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture1DArray.MostDetailedMip = subresources.baseMipLevel;
		viewDesc.Texture1DArray.MipLevels = subresources.numMipLevels;
		break;
	case TextureDimension::Texture2D:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MostDetailedMip = subresources.baseMipLevel;
		viewDesc.Texture2D.MipLevels = subresources.numMipLevels;
		viewDesc.Texture2D.PlaneSlice = planeSlice;
		break;
	case TextureDimension::Texture2DArray:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture2DArray.MostDetailedMip = subresources.baseMipLevel;
		viewDesc.Texture2DArray.MipLevels = subresources.numMipLevels;
		viewDesc.Texture2DArray.PlaneSlice = planeSlice;
		break;
	case TextureDimension::TextureCube:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		viewDesc.TextureCube.MostDetailedMip = subresources.baseMipLevel;
		viewDesc.TextureCube.MipLevels = subresources.numMipLevels;
		break;
	case TextureDimension::TextureCubeArray:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		viewDesc.TextureCubeArray.First2DArrayFace = subresources.baseArraySlice;
		viewDesc.TextureCubeArray.NumCubes = subresources.numArraySlices / 6;
		viewDesc.TextureCubeArray.MostDetailedMip = subresources.baseMipLevel;
		viewDesc.TextureCubeArray.MipLevels = subresources.numMipLevels;
		break;
	case TextureDimension::Texture2DMS:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		break;
	case TextureDimension::Texture2DMSArray:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
		viewDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
		break;
	case TextureDimension::Texture3D:
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		viewDesc.Texture3D.MostDetailedMip = subresources.baseMipLevel;
		viewDesc.Texture3D.MipLevels = subresources.numMipLevels;
		break;
	case TextureDimension::Unknown:
	default:
		assert(0);
		return;
	}

	m_D3d12Device->CreateShaderResourceView(texture->GetNativeResource(), &viewDesc, descriptor);
}

void st::rhi::dx12::GpuDevice::CreateTextureUAV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources,
	TextureDimension dimension)
{
	const auto& desc = texture->GetDesc();
	subresources.Resolve(desc);

	if (dimension == TextureDimension::Unknown)
		dimension = desc.dimension;

	D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};

	viewDesc.Format = GetDxgiFormatMapping(format == Format::UNKNOWN ? desc.format : format).uavFormat;

	switch (dimension)
	{
	case TextureDimension::Texture1D:
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
		viewDesc.Texture1D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture1DArray:
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2D:
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2DArray:
	case TextureDimension::TextureCube:
	case TextureDimension::TextureCubeArray:
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture3D:
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		viewDesc.Texture3D.FirstWSlice = 0;
		viewDesc.Texture3D.WSize = desc.depth;
		viewDesc.Texture3D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2DMS:
	case TextureDimension::Texture2DMSArray: {
		assert(0);
		return;
	}
	case TextureDimension::Unknown:
	default:
		assert(0);
		return;
	}

	m_D3d12Device->CreateUnorderedAccessView(texture->GetNativeResource(), nullptr, &viewDesc, descriptor);
}

void st::rhi::dx12::GpuDevice::CreateTextureRTV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources)
{
	const auto& desc = texture->GetDesc();
	subresources.Resolve(desc);

	D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};

	viewDesc.Format = GetDxgiFormatMapping(format == Format::UNKNOWN ? desc.format : format).rtvFormat;

	switch (desc.dimension)
	{
	case TextureDimension::Texture1D:
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
		viewDesc.Texture1D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture1DArray:
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2D:
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2DArray:
	case TextureDimension::TextureCube:
	case TextureDimension::TextureCubeArray:
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2DMS:
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		break;
	case TextureDimension::Texture2DMSArray:
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
		viewDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
		break;
	case TextureDimension::Texture3D:
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		viewDesc.Texture3D.FirstWSlice = subresources.baseArraySlice;
		viewDesc.Texture3D.WSize = subresources.numArraySlices;
		viewDesc.Texture3D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Unknown:
	default:
		assert(0);
		return;
	}

	m_D3d12Device->CreateRenderTargetView(texture->GetNativeResource(), &viewDesc, descriptor);
}

void st::rhi::dx12::GpuDevice::CreateTextureDSV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, TextureSubresourceSet subresources, bool isReadOnly)
{
	const auto& desc = texture->GetDesc();
	subresources.Resolve(desc);

	D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};

	viewDesc.Format = GetDxgiFormatMapping(desc.format).rtvFormat;

	if (isReadOnly)
	{
		viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
		if (viewDesc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || viewDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
			viewDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
	}

	switch (desc.dimension)
	{
	case TextureDimension::Texture1D:
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
		viewDesc.Texture1D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture1DArray:
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		viewDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2D:
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2DArray:
	case TextureDimension::TextureCube:
	case TextureDimension::TextureCubeArray:
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
		viewDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
		break;
	case TextureDimension::Texture2DMS:
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		break;
	case TextureDimension::Texture2DMSArray:
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		viewDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
		viewDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
		break;
	case TextureDimension::Texture3D: {
		assert(0);
		return;
	}
	case TextureDimension::Unknown:
	default:
		assert(0);
		return;
	}

	m_D3d12Device->CreateDepthStencilView(texture->GetNativeResource(), &viewDesc, descriptor);
}

void st::rhi::dx12::GpuDevice::CreateBufferCBV(IBuffer* buffer, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, uint32_t sizeBytes)
{
	assert(IsAligned(offsetBytes, (uint32_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
	assert(IsAligned(sizeBytes, (uint32_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

	ID3D12Resource* nativeRes = buffer->GetNativeResource();

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = nativeRes->GetGPUVirtualAddress() + offsetBytes;
	desc.SizeInBytes = sizeBytes;

	m_D3d12Device->CreateConstantBufferView(&desc, descriptor);
}

void st::rhi::dx12::GpuDevice::CreateBufferSRV(IBuffer* buffer, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, uint32_t sizeBytes)
{
	const auto& desc = buffer->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
	viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	// Structured buffer
	if (desc.format == Format::UNKNOWN)
	{
		if (desc.stride != 0)
		{
			assert(IsAligned(offsetBytes, desc.stride));
			viewDesc.Format = DXGI_FORMAT_UNKNOWN;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			viewDesc.Buffer.FirstElement = offsetBytes / desc.stride;
			viewDesc.Buffer.NumElements = std::min(sizeBytes, (uint32_t)desc.sizeBytes) / desc.stride;
			viewDesc.Buffer.StructureByteStride = desc.stride;
		}
		else
		{
			viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			viewDesc.Buffer.FirstElement = offsetBytes / sizeof(uint32_t);
			viewDesc.Buffer.NumElements = std::min(sizeBytes, (uint32_t)desc.sizeBytes) / sizeof(uint32_t);
			viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		}
	}
	// Typed buffer
	else
	{
		uint32_t stride = GetFormatInfo(desc.format).bytesPerBlock;
		viewDesc.Format = GetDxgiFormatMapping(desc.format).srvFormat;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		viewDesc.Buffer.FirstElement = offsetBytes / stride;
		viewDesc.Buffer.NumElements = std::min(sizeBytes, (uint32_t)desc.sizeBytes) / stride;
	}

	m_D3d12Device->CreateShaderResourceView(buffer->GetNativeResource(), &viewDesc, descriptor);
}

void st::rhi::dx12::GpuDevice::CreateBufferUAV(IBuffer* buffer, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, uint32_t sizeBytes)
{
	const auto& desc = buffer->GetDesc();

	D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
	viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

	if (desc.format == Format::UNKNOWN)
	{
		if (desc.stride != 0) // Structured buffer
		{
			assert(IsAligned(offsetBytes, desc.stride));
			viewDesc.Format = DXGI_FORMAT_UNKNOWN;
			viewDesc.Buffer.FirstElement = offsetBytes / desc.stride;
			viewDesc.Buffer.NumElements = std::min(sizeBytes, (uint32_t)desc.sizeBytes) / desc.stride;
			viewDesc.Buffer.StructureByteStride = desc.stride;
		}
		else
		{
			viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			viewDesc.Buffer.FirstElement = offsetBytes / sizeof(uint32_t);
			viewDesc.Buffer.NumElements = std::min(sizeBytes, (uint32_t)desc.sizeBytes) / sizeof(uint32_t);
			viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		}
	}
	else
	{
		uint32_t stride = GetFormatInfo(desc.format).bytesPerBlock;
		viewDesc.Format = GetDxgiFormatMapping(desc.format).srvFormat;
		viewDesc.Buffer.FirstElement = offsetBytes / stride;
		viewDesc.Buffer.NumElements = std::min(sizeBytes, (uint32_t)desc.sizeBytes) / stride;
	}


	m_D3d12Device->CreateUnorderedAccessView(buffer->GetNativeResource(), nullptr, &viewDesc, descriptor);
}

void st::rhi::dx12::GpuDevice::ReleaseStaleResources(uint32_t bufferIdx)
{
	{
		std::scoped_lock lock{ m_LivingResourcesMutex };
		for (IResource* pres : m_StaleResources[bufferIdx].Resources)
		{
			ReleaseResource(pres);
			m_LivingResources.erase(pres);
		}
	}
	m_StaleResources[bufferIdx].Resources.clear();

	for (const auto& desc : m_StaleResources[bufferIdx].Descriptors)
	{
		desc.second->ReleaseDescriptor(desc.first);
	}
	m_StaleResources[bufferIdx].Descriptors.clear();
}