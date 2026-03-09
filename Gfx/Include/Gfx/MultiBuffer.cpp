#include "Gfx/MultiBuffer.h"
#include "RHI/Buffer.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"

st::gfx::MultiBuffer::MultiBuffer() : 
	m_Usage{ st::rhi::BufferShaderUsage::None },
	m_SizeBytes{ 0 },
	m_Stride{ 0 },
	m_DeviceManager{ nullptr }
{}

st::gfx::MultiBuffer::~MultiBuffer() = default;

void st::gfx::MultiBuffer::InitRaw(const st::rhi::BufferShaderUsage usage, size_t sizeBytes, rhi::ResourceState defaultState,
								st::gfx::DeviceManager* deviceManager, const std::string& debugName)
{
	Release();

	m_Buffers.resize(deviceManager->GetSwapchainBufferCount());

	m_Usage = usage;
	m_SizeBytes = sizeBytes;
	m_Stride = 0;
	m_DefaultState = defaultState;
	m_DebugName = debugName;
	m_DeviceManager = deviceManager;
}

void st::gfx::MultiBuffer::InitStructured(const st::rhi::BufferShaderUsage usage, size_t sizeBytes, uint32_t stride, rhi::ResourceState defaultState,
	st::gfx::DeviceManager* deviceManager, const std::string& debugName)
{
	Release();

	m_Buffers.resize(deviceManager->GetSwapchainBufferCount());

	m_Usage = usage;
	m_SizeBytes = sizeBytes;
	m_Stride = stride;
	m_DefaultState = defaultState;
	m_DebugName = debugName;
	m_DeviceManager = deviceManager;
}

void st::gfx::MultiBuffer::Release()
{
	m_Buffers.clear();
	m_Usage = st::rhi::BufferShaderUsage::None;
	m_SizeBytes = 0;
	m_Stride = 0;
	m_DeviceManager = nullptr;
}

void st::gfx::MultiBuffer::Reset()
{
	m_Buffers.clear();
	m_Buffers.resize(m_DeviceManager->GetSwapchainBufferCount());
	m_SizeBytes = 0;
}

void st::gfx::MultiBuffer::SetSize(size_t size)
{
	m_SizeBytes = size;
}

void st::gfx::MultiBuffer::Grow(size_t sizeBytes)
{
	m_SizeBytes = std::max(m_SizeBytes, sizeBytes);
}

st::rhi::BufferHandle st::gfx::MultiBuffer::GetCurrentBuffer()
{
	if (m_SizeBytes == 0)
	{
		// Can't have an array of size 0
		return {};
	}

	auto& buffer = m_Buffers[m_DeviceManager->GetFrameModuleIndex()];
	if (!buffer || buffer->GetDesc().sizeBytes != m_SizeBytes)
	{
		auto desc = rhi::BufferDesc{
			.shaderUsage = m_Usage,
			.sizeBytes = m_SizeBytes,
			.stride = m_Stride };

		buffer = m_DeviceManager->GetDevice()->CreateBuffer(desc, m_DefaultState, std::format("{}[{}]",
			m_DebugName, m_DeviceManager->GetFrameModuleIndex()));
	}

	return buffer.get_weak();
}