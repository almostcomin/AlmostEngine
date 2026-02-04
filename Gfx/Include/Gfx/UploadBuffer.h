#pragma once

#include "Core/RingBuffer.h"

namespace st::rhi
{
	class Device;
}

namespace st::gfx
{

class UploadBuffer
{
public:

	UploadBuffer(uint64_t firstFrameIdx, size_t bufferSize, rhi::Device* device);
	~UploadBuffer();

	template<typename T>
	std::pair<T*, uint64_t> RequestSpaceForBufferDataUpload() {
		auto result = RequestSpaceForBufferDataUpload(sizeof(T));
		return std::make_pair(static_cast<T*>(result.first), result.second);
	}

	std::pair<void*, uint64_t> RequestSpaceForBufferDataUpload(size_t size);
	std::pair<void*, uint64_t> RequestSpaceForTextureDataUpload(size_t size);
	std::pair<void*, uint64_t> RequestSpace(size_t size, size_t alignment);

	void OnNextFrame();
	void OnFrameCompleted(uint64_t frameIdx);

	rhi::BufferHandle GetBuffer() { return m_Buffer.get_weak(); }

private:

	struct FrameBufferInfo
	{
		uint64_t FrameIdx;
		uint64_t BufferPos;
	};

private:

	rhi::BufferOwner m_Buffer;
	char* m_BufferStartPtr;
	size_t m_Size;

	uint64_t m_Head;
	uint64_t m_Tail;
	std::mutex m_RequestMutex;

	st::RingBuffer<FrameBufferInfo, 8> m_FrameInfos;
	uint64_t m_CurrentFrameIdx;

	rhi::Device* m_Device;
};

} // namespace st::gfx