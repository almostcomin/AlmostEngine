#pragma once

#include "RHI/ShaderViews.h"

namespace st::rhi
{
	class Device;
}

namespace st::gfx
{

class FrameUniformBufferRaw
{
public:

	FrameUniformBufferRaw(size_t bufferCount, size_t bufferSize, rhi::Device* device, const std::string& debugName);
	~FrameUniformBufferRaw();
	
	void* GetNextPtrRaw();
	st::rhi::BufferUniformView GetUniformView();
	
private:

	std::vector<st::rhi::BufferOwner> m_Buffers;
	uint32_t m_CurrentIndex;
};

template<class T>
class FrameUniformBuffer : public FrameUniformBufferRaw
{
public:

	FrameUniformBuffer(size_t bufferCount, rhi::Device* device, const std::string& debugName) :
		FrameUniformBufferRaw(bufferCount, sizeof(T), device, debugName)
	{}

	T* GetNextPtr() { return (T*)GetNextPtrRaw(); }
};

} // namespace st::gfx