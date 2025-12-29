#pragma once

#include "RHI/Fence.h"
#include "RHI/dx12/d3d12_headers.h"

namespace st::rhi::dx12
{
	class Fence : public IFence
	{
	public:

		Fence(ID3D12Fence* fence, Device* device, const std::string& debugName) : IFence(device, debugName), m_D3d12Fence{ fence }
		{}

		uint64_t GetCompletedValue() override { return m_D3d12Fence->GetCompletedValue(); }

		ResourceType GetResourceType() const override { return ResourceType::Fence; }
		NativeResource GetNativeResource() override { return m_D3d12Fence.Get(); }

	private:

		void Release(Device* device) override { m_D3d12Fence.Reset(); }

		ComPtr<ID3D12Fence> m_D3d12Fence;
	};
}