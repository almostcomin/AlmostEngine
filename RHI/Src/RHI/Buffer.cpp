#include "RHI/RHI_PCH.h"
#include "RHI/Buffer.h"
#include "RHI/Device.h"

uint64_t alm::rhi::IBuffer::GetSizeInBytes() const
{
    StorageRequirements req = GetDevice()->GetStorageRequirements(GetDesc());
    return req.size;
}