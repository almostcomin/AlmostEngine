#include "Gfx/MultiBuffer.h"
#include "RHI/Buffer.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"

st::gfx::MultiBuffer::MultiBuffer() : 
	m_Usage{ st::rhi::BufferShaderUsage::None },
	m_Size { 0 },
	m_DeviceManager{ nullptr }
{}

st::gfx::MultiBuffer::~MultiBuffer() = default;

void st::gfx::MultiBuffer::Init(const st::rhi::BufferShaderUsage usage, size_t size, rhi::ResourceState defaultState,
								st::gfx::DeviceManager* deviceManager, const std::string& debugName)
{
	Release();

	m_Buffers.resize(deviceManager->GetSwapchainBufferCount());

	m_Usage = usage;
	m_Size = size;
	m_DefaultState = defaultState;
	m_DebugName = debugName;
	m_DeviceManager = deviceManager;
}

void st::gfx::MultiBuffer::Release()
{
	m_Buffers.clear();
	m_Usage = st::rhi::BufferShaderUsage::None;
	m_Size = 0;
	m_DeviceManager = nullptr;
}

void st::gfx::MultiBuffer::SetSize(size_t size)
{
	m_Size = size;
}

void st::gfx::MultiBuffer::Grow(size_t size)
{
	m_Size = std::max(m_Size, size);
}

st::rhi::BufferHandle st::gfx::MultiBuffer::GetCurrentBuffer()
{
	if (m_Size == 0)
	{
		// Can't have an array of size 0
		return {};
	}

	auto& buffer = m_Buffers[m_DeviceManager->GetFrameModuleIndex()];
	if (!buffer || buffer->GetDesc().sizeBytes != m_Size)
	{
		buffer = m_DeviceManager->GetDevice()->CreateBuffer(
			rhi::BufferDesc{ .shaderUsage = m_Usage, .sizeBytes = m_Size },
			m_DefaultState, std::format("{}[{}]", m_DebugName, m_DeviceManager->GetFrameModuleIndex()));
	}

	return buffer.get_weak();
}