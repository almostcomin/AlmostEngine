#pragma once

#include "RenderAPI/CommandList.h"
#include <wrl/client.h>
#include <d3d12.h>

using namespace Microsoft::WRL;

namespace st::rapi::dx12
{
	class CommandList : public ICommandList
	{
	public:

		CommandList(ID3D12GraphicsCommandList* d3d12CommandList, ID3D12CommandAllocator* commandAllocator, QueueType type) :
			m_D3d12Commandlist{ d3d12CommandList },
			m_D3d12CommandAllocator{ commandAllocator },
			m_Type{ type }
		{}

		QueueType GetType() const override { return m_Type; }

	private:

		ComPtr<ID3D12GraphicsCommandList> m_D3d12Commandlist;
		ComPtr<ID3D12CommandAllocator> m_D3d12CommandAllocator;
		QueueType m_Type;
	};
}