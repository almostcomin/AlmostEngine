#pragma once

#include <nvrhi/nvrhi.h>
#include <dxgi.h>

namespace st::gfx
{

	nvrhi::Format GetFormat(DXGI_FORMAT format);
	uint32_t BitsPerPixel(nvrhi::Format format);

} // namespace st::gfx