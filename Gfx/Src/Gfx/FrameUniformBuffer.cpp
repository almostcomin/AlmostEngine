#include "Gfx/GfxPCH.h"
#include "Gfx/FrameUniformBuffer.h"
#include "RHI/Device.h"
#include "RHI/Buffer.h"

alm::gfx::FrameUniformBufferRaw::FrameUniformBufferRaw(size_t bufferCount, size_t bufferSize, alm::rhi::Device* device, const std::string& debugName)
{
	rhi::BufferDesc desc{
		.memoryAccess = rhi::MemoryAccess::Upload,
		.shaderUsage = rhi::BufferShaderUsage::Uniform,
		.sizeBytes = bufferSize,
		.stride = 0 };

	m_Buffers.resize(bufferCount);
	for (int i = 0; i < m_Buffers.size(); ++i)
	{
		std::stringstream ss;
		ss << debugName << "[" << i << "]";
		m_Buffers[i] = device->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE, ss.str());
	}

	m_CurrentIndex = 0;
}

alm::gfx::FrameUniformBufferRaw::~FrameUniformBufferRaw()
{}

void* alm::gfx::FrameUniformBufferRaw::GetNextPtrRaw()
{
	m_CurrentIndex = (m_CurrentIndex + 1) % m_Buffers.size();
	return m_Buffers[m_CurrentIndex]->Map();
}

alm::rhi::BufferUniformView alm::gfx::FrameUniformBufferRaw::GetUniformView()
{
	return m_Buffers[m_CurrentIndex]->GetUniformView();
}
