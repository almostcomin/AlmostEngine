#include "RenderAPI/dx12/CommandList.h"
#include "RenderAPI/dx12/Buffer.h"
#include "Core/Util.h"
#include <pix_win.h>

void st::rapi::dx12::CommandList::WriteBuffer(IBuffer* buffer, const void* data, size_t dataSize, uint64_t offset)
{
	auto* buffer = checked_cast<Buffer*>(buffer);

	akimekedao

}

void st::rapi::dx12::CommandList::BeginMarker(const char* str)
{
	PIXBeginEvent(m_D3d12Commandlist.Get(), 0, str);
}

void st::rapi::dx12::CommandList::EndMarker()
{
	PIXEndEvent(m_D3d12Commandlist.Get());
}