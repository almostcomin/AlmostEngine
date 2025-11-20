#pragma once

#include "Core/ComPtr.h"
#include "RenderAPI/dx12/Device.h"
#include "RenderAPI/dx12/DescriptorHeap.h"

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
		BufferHandle CreateBuffer(const BufferDesc& desc) override;
		TextureHandle CreateTexture(const TextureDesc& desc, ResourceState initialState) override;
		TextureHandle CreateHandleForNativeTexture(void* obj, const TextureDesc& desc) override;
		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) override;
		CommandListHandle CreateCommandList(const CommandListParams& params) override;
		GraphicsPipelineStateHandle CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, const FramebufferInfo& fbInfo) override;
		FenceHandle CreateFence() override;

		D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCPUDescriptorHandle(DescriptorIndex idx);

		DescriptorHeap* GetShaderResourceViewHeap() { return &m_ShaderResourceViewHeap; }

		void ExecuteCommandLists(std::span<ICommandList*> commandLists, QueueType type, IFence* signal, uint64_t value) override;
		void ExecuteCommandList(ICommandList* commandList, QueueType type, IFence* signal, uint64_t value) override;

		void WaitForIdle() override;

		ID3D12Device* GetNativeDevice() { return m_D3d12Device.Get(); }

	private:

		void CreateBindlessRootSignature();

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
	};

} // namespace st::rapi::dx12