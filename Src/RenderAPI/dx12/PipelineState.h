#pragma once

#include "RenderAPI/PipelineState.h"
#include "RenderAPI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"

namespace st::rapi::dx12
{

	class GraphicsPipelineState : public IGraphicsPipelineState
	{
	public:

		GraphicsPipelineState(ComPtr<ID3D12PipelineState> pso, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d3d12Desc, const GraphicsPipelineStateDesc& desc) :
			m_PSO{ pso },
			m_D3d12Desc{ d3d12Desc },
			m_Desc{ desc }
		{}

		const GraphicsPipelineStateDesc& GetDesc() const override { return m_Desc; }
		D3D12_GRAPHICS_PIPELINE_STATE_DESC GetD3d12Desc() const { return m_D3d12Desc; }

		NativeResource GetNativeResource() override { return m_PSO.Get(); }

	private:

		ComPtr<ID3D12PipelineState> m_PSO;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_D3d12Desc;
		GraphicsPipelineStateDesc m_Desc;
	};

} // namespace st::rapi::dx12