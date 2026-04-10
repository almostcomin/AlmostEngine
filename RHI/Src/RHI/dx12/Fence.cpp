#include "RHI/RHI_PCH.h"
#include "RHI/dx12/Fence.h"

void alm::rhi::dx12::Fence::Wait(uint64_t value)
{
	// Test if the fence has been reached
	if (GetCompletedValue() < value)
	{
		// If it's not, wait for it to finish using an event
		HANDLE fenceEvent = CreateEvent(nullptr, false, false, nullptr);
		//ResetEvent(event);
		m_D3d12Fence->SetEventOnCompletion(value, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}
}
