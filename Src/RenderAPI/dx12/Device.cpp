#include "RenderAPI/dx12/Device.h"
#include <wrl/client.h>
#include <array>
#include "RenderAPI/dx12/DescriptorHeap.h"
#include "RenderAPI/dx12/Texture.h"
#include "RenderAPI/dx12/Framebuffer.h"
#include "RenderAPI/dx12/CommandList.h"
#include "Core/Util.h"

using namespace Microsoft::WRL;

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
} // anonymous namespace

namespace st::rapi::dx12
{
	struct Queue
	{
		ComPtr<ID3D12CommandQueue> queue;
		ComPtr<ID3D12Fence> fence;
		uint64_t lastSubmittedInstance = 0;
		uint64_t lastCompletedInstance = 0;

		uint64_t UpdateLastCompletedInstance()
		{
			if (lastCompletedInstance < lastSubmittedInstance)
			{
				lastCompletedInstance = fence->GetCompletedValue();
			}
			return lastCompletedInstance;
		}
	};

	class GpuDevice : public st::rapi::Device
	{
	public:

		GpuDevice(const DeviceDesc& desc);
		~GpuDevice();

		TextureHandle CreateHandleForNativeTexture(void* obj, const TextureDesc& desc) override;
		
		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) override;

		CommandListHandle CreateCommandList(const CommandListParams& params) override;

		void WaitForIdle() override;

	private:

		std::array<Queue, (int)QueueType::_Count> m_Queues;

		DescriptorHeap m_DepthStencilViewHeap;
		DescriptorHeap m_RenderTargetViewHeap;
		DescriptorHeap m_ShaderResourceViewHeap;
		DescriptorHeap m_SamplerHeap;

		D3D12_FEATURE_DATA_D3D12_OPTIONS  m_Options = {};
		D3D12_FEATURE_DATA_D3D12_OPTIONS1 m_Options1 = {};
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 m_Options5 = {};
		D3D12_FEATURE_DATA_D3D12_OPTIONS6 m_Options6 = {};
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 m_Options7 = {};

		bool m_MeshletsSupported = false;
		bool m_RayTracingSupported = false;
		bool m_TraceRayInlineSupported = false;
		bool m_SamplerFeedbackSupported = false;
		bool m_VariableRateShadingSupported = false;
		bool m_HeapDirectlyIndexedSupported = false;

		HANDLE m_FenceEvent;

		ComPtr<ID3D12Device> m_D3d12Device;
		ComPtr<ID3D12Device2> m_D3d12Device2;
		ComPtr<ID3D12Device5> m_D3d12Device5;
		ComPtr<ID3D12Device8> m_D3d12Device8;
	};

} // namespace st::rapi::dx12

std::unique_ptr<st::rapi::Device> st::rapi::dx12::CreateDevice(const st::rapi::dx12::DeviceDesc& desc)
{
	return std::make_unique<st::rapi::dx12::GpuDevice>(desc);
}

st::rapi::dx12::GpuDevice::GpuDevice(const st::rapi::dx12::DeviceDesc& desc) :
	m_DepthStencilViewHeap{ desc.pDevice },
	m_RenderTargetViewHeap{ desc.pDevice },
	m_ShaderResourceViewHeap{ desc.pDevice },
	m_SamplerHeap{ desc.pDevice }
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
	m_ShaderResourceViewHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, desc.shaderResourceViewHeapSize, true);
	m_SamplerHeap.AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, desc.samplerHeapSize, true);

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

st::rapi::TextureHandle st::rapi::dx12::GpuDevice::CreateHandleForNativeTexture(void* obj, const TextureDesc& desc)
{
	if (obj == nullptr)
	{
		return nullptr;
	}

	Texture* tex = new Texture{ desc, static_cast<ID3D12Resource*>(obj), m_D3d12Device.Get() };
	return TextureHandle{ tex };
}

st::rapi::FramebufferHandle st::rapi::dx12::GpuDevice::CreateFramebuffer(const FramebufferDesc& desc)
{
	auto* fb = new Framebuffer;
	fb->desc = desc;
	fb->framebufferInfo = FramebufferInfo{ desc };

	if (!desc.ColorAttachments.empty())
	{
		Texture* texture = checked_cast<Texture*>(desc.ColorAttachments[0].texture);
		fb->rtWidth = texture->m_Desc.width;
		fb->rtHeight = texture->m_Desc.height;
	}
	else if (desc.DepthAttachment.Valid())
	{
		Texture* texture = checked_cast<Texture*>(desc.DepthAttachment.texture);
		fb->rtWidth = texture->m_Desc.width;
		fb->rtHeight = texture->m_Desc.height;
	}

	for (size_t rt = 0; rt < desc.ColorAttachments.size(); rt++)
	{
		auto& attachment = desc.ColorAttachments[rt];

		Texture* texture = checked_cast<Texture*>(attachment.texture);
		assert(texture->m_Desc.width == fb->rtWidth);
		assert(texture->m_Desc.height == fb->rtHeight);

		DescriptorIndex index = m_RenderTargetViewHeap.AllocateDescriptor();

		const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_RenderTargetViewHeap.GetCpuHandle(index);
		texture->CreateRTV(descriptorHandle, attachment.format, attachment.subresources);

		fb->RTVs.push_back(index);

		st::rapi::TextureHandle handle = std::static_pointer_cast<ITexture>(texture->shared_from_this());
		fb->textures.push_back(handle);
	}

	if (desc.DepthAttachment.Valid())
	{
		Texture* texture = checked_cast<Texture*>(desc.DepthAttachment.texture);
		assert(texture->m_Desc.width == fb->rtWidth);
		assert(texture->m_Desc.height == fb->rtHeight);

		DescriptorIndex index = m_DepthStencilViewHeap.AllocateDescriptor();

		const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_DepthStencilViewHeap.GetCpuHandle(index);
		texture->CreateDSV(descriptorHandle, desc.DepthAttachment.subresources, desc.DepthAttachment.isReadOnly);

		fb->DSV = index;

		st::rapi::TextureHandle handle = std::static_pointer_cast<ITexture>(texture->shared_from_this());
		fb->textures.push_back(handle);
	}

	return FramebufferHandle{ fb };
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
	m_D3d12Device->CreateCommandAllocator(d3dCommandListType, IID_PPV_ARGS(&d3d12CommandAllocator));
	ComPtr<ID3D12GraphicsCommandList> d3d12CommandList;
	m_D3d12Device->CreateCommandList(0, d3dCommandListType, d3d12CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&d3d12CommandList));

	auto* commandList = new CommandList{ d3d12CommandList.Get(), d3d12CommandAllocator.Get(), params.queueType };

	return CommandListHandle{ commandList };
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