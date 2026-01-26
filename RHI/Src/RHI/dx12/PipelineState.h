#pragma once

#include "RHI/PipelineState.h"
#include "RHI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"

namespace st::rhi::dx12
{

	class GraphicsPipelineState : public IGraphicsPipelineState
	{
	public:

		GraphicsPipelineState(ComPtr<ID3D12PipelineState> pso, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d3d12Desc, const GraphicsPipelineStateDesc& desc,
							  Device* device, const std::string& debugName) :
			IGraphicsPipelineState{ device, debugName },
			m_PSO{ pso },
			m_D3d12Desc{ d3d12Desc },
			m_Desc{ desc }
		{}

		const GraphicsPipelineStateDesc& GetDesc() const override { return m_Desc; }
		D3D12_GRAPHICS_PIPELINE_STATE_DESC GetD3d12Desc() const { return m_D3d12Desc; }

		ResourceType GetResourceType() const override { return ResourceType::GraphicsPipelineState; }
		NativeResource GetNativeResource() override { return m_PSO.Get(); }

	private:

		void Release(Device* device) override { m_PSO.Reset(); }

		ComPtr<ID3D12PipelineState> m_PSO;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_D3d12Desc;
		GraphicsPipelineStateDesc m_Desc;
	};

	class ComputePipelineState : public IComputePipelineState
	{
	public:

		ComputePipelineState(ComPtr<ID3D12PipelineState> pso, const D3D12_COMPUTE_PIPELINE_STATE_DESC& d3d12Desc, Device* device, const std::string& debugName) :
			IComputePipelineState{ device, debugName },
			m_PSO{ pso },
			m_D3d12Desc{ d3d12Desc }
		{}

		D3D12_COMPUTE_PIPELINE_STATE_DESC GetD3d12Desc() const { return m_D3d12Desc; }

		ResourceType GetResourceType() const override { return ResourceType::ComputePipelineState; }
		NativeResource GetNativeResource() override { return m_PSO.Get(); }

	private:

		void Release(Device* device) override { m_PSO.Reset(); }

		ComPtr<ID3D12PipelineState> m_PSO;
		D3D12_COMPUTE_PIPELINE_STATE_DESC m_D3d12Desc;
	};

} // namespace st::rhi::dx12