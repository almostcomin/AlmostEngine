#pragma once

#include "RenderAPI/PipelineState.h"
#include "RenderAPI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"

namespace st::rapi::dx12
{

	class GraphicsPipelineState : public IGraphicsPipelineState
	{
	public:

		GraphicsPipelineState(ComPtr<ID3D12PipelineState> pso, const GraphicsPipelineStateDesc& desc) : 
			m_PSO{ pso },
			m_Desc{ desc }
		{}

		const GraphicsPipelineStateDesc& GetDesc() const override { return m_Desc; }

	private:

		GraphicsPipelineStateDesc m_Desc;
		ComPtr<ID3D12PipelineState> m_PSO;
	};

} // namespace st::rapi::dx12