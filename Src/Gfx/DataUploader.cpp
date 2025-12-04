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
	m_CommitCount{ 1 }, m_Device { device }
{
	m_UploadBuffer = m_Device->CreateBuffer(rapi::BufferDesc{
		.memoryAccess = rapi::MemoryAccess::Upload,
		.shaderUsage = rapi::ShaderUsage::None,
		.sizeBytes = c_UploadBufferSize,
		.debugName = "UploadBuffer"
	}, rapi::ResourceState::COMMON);

	m_CommitFence = m_Device->CreateFence(0, "DataUploader fence");

	m_UploadBufferHead = 0;
	m_UploadBufferTail = 0;

	m_ProcessInFlightCommandsThread = std::thread([this] { this->AsyncUpdate(); });
}

st::gfx::DataUploader::~DataUploader()
{
	// Finish async thread
	{
		m_AsyncShutdown = true;
		m_InFlightCommandListsCV.notify_one();
		m_ProcessInFlightCommandsThread.join();
	}

	assert(m_UploadBufferHead == m_UploadBufferTail);
	assert(m_InFlightCommandLists.Empty());

	m_Device->ReleaseImmediately(m_CommitFence);
	m_Device->ReleaseImmediately(m_UploadBuffer);
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

	auto commandList = GetCommandList();

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());

	if (currentBufferState != rapi::ResourceState::COPY_DST)
	{
		commandList->PushBarrier(
			rapi::Barrier::Buffer(dstBuffer.get(), currentBufferState, rapi::ResourceState::COPY_DST));
	}

	commandList->WriteBuffer(dstBuffer.get(), dstStart, m_UploadBuffer.get(), uploadBufferOffset, srcData.size());

	if (targetBufferState != rapi::ResourceState::COPY_DST)
	{
		commandList->PushBarrier(
			rapi::Barrier::Buffer(dstBuffer.get(), rapi::ResourceState::COPY_DST, targetBufferState));
	}

	if (opt_gpuMarker)
		commandList->EndMarker();

	return FinishCommandList(commandList);
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

	auto commandList = GetCommandList();

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadTextureData [{}]", opt_gpuMarker).c_str());

	const rapi::TextureDesc& desc = dstTexture->GetDesc();
	uint32_t lastArraySlice = subresources.GetLastArraySlice(desc);
	uint32_t lastMip = subresources.GetLastMipLevel(desc);

	// Transition to copy_dest
	if (currentState != rapi::ResourceState::COPY_DST)
	{
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
	}

	commandList->WriteTexture(dstTexture.get(), subresources, m_UploadBuffer.get(), uploadBufferOffset);

	// Transition from copy_dest to target state
	if (targetState != rapi::ResourceState::COPY_DST)
	{
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
	}

	if (opt_gpuMarker)
		commandList->EndMarker();

	return FinishCommandList(commandList);
}

void st::gfx::DataUploader::RunGarbageCollector()
{
	std::vector<st::SignalEmitter> toSignal;
	uint64_t completedIdx = m_CommitFence->GetCompletedValue();

	// Command list completed
	{
		while (!m_InFlightCommandLists.Empty() && m_InFlightCommandLists.Front().CompletedIdx <= completedIdx)
		{
			toSignal.push_back(std::move(m_InFlightCommandLists.Front().Signal));
			m_InFlightCommandLists.Pop();
		}
	}

	// Return space to upload buffer
	{
		std::scoped_lock lock{ m_UploadBufferMutex };
		while (!m_UploadBufferCompletion.Empty() && m_UploadBufferCompletion.Front().CompletedIdx <= completedIdx)
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

st::rapi::CommandListHandle st::gfx::DataUploader::GetCommandList()
{
	rapi::CommandListParams params{
		.queueType = rapi::QueueType::Graphics,
		.debugName = "DataUploader CommandList"
	};
	auto commandList = m_Device->CreateCommandList(params);
	commandList->Open();

	return commandList;
}

st::SignalListener st::gfx::DataUploader::FinishCommandList(rapi::CommandListHandle commandList)
{
	commandList->Close();

	st::SignalListener signal;
	{
		std::lock_guard lock{ m_InFlightCommandListsMutex };

		m_Device->ExecuteCommandList(commandList.get(), rapi::QueueType::Graphics, m_CommitFence.get(), m_CommitCount);

		m_InFlightCommandLists.Push(InFlightCommandListEntry{
			m_CommitCount, commandList, {} });

		signal = m_InFlightCommandLists.Back().Signal.GetListener();
		++m_CommitCount;
	}
	m_InFlightCommandListsCV.notify_one();

	return signal;
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
		uint64_t retOffset = m_UploadBufferTail;
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

		return retOffset;
	}

	return c_FailedToReserveSpace;
}

void st::gfx::DataUploader::AsyncUpdate()
{
	std::unique_lock<std::mutex> lock(m_InFlightCommandListsMutex);

	while (true)
	{
		m_InFlightCommandListsCV.wait(lock, [this] 
		{
			return m_AsyncShutdown || !m_InFlightCommandLists.Empty();
		});

		while (!m_InFlightCommandLists.Empty())
		{
			std::vector<SignalEmitter> toSignal;
			std::vector<rapi::CommandListHandle> commandLists;
			uint64_t completedIdx = m_CommitFence->GetCompletedValue();

			while (!m_InFlightCommandLists.Empty() && m_InFlightCommandLists.Front().CompletedIdx <= completedIdx)
			{
				toSignal.push_back(std::move(m_InFlightCommandLists.Front().Signal));
				commandLists.push_back(m_InFlightCommandLists.Front().CommandList);
				m_InFlightCommandLists.Pop();
			}

			lock.unlock();

			for (auto& cmdList : commandLists)
			{
				m_Device->ReleaseImmediately(cmdList);
			}

			if (!toSignal.empty())
			{
				// Return space to upload buffer
				{
					std::scoped_lock lock{ m_UploadBufferMutex };
					while (!m_UploadBufferCompletion.Empty() && m_UploadBufferCompletion.Front().CompletedIdx <= completedIdx)
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

			lock.lock();

			std::this_thread::yield();
		}

		if (m_AsyncShutdown)
			break;
	}
}