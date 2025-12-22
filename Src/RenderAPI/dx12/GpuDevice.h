#pragma once

#include "Core/ComPtr.h"
#include "RenderAPI/dx12/Device.h"
#include "RenderAPI/dx12/DescriptorHeap.h"
#include <unordered_set>

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

		ShaderHandle CreateShader(const ShaderDesc& desc, const WeakBlob& bytecode) override;
		BufferHandle CreateBuffer(const BufferDesc& desc, ResourceState initialState) override;
		TextureHandle CreateTexture(const TextureDesc& desc, ResourceState initialState) override;
		TextureHandle CreateHandleForNativeTexture(void* obj, const TextureDesc& desc) override;
		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) override;
		CommandListHandle CreateCommandList(const CommandListParams& params) override;
		GraphicsPipelineStateHandle CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, const FramebufferInfo& fbInfo) override;
		FenceHandle CreateFence(uint64_t initialVale, const char* debugName) override;

		StorageRequirements GetStorageRequirements(const BufferDesc& desc) override;
		StorageRequirements GetStorageRequirements(const TextureDesc& desc) override;
		StorageRequirements GetCopyableRequirements(const BufferDesc& desc) override;
		StorageRequirements GetCopyableRequirements(const TextureDesc& desc) override;

		size_t GetCopyDataAlignment(CopyMethod method) override;

		DescriptorHeap* GetDepthStencilViewHeap() { return &m_DepthStencilViewHeap; }
		DescriptorHeap* GetRenderTargetViewHeap() { return &m_RenderTargetViewHeap; }
		DescriptorHeap* GetShaderResourceViewHeap() { return &m_ShaderResourceViewHeap; }
		DescriptorHeap* GetSamperHeap() { return &m_SamplerHeap; }

		void ExecuteCommandLists(std::span<ICommandList*> commandLists, QueueType type, IFence* signal, uint64_t value) override;
		void ExecuteCommandList(ICommandList* commandList, QueueType type, IFence* signal, uint64_t value) override;

		void WaitForIdle() override;

		void NextFrame() override;

		void Shutdown() override;

		ID3D12Device* GetNativeDevice() { return m_D3d12Device.Get(); }

	protected:

		void ReleaseImmediatelyInternal(IResource* resource) override;
		void ReleaseQueuedInternal(IResource* resource) override;

	private:

		void CreateBindlessRootSignature();

		D3D12_RESOURCE_DESC BuildD3d12Desc(const TextureDesc& desc);

		template<class T>
		st::weak<T> InsertNewResource(T* p)
		{
			std::scoped_lock lock{ m_LivingResourcesMutex };
			auto insertResult = m_LivingResources.insert(std::move(MakeIResourceUnique(checked_cast<IResource*>(p))));
			return st::static_pointer_cast<T>(insertResult.first->get_weak());
		}

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

		ComPtr<ID3D12RootSignature> m_BindlessRootSignature;

		std::mutex m_LivingResourcesMutex;
		std::unordered_set<st::unique<IResource>, ResourcePtrHash, ResourcePtrEqual> m_LivingResources;

		std::mutex m_StaleResourcesMutex;
		std::vector<std::vector<IResource*>> m_StaleResources;

		uint64_t m_CurrentFrameIdx;

		DeviceDesc m_Desc;
	};

} // namespace st::rapi::dx12