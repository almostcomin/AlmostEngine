#include "RHI/dx12/TimerQuery.h"
#include "RHI/dx12/Utils.h"
#include "RHI/dx12/GpuDevice.h"

float alm::rhi::dx12::TimerQuery::GetQueryTimeMs()
{
    if (!m_Resolved)
    {
        if (m_Fence)
        {
            HANDLE fenceEvent = CreateEvent(nullptr, false, false, nullptr);
            WaitForFence(m_Fence.Get(), m_FenceCounter, fenceEvent);
            m_Fence = nullptr;
            CloseHandle(fenceEvent);
        }

        uint64_t frequency;
        auto* device = alm::checked_cast<GpuDevice*>(GetDevice());
        device->GetQueue(QueueType::Graphics)->d3d12Queue->GetTimestampFrequency(&frequency);

        D3D12_RANGE bufferReadRange = {
            m_BeginQueryIndex * sizeof(uint64_t),
            (m_BeginQueryIndex + 2) * sizeof(uint64_t) };
        uint64_t* data;
        const HRESULT res = device->GetQueryResolveBuffer()->Map(0, &bufferReadRange, (void**)&data);

        if (FAILED(res))
        {
            LOG_ERROR("getTimerQueryTime: Map() failed");
            return 0.f;
        }

        m_Resolved = true;
        m_Time = float(double(data[m_EndQueryIndex] - data[m_BeginQueryIndex]) * 1000.0 / double(frequency));

        device->GetQueryResolveBuffer()->Unmap(0, nullptr);
    }

    return m_Time;
}

bool alm::rhi::dx12::TimerQuery::Poll()
{
    if (!m_EndExecuted)
        return false;

    if (!m_Fence)
        return true;

    if (m_Fence->GetCompletedValue() >= m_FenceCounter)
    {
        m_Fence = nullptr;
        return true;
    }

    return false;
}

void alm::rhi::dx12::TimerQuery::OnBeginExecuted()
{
    assert(!m_BeginExecuted);
    m_BeginExecuted = true;
}

void alm::rhi::dx12::TimerQuery::OnEndExecuted(Queue& queue)
{
    assert(!m_EndExecuted);

    m_EndExecuted = true;
    m_Fence = queue.d3d12Fence;
    m_FenceCounter = queue.lastSubmittedInstance;
}