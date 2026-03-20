#pragma once

#include "Core/ComPtr.h"
#include "RHI/dx12/Device.h"
#include "RHI/dx12/DescriptorHeap.h"
#include "Core/BitSetAllocator.h"

namespace st::rhi::dx12
{
	struct Queue
	{
		ComPtr<ID3D12CommandQueue> d3d12Queue;
		ComPtr<ID3D12Fence> d3d12Fence;
		uint64_t lastSubmittedInstance = 0;
		uint64_t lastCompletedInstance = 0;

		uint64_t GetLastCompletedInstance()
		{
			if (lastCompletedInstance < lastSubmittedInstance)
			{
				lastCompletedInstance = d3d12Fence->GetCompletedValue();
			}
			return lastCompletedInstance;
		}
	};

	class GpuDevice : public st::rhi::Device
	{
	public:

		GpuDevice(const DeviceDesc& desc);
		~GpuDevice();

		ShaderOwner CreateShader(const ShaderDesc& desc, const WeakBlob& bytecode, const std::string& debugName) override;
		BufferOwner CreateBuffer(const BufferDesc& desc, ResourceState initialState, const std::string& debugName) override;
		TextureOwner CreateTexture(const TextureDesc& desc, ResourceState initialState, const std::string& debugName) override;
		TextureOwner CreateHandleForNativeTexture(void* obj, const TextureDesc& desc, const std::string& debugName) override;
		FramebufferOwner CreateFramebuffer(const FramebufferDesc& desc, const std::string& debugName) override;
		CommandListOwner CreateCommandList(const CommandListParams& params, const std::string& debugName) override;
		GraphicsPipelineStateOwner CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, const FramebufferInfo& fbInfo, const std::string& debugName) override;
		ComputePipelineStateOwner CreateComputePipelineState(const ComputePipelineStateDesc& desc, const std::string& debugName) override;
		FenceOwner CreateFence(uint64_t initialVale, const std::string& debugName) override;
		TimerQueryOwner CreateTimerQuery(const std::string& debugName) override;

		StorageRequirements GetStorageRequirements(const BufferDesc& desc) override;
		StorageRequirements GetStorageRequirements(const TextureDesc& desc) override;
		StorageRequirements GetCopyableRequirements(const BufferDesc& desc) override;
		StorageRequirements GetCopyableRequirements(const TextureDesc& desc, const rhi::TextureSubresourceSet& subresources) override;
		SubresourceCopyableRequirements GetSubresourceCopyableRequirements(const TextureDesc& desc, uint32_t mipLevel, uint32_t arraySlice) override;

		size_t GetCopyDataAlignment(CopyMethod method) override;

		TextureSampledView CreateTextureSampledView(ITexture* texture, const TextureSubresourceSet& subresources,
			Format format, TextureDimension dimension) override;
		TextureStorageView CreateTextureStorageView(ITexture* texture, const TextureSubresourceSet& subresources,
			Format format, TextureDimension dimension) override;
		TextureColorTargetView CreateTextureColorTargetView(ITexture* texture, Format format, TextureSubresourceSet subresources) override;
		TextureDepthTargetView CreateTextureDepthTargetView(ITexture* texture, TextureSubresourceSet subresources, bool isReadOnly) override;

		void ReleaseTextureSampledView(TextureSampledView& v, bool immediate = false) override;
		void ReleaseTextureStorageView(TextureStorageView& v, bool immediate = false) override;
		void ReleaseTextureColorTargetView(TextureColorTargetView& v, bool immediate = false) override;
		void ReleaseTextureDepthTargetView(TextureDepthTargetView& v, bool immediate = false) override;

		BufferUniformView CreateBufferUniformView(IBuffer* buffer, uint32_t start = 0, int size = -1) override;
		BufferReadOnlyView CreateBufferReadOnlyView(IBuffer* buffer, uint32_t start = 0, int size = -1) override;
		BufferReadWriteView CreateBufferReadWriteView(IBuffer* buffer, uint32_t start = 0, int size = -1) override;

		void ReleaseBufferUniformView(BufferUniformView& v, bool immediate = false) override;
		void ReleaseBufferReadOnlyView(BufferReadOnlyView& v, bool immediate = false) override;
		void ReleaseBufferReadWriteView(BufferReadWriteView& v, bool immediate = false) override;

		void ExecuteCommandLists(std::span<ICommandList* const> commandLists, QueueType type, IFence* signal, uint64_t value) override;
		void ExecuteCommandList(ICommandList* commandList, QueueType type, IFence* signal, uint64_t value) override;

		void WaitForIdle() override;

		void NextFrame() override;

		void Shutdown() override;

		NativeResource GetNativeDevice() override { return m_D3d12Device.Get(); }

		const Stats& GetStats() const override { return m_LastStats; }

		Queue* GetQueue(QueueType type) { return &m_Queues[(int)type]; }

		DescriptorHeap* GetDepthStencilViewHeap() { return &m_DepthStencilViewHeap; }
		DescriptorHeap* GetRenderTargetViewHeap() { return &m_RenderTargetViewHeap; }
		DescriptorHeap* GetShaderResourceViewHeap() { return &m_ShaderResourceViewHeap; }
		DescriptorHeap* GetSamperHeap() { return &m_SamplerHeap; }

		ID3D12QueryHeap* GetQueryHeap() { return m_TimerQueryHeap.Get(); }
		ID3D12Resource* GetQueryResolveBuffer() { return m_TimerQueryResolveBuffer.Get(); }

		ID3D12RootSignature* GetBindlessRootSignature() { return m_BindlessRootSignature.Get(); }

		ID3D12Device* GetD3d12Device() { return m_D3d12Device.Get(); }

	protected:

		void ReleaseImmediatelyInternal(IResource* resource) override;
		void ReleaseQueuedInternal(IResource* resource) override;

	private:

		struct FrameStaleResources
		{
			std::vector<IResource*> Resources;
			std::vector<std::pair<DescriptorIndex, DescriptorHeap*>> Descriptors;
		};

	private:

		void CreateBindlessRootSignature();

		D3D12_RESOURCE_DESC BuildD3d12Desc(const TextureDesc& desc);

		void CreateTextureSRV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources, TextureDimension dimension);
		void CreateTextureUAV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources, TextureDimension dimension);
		void CreateTextureRTV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, Format format, TextureSubresourceSet subresources);
		void CreateTextureDSV(ITexture* texture, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, TextureSubresourceSet subresources, bool isReadOnly);

		void CreateBufferCBV(IBuffer* buffer, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, uint32_t sizeBytes);
		void CreateBufferSRV(IBuffer* buffer, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, uint32_t sizeBytes);
		void CreateBufferUAV(IBuffer* buffer, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t offsetBytes, uint32_t sizeBytes);

		template<class T>
		st::unique<T> InsertNewResource(T* p)
		{
			static_assert(std::is_base_of_v<IResource, T>);

			std::scoped_lock lock{ m_LivingResourcesMutex };
			st::unique<IResource> handle = MakeIResourceUnique(p);
			auto insertResult = m_LivingResources.insert(handle.get());
			return st::adopt_unique<T>(std::move(handle));
		}

		void ReleaseStaleResources(uint32_t bufferIdx);

	private:

		std::array<Queue, (int)QueueType::_Count> m_Queues;

		DescriptorHeap m_DepthStencilViewHeap;
		DescriptorHeap m_RenderTargetViewHeap;
		DescriptorHeap m_ShaderResourceViewHeap;
		DescriptorHeap m_SamplerHeap;

		BitSetAllocator m_TimerQueries;
		ComPtr<ID3D12QueryHeap> m_TimerQueryHeap;
		ComPtr<ID3D12Resource> m_TimerQueryResolveBuffer;

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
		std::unordered_set<IResource*> m_LivingResources;

		std::mutex m_StaleResourcesMutex;
		std::mutex m_StaleDescriptorsMutex;
		std::vector<FrameStaleResources> m_StaleResources;

		uint64_t m_CurrentFrameIdx;

		Stats m_LastStats;
		Stats m_CurrentStats;

		DeviceDesc m_Desc;
	};

} // namespace st::rhi::dx12