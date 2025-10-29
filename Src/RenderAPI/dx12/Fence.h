#pragma once

#include "RenderAPI/Fence.h"
#include <wrl/client.h>
#include <d3d12.h>

namespace st::rapi::dx12
{
	class Fence : public IFence
	{
	public:

		Fence(ID3D12Fence* fence) : m_D3d12Fence{ fence }
		{}

	private:

		Microsoft::WRL::ComPtr<ID3D12Fence> m_D3d12Fence;
	};
}