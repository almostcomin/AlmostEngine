#include "RHI/Resource.h"
#include "RHI/Device.h"

alm::rhi::IResource::IResource(Device* device, const std::string& debugName) : m_DebugName(debugName), m_Device(device)
{}

alm::rhi::IResource::~IResource()
{}