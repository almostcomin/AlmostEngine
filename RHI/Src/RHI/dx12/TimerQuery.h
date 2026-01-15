#pragma once

#include "RHI/TimerQuery.h"
#include "RHI/dx12/d3d12_headers.h"
#include "Core/ComPtr.h"

namespace st::rhi::dx12
{
	class Queue;

	class TimerQuery : public ITimerQuery
	{
	public:

		TimerQuery(uint32_t beginQueryIndex, uint32_t endQueryIndex, Device* device, const std::string& debugName) :
			ITimerQuery{ device, debugName },
			m_BeginQueryIndex{ beginQueryIndex },
			m_EndQueryIndex{ endQueryIndex }
		{}

		void Reset() override
		{
			m_BeginExecuted = false;
			m_EndExecuted = false;
			m_Resolved = false;
			m_Time = 0.f;
			m_Fence = nullptr;
		};

		float GetQueryTime() override;
		bool Poll() override;

		ResourceType GetResourceType() const override { return ResourceType::TimerQuery; }
		NativeResource GetNativeResource() override { assert(0); return nullptr; }

		void OnBeginExecuted();
		void OnEndExecuted(Queue& queue);

	private:

		void Release(Device* device) override {}

	public:

		uint32_t m_BeginQueryIndex = 0;
		uint32_t m_EndQueryIndex = 0;

		ComPtr<ID3D12Fence> m_Fence;
		uint64_t m_FenceCounter = 0;

		bool m_BeginExecuted = false;
		bool m_EndExecuted = false;
		bool m_Resolved = false;
		float m_Time = 0.f;
	};
}