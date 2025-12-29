#include "RHI/Resource.h"
#include "RHI/Device.h"

st::rhi::IResource::IResource(Device* device, const std::string& debugName) : m_DebugName(debugName), m_Device(device)
{}

st::rhi::IResource::~IResource()
{}