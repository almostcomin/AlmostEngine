#include "Gfx/DataUploader.h"
#include "Core/Log.h"
#include "Gfx/Util.h"
#include "RenderAPI/Device.h"
#include "Core/Util.h"

namespace
{
	static constexpr size_t c_UploadBufferSize = st::MiB(256);
	static constexpr uint64_t c_FailedToReserveSpace = std::numeric_limits<uint64_t>::max();
}

// m_CommitCount starts with 1 since 0 is interpreted as not-initialized
st::gfx::DataUploader::DataUploader(rapi::Device* device) : 
	m_CommitCount{ 1 }, m_CommitFence{ device->CreateFence() }, m_Device { device }
{
	m_UploadBuffer = m_Device->CreateBuffer(rapi::BufferDesc{
		.memoryAccess = rapi::MemoryAccess::Upload,
		.shaderUsage = rapi::ShaderUsage::None,
		.sizeBytes = c_UploadBufferSize
	}, rapi::ResourceState::COMMON);
	m_UploadBufferHead = 0;
	m_UploadBufferTail = 0;
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadBufferData(
	const st::WeakBlob& srcData, st::rapi::BufferHandle dstBuffer, st::rapi::ResourceState currentBufferState, st::rapi::ResourceState targetBufferState,
	size_t dstStart, const char* opt_gpuMarker)
{
	st::rapi::BufferDesc desc = dstBuffer->GetDesc();
	assert(dstStart + srcData.size() <= desc.sizeBytes);

	uint64_t uploadBufferOffset = RequestUploadBufferSpace(srcData.size());
	if (uploadBufferOffset == c_FailedToReserveSpace)
	{
		// TODO: Can just happen that wwe are requestion too much uploads in single frame.
		// So we have to wait the completion of some in-flight commands to free space for new
		// TODO: Create a pending commands list that can be activated after space is freed (RunGarbageCollector)
		return std::unexpected("Failed to reserve memory in upload buffer");
	}
	void* uploadBufferMem = m_UploadBuffer->Map(uploadBufferOffset, srcData.size());
	if (!uploadBufferMem)
	{
		return std::unexpected("Failed to map upload buffer");
	}	
	std::memcpy(uploadBufferMem, srcData.data(), srcData.size());
	m_UploadBuffer->Unmap(uploadBufferOffset, srcData.size());

	auto commandList = GetNewCommandList();

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());

	commandList->PushBarrier(
		rapi::Barrier::Buffer(dstBuffer.get(), currentBufferState, rapi::ResourceState::COPY_DST));

	commandList->WriteBuffer(dstBuffer.get(), dstStart, m_UploadBuffer.get(), uploadBufferOffset, srcData.size());

	commandList->PushBarrier(
		rapi::Barrier::Buffer(dstBuffer.get(), rapi::ResourceState::COPY_DST, targetBufferState));

	if (opt_gpuMarker)
		commandList->EndMarker();

	return m_CurrentSignal.GetListener();
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadTextureData(
	const st::WeakBlob& srcData, rapi::TextureHandle dstTexture, st::rapi::ResourceState currentState, st::rapi::ResourceState targetState,
	const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	uint64_t uploadBufferOffset = RequestUploadBufferSpace(srcData.size());
	if (uploadBufferOffset == c_FailedToReserveSpace)
	{
		// TODO: Can just happen that wwe are requestion too much uploads in single frame.
		// So we have to wait the completion of some in-flight commands to free space for new
		// TODO: Create a pending commands list that can be activated after space is freed (RunGarbageCollector)
		return std::unexpected("Failed to reserve memory in upload buffer");
	}
	void* uploadBufferMem = m_UploadBuffer->Map(uploadBufferOffset, srcData.size());
	if (!uploadBufferMem)
	{
		return std::unexpected("Failed to map upload buffer");
	}
	std::memcpy(uploadBufferMem, srcData.data(), srcData.size());
	m_UploadBuffer->Unmap(uploadBufferOffset, srcData.size());

	auto commandList = GetNewCommandList();

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadTextureData [{}]", opt_gpuMarker).c_str());

	const rapi::TextureDesc& desc = dstTexture->GetDesc();
	uint32_t lastArraySlice = subresources.GetLastArraySlice(desc);
	uint32_t lastMip = subresources.GetLastMipLevel(desc);

	// Transition to copy_dest
	if (subresources.IsEntireTexture(desc))
	{
		commandList->PushBarrier(
			rapi::Barrier::Texture(dstTexture.get(), currentState, rapi::ResourceState::COPY_DST));
	}
	else
	{
		std::vector<rapi::Barrier> barriers;
		barriers.reserve(desc.mipLevels * desc.arraySize);

		for (uint32_t arraySlice = subresources.baseArraySlice; arraySlice <= lastArraySlice; ++arraySlice)
		{
			for (int mip = subresources.baseMipLevel; mip <= lastMip; ++mip)
			{
				barriers.push_back(rapi::Barrier::Texture(
					dstTexture.get(), currentState, rapi::ResourceState::COPY_DST, mip, arraySlice));
			}
		}			

		commandList->PushBarriers(barriers);
	}

	commandList->WriteTexture(dstTexture.get(), subresources, m_UploadBuffer.get(), uploadBufferOffset);

	// Transition from copy_dest to target state
	if (subresources.IsEntireTexture(desc))
	{
		commandList->PushBarrier(
			rapi::Barrier::Texture(dstTexture.get(), rapi::ResourceState::COPY_DST, targetState));
	}
	else
	{
		std::vector<rapi::Barrier> barriers;
		barriers.reserve(desc.mipLevels * desc.arraySize);

		for (uint32_t arraySlice = subresources.baseArraySlice; arraySlice <= lastArraySlice; ++arraySlice)
		{
			for (int mip = subresources.baseMipLevel; mip <= lastMip; ++mip)
			{
				barriers.push_back(rapi::Barrier::Texture(
					dstTexture.get(), rapi::ResourceState::COPY_DST, targetState, mip, arraySlice));
			}
		}

		commandList->PushBarriers(barriers);
	}

	if (opt_gpuMarker)
		commandList->EndMarker();

	return m_CurrentSignal.GetListener();
}

void st::gfx::DataUploader::ProcessRenderingThreadCommands()
{
	std::scoped_lock lock{ m_ThreadLocalMutex };

	std::vector<rapi::CommandListHandle> commandListsToExecute;
	for (auto it = m_ThreadLocal.begin(); it != m_ThreadLocal.end(); ++it)
	{
		it->second->Close();
		commandListsToExecute.push_back(std::move(it->second));
	}
	m_ThreadLocal.clear();

	if (!commandListsToExecute.empty())
	{
		std::vector<rapi::ICommandList*> commandLists;
		commandLists.reserve(commandListsToExecute.size());
		for (auto& cl : commandListsToExecute)
			commandLists.push_back(cl.get());

		m_Device->ExecuteCommandLists(commandLists, rapi::QueueType::Graphics, m_CommitFence.get(), m_CommitCount);

		m_InFlightCommandLists.Push(InFlightCommandListEntry{
			m_CommitCount, std::move(commandListsToExecute), std::move(m_CurrentSignal) });

		++m_CommitCount;
	}
}

void st::gfx::DataUploader::RunGarbageCollector()
{
	std::vector<st::SignalEmitter> toSignal;
	uint64_t completedIdx = m_CommitFence->GetCompletedValue();

	// Command list completed
	{
		std::scoped_lock lock{ m_ThreadLocalMutex };
		while (!m_InFlightCommandLists.Empty() || m_InFlightCommandLists.Front().CompletedIdx <= completedIdx)
		{
			toSignal.push_back(std::move(m_InFlightCommandLists.Front().Signal));
			m_InFlightCommandLists.Pop();
		}
	}

	// Return space to upload buffer
	{
		std::scoped_lock lock{ m_UploadBufferMutex };
		while (!m_UploadBufferCompletion.Empty() || m_UploadBufferCompletion.Front().CompletedIdx <= completedIdx)
		{
			m_UploadBufferHead = m_UploadBufferCompletion.Front().BufferPosition;
			m_UploadBufferCompletion.Pop();
		}
	}

	for (auto& signal : toSignal)
	{
		signal.Signal();
	}
}

st::rapi::CommandListHandle st::gfx::DataUploader::GetNewCommandList()
{
	rapi::CommandListParams params{
		.queueType = rapi::QueueType::Graphics 
	};
	return m_Device->CreateCommandList(params);
}

uint64_t st::gfx::DataUploader::RequestUploadBufferSpace(size_t size)
{
	std::scoped_lock lock{ m_UploadBufferMutex };

	auto head = m_UploadBufferHead;
	auto tail = m_UploadBufferTail;
	bool found = false;
	if (tail >= head)
	{
		// Check if there is enough space from the tail to the end
		if (c_UploadBufferSize - tail >= size)
			found = true;
		else
			tail = 0; // Lets try with the tail at the begining
	}
	if (!found)
	{
		assert(head >= tail);
		if (head - tail >= size)
		{
			found = true;
		}
	}

	if (found)
	{
		m_UploadBufferTail = tail + size;

		// If there is no elements in the completion list or
		// the last alement added is older than current commit idx
		if (m_UploadBufferCompletion.Empty() || m_UploadBufferCompletion.Back().CompletedIdx < m_CommitCount)
		{
			m_UploadBufferCompletion.Push(UploadBufferCompletionEntry{ m_CommitCount, m_UploadBufferTail });
		}
		// else, we are uploading the same commit_idx, so just update the position
		else
		{
			assert(m_UploadBufferCompletion.Back().CompletedIdx == m_CommitCount);
			m_UploadBufferCompletion.Back().BufferPosition = m_UploadBufferTail;
		}

		return m_UploadBufferTail;
	}

	return c_FailedToReserveSpace;
}