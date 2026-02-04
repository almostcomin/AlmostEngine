#include "Gfx/UploadBuffer.h"
#include "RHI/Device.h"

st::gfx::UploadBuffer::UploadBuffer(uint64_t firstFrameIdx, size_t bufferSize, rhi::Device* device) :
	m_Head{ 0 },
	m_Tail{ 0 },
	m_Size{ bufferSize },
	m_CurrentFrameIdx{ firstFrameIdx },
	m_Device{ device }
{
	rhi::BufferDesc bufferDesc{
		.memoryAccess = rhi::MemoryAccess::Upload,
		.shaderUsage = rhi::BufferShaderUsage::None,
		.sizeBytes = m_Size,
		.format = rhi::Format::UNKNOWN,
		.stride = 0 };

	m_Buffer = m_Device->CreateBuffer(bufferDesc, rhi::ResourceState::COPY_SRC, "UploadBuffer");
	m_BufferStartPtr = (char*)m_Buffer->Map();
}

st::gfx::UploadBuffer::~UploadBuffer()
{
	assert(m_Head == m_Tail);
}

std::pair<void*, uint64_t> st::gfx::UploadBuffer::RequestSpaceForBufferDataUpload(size_t size)
{
	return RequestSpace(size, m_Device->GetCopyDataAlignment(rhi::CopyMethod::Buffer2Buffer));
}

std::pair<void*, uint64_t> st::gfx::UploadBuffer::RequestSpaceForTextureDataUpload(size_t size)
{
	return RequestSpace(size, m_Device->GetCopyDataAlignment(rhi::CopyMethod::Buffer2Texture));
}

std::pair<void*, uint64_t> st::gfx::UploadBuffer::RequestSpace(size_t size, size_t alignment)
{
	if (alignment == 0)
		alignment = 1;

	std::scoped_lock lock{ m_RequestMutex };

	auto head = m_Head;
	auto tail = m_Tail;
	uint64_t startTail = UINT64_MAX;
	auto sizeNeeded = size;

	bool found = false;
	// Case 1: tail >= head, try tail to end
	if (tail >= head)
	{
		sizeNeeded = AlignUp(tail, alignment) - tail + size;
		// Check if there is enough space from the tail to the end
		if (tail + sizeNeeded <= m_Size)
		{
			startTail = tail;
		}
		else
		{
			// Try wrap to beginning
			auto tail = 0;
			sizeNeeded = size; // 0 is aligned
			if (sizeNeeded < head)
			{
				startTail = tail;
			}
		}
	}
	// Case 2: tail < head, try from tail to head
	else
	{
		sizeNeeded = AlignUp(tail, alignment) - tail + size;
		if (tail + sizeNeeded < head)
		{
			startTail = tail;
		}
	}

	if (startTail == UINT64_MAX)
	{
		LOG_ERROR("Not enough space in upload buffer, requested: {}", size);
		return { nullptr, UINT64_MAX };
	}

	uint64_t alignedStart = AlignUp(startTail, alignment);
	m_Tail = startTail + sizeNeeded;

	return { m_BufferStartPtr + alignedStart, alignedStart };
}

void st::gfx::UploadBuffer::OnNextFrame()
{
	m_FrameInfos.Push({ m_CurrentFrameIdx, m_Tail });
	++m_CurrentFrameIdx;
}

void st::gfx::UploadBuffer::OnFrameCompleted(uint64_t frameIdx)
{
	assert(!m_FrameInfos.Empty());
	assert(m_FrameInfos.Front().FrameIdx == frameIdx);

	m_Head = m_FrameInfos.Front().BufferPos;
	m_FrameInfos.Pop();
}