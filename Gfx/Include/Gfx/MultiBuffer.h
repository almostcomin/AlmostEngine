#pragma once

#include "RHI/ResourceState.h"
#include "RHI/Descriptors.h"

namespace st::rhi
{
	struct BufferDesc;
}

namespace st::gfx
{
	class DeviceManager;
}

namespace st::gfx
{

class MultiBuffer
{
public:

	MultiBuffer();
	~MultiBuffer();

	bool IsInitialized() const { return !m_Buffers.empty(); }

	void Init(const st::rhi::BufferShaderUsage usage, size_t size, rhi::ResourceState defaultState, st::gfx::DeviceManager* deviceManager, const std::string& debugName);
	void Release();

	size_t GetSize() const { return m_Size; }
	void SetSize(size_t size);
	void Grow(size_t size);

	rhi::BufferHandle GetCurrentBuffer();

private:

	st::rhi::BufferShaderUsage m_Usage;
	rhi::ResourceState m_DefaultState;

	std::vector<st::rhi::BufferOwner> m_Buffers;
	size_t m_Size;

	std::string m_DebugName;
	st::gfx::DeviceManager* m_DeviceManager;
};

} // namespace st::gfx