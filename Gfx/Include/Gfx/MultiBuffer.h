#pragma once

#include "RHI/ResourceState.h"
#include "RHI/Descriptors.h"
#include "RHI/ShaderViews.h"

namespace alm::rhi
{
	struct BufferDesc;
}

namespace alm::gfx
{
	class DeviceManager;
}

namespace alm::gfx
{

class MultiBuffer
{
public:

	MultiBuffer();
	~MultiBuffer();

	bool IsInitialized() const { return !m_Buffers.empty(); }

	void InitRaw(alm::rhi::BufferShaderUsage usage, size_t sizeBytes, rhi::ResourceState defaultState, alm::gfx::DeviceManager* deviceManager,
			  const std::string& debugName);
	void InitStructured(alm::rhi::BufferShaderUsage usage, size_t sizeBytes, uint32_t stride, rhi::ResourceState defaultState,
		alm::gfx::DeviceManager* deviceManager, const std::string& debugName);
	void InitUniformBuffer(size_t sizeBytes, alm::gfx::DeviceManager* deviceManager, const std::string& debugName);

	void Release();

	void Reset();

	size_t GetSizeBytes() const { return m_SizeBytes; }
	void SetSize(size_t size);
	void Grow(size_t sizeBytes);

	void* Map();
	void Unmap();

	rhi::BufferUniformView GetUniformView();
	rhi::BufferReadOnlyView GetReadOnlyView();
	rhi::BufferReadWriteView GetReadWriteView();

	rhi::BufferHandle GetCurrentBuffer();

private:

	alm::rhi::MemoryAccess m_MemoryAccess;
	alm::rhi::BufferShaderUsage m_Usage;
	rhi::ResourceState m_DefaultState;

	std::vector<alm::rhi::BufferOwner> m_Buffers;
	size_t m_SizeBytes;
	uint32_t m_Stride; // For structured buffer only

	std::string m_DebugName;
	alm::gfx::DeviceManager* m_DeviceManager;
};

} // namespace st::gfx