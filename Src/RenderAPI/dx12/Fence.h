#pragma once

#include "RenderAPI/Fence.h"
#include "RenderAPI/dx12/d3d12_headers.h"

namespace st::rapi::dx12
{
	class Fence : public IFence
	{
	public:

		Fence(ID3D12Fence* fence, const char* debugName) : m_D3d12Fence{ fence }, m_DebugName{ debugName }
		{}

		uint64_t GetCompletedValue() override { return m_D3d12Fence->GetCompletedValue(); }

		ResourceType GetResourceType() const override { return ResourceType::Fence; }
		NativeResource GetNativeResource() override { return m_D3d12Fence.Get(); }

	private:

		void Release(Device* device) override { m_D3d12Fence.Reset(); }
		const std::string& GetDebugName() override { return m_DebugName; }

		ComPtr<ID3D12Fence> m_D3d12Fence;
		std::string m_DebugName;
	};
}