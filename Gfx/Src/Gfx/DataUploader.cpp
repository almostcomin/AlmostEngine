#include "Gfx/DataUploader.h"
#include "Gfx/Util.h"
#include "RHI/Device.h"

namespace
{
	static constexpr size_t c_UploadBufferSize = st::MiB(256);
	static constexpr uint64_t c_FailedToReserveSpace = std::numeric_limits<uint64_t>::max();
}

// m_CommitCount starts with 1 since 0 is interpreted as not-initialized
st::gfx::DataUploader::DataUploader(rhi::Device* device) : 
	m_CommitCount{ 1 }, m_Device { device }
{
	m_UploadBuffer = m_Device->CreateBuffer(rhi::BufferDesc{
		.memoryAccess = rhi::MemoryAccess::Upload,
		.shaderUsage = rhi::BufferShaderUsage::None,
		.sizeBytes = c_UploadBufferSize
	}, rhi::ResourceState::COMMON, "UploadBuffer");

	m_CommitFence = m_Device->CreateFence(0, "DataUploader fence");

	m_UploadBufferHead = 0;
	m_UploadBufferTail = 0;

	m_NextTicketIdx = 1;
	m_CompletedTicketIdx = 0;

	m_ProcessInFlightCommandsThread = std::thread([this] { this->AsyncUpdate(); });
}

st::gfx::DataUploader::~DataUploader()
{
	// Finish async thread
	{
		m_AsyncShutdown = true;
		m_InFlightCommandListsCV.notify_all();
		m_ProcessInFlightCommandsThread.join();
	}

	assert(m_UploadBufferHead == m_UploadBufferTail);
	assert(m_InFlightCommandLists.Empty());

	m_Device->ReleaseImmediately(m_CommitFence);
	m_Device->ReleaseImmediately(m_UploadBuffer);
}

std::expected<st::gfx::DataUploader::UploadTicket, std::string> st::gfx::DataUploader::RequestUploadTicket(const rhi::BufferDesc& desc)
{
	auto storageReq = m_Device->GetStorageRequirements(desc);
	return RequestUploadTicket(storageReq.size, storageReq.alignment);
}

std::expected<st::gfx::DataUploader::UploadTicket, std::string> st::gfx::DataUploader::RequestUploadTicket(const rhi::TextureDesc& desc)
{
	auto storageReq = m_Device->GetCopyableRequirements(desc);
	return RequestUploadTicket(storageReq.size, storageReq.alignment);
}

std::expected<st::gfx::DataUploader::UploadTicket, std::string> st::gfx::DataUploader::RequestUploadTicket(size_t size, size_t alignment)
{
	assert(alignment > 0);

	std::scoped_lock lock{ m_UploadBufferMutex };
	
	auto head = m_UploadBufferHead;
	auto tail = m_UploadBufferTail;
	uint64_t startTail = UINT64_MAX;
	auto sizeNeeded = size;
	
	bool found = false;
	// Case 1: tail >= head, try tail to end
	if (tail >= head)
	{
		sizeNeeded = AlignUp(tail, alignment) - tail + size;
		// Check if there is enough space from the tail to the end
		if (tail + sizeNeeded <= c_UploadBufferSize)
		{
			startTail = tail;
		}
		else
		{
			// Try wrap to beginning
			auto tail = 0;
			sizeNeeded = size; // Since 0 is aligned
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
		return std::unexpected("Not enough space in upload buffer");
	}


	uint64_t alignedStart = AlignUp(startTail, alignment);
	m_UploadBufferTail = startTail + sizeNeeded;

	void* ptr = m_UploadBuffer->Map(alignedStart, size);
	if (!ptr)
	{
		LOG_ERROR("Failed to map upload buffer");
		return std::unexpected("Failed to map upload buffer");
	}

	return UploadTicket{ ptr, startTail, startTail + sizeNeeded, alignedStart, size, m_NextTicketIdx++ };
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::CommitUploadBufferTicket(UploadTicket&& ticket, rhi::BufferHandle dstBuffer,
	rhi::ResourceState currentBufferState, rhi::ResourceState targetBufferState, size_t dstStart, const char* opt_gpuMarker)
{
	m_UploadBuffer->Unmap(ticket.aligned_start, ticket.size);

	auto commandList = GetCommandList();

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());

	if (currentBufferState != rhi::ResourceState::COPY_DST)
	{
		commandList->PushBarrier(
			rhi::Barrier::Buffer(dstBuffer.get(), currentBufferState, rhi::ResourceState::COPY_DST));
	}

	commandList->WriteBuffer(dstBuffer.get(), dstStart, m_UploadBuffer.get(), ticket.aligned_start, ticket.size);

	if (targetBufferState != rhi::ResourceState::COPY_DST)
	{
		commandList->PushBarrier(
			rhi::Barrier::Buffer(dstBuffer.get(), rhi::ResourceState::COPY_DST, targetBufferState));
	}

	if (opt_gpuMarker)
		commandList->EndMarker();

	return FinishCommandList(std::move(commandList), std::move(ticket));
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::CommitUploadTextureTicket(UploadTicket&& ticket, rhi::TextureHandle dstTexture,
	rhi::ResourceState currentState, rhi::ResourceState targetState, const rhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	m_UploadBuffer->Unmap(ticket.aligned_start, ticket.size);

	auto commandList = GetCommandList();

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadTextureData [{}]", opt_gpuMarker).c_str());

	const rhi::TextureDesc& desc = dstTexture->GetDesc();
	uint32_t lastArraySlice = subresources.GetLastArraySlice(desc);
	uint32_t lastMip = subresources.GetLastMipLevel(desc);

	// Transition to copy_dest
	if (currentState != rhi::ResourceState::COPY_DST)
	{
		if (subresources.IsEntireTexture(desc))
		{
			commandList->PushBarrier(
				rhi::Barrier::Texture(dstTexture.get(), currentState, rhi::ResourceState::COPY_DST));
		}
		else
		{
			std::vector<rhi::Barrier> barriers;
			barriers.reserve(desc.mipLevels * desc.arraySize);

			for (uint32_t arraySlice = subresources.baseArraySlice; arraySlice <= lastArraySlice; ++arraySlice)
			{
				for (int mip = subresources.baseMipLevel; mip <= lastMip; ++mip)
				{
					barriers.push_back(rhi::Barrier::Texture(
						dstTexture.get(), currentState, rhi::ResourceState::COPY_DST, mip, arraySlice));
				}
			}

			commandList->PushBarriers(barriers);
		}
	}

	commandList->WriteTexture(dstTexture.get(), subresources, m_UploadBuffer.get(), ticket.aligned_start);

	// Transition from copy_dest to target state
	if (targetState != rhi::ResourceState::COPY_DST)
	{
		if (subresources.IsEntireTexture(desc))
		{
			commandList->PushBarrier(
				rhi::Barrier::Texture(dstTexture.get(), rhi::ResourceState::COPY_DST, targetState));
		}
		else
		{
			std::vector<rhi::Barrier> barriers;
			barriers.reserve(desc.mipLevels * desc.arraySize);

			for (uint32_t arraySlice = subresources.baseArraySlice; arraySlice <= lastArraySlice; ++arraySlice)
			{
				for (int mip = subresources.baseMipLevel; mip <= lastMip; ++mip)
				{
					barriers.push_back(rhi::Barrier::Texture(
						dstTexture.get(), rhi::ResourceState::COPY_DST, targetState, mip, arraySlice));
				}
			}

			commandList->PushBarriers(barriers);
		}
	}

	if (opt_gpuMarker)
		commandList->EndMarker();

	return FinishCommandList(std::move(commandList), std::move(ticket));
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadBufferData(
	const st::WeakBlob& srcData, st::rhi::BufferHandle dstBuffer, st::rhi::ResourceState currentBufferState, st::rhi::ResourceState targetBufferState,
	size_t dstStart, const char* opt_gpuMarker)
{
	st::rhi::BufferDesc desc = dstBuffer->GetDesc();
	assert(dstStart + srcData.size() <= desc.sizeBytes);

	auto ticket = RequestUploadTicket(srcData.size(), m_Device->GetCopyDataAlignment(rhi::CopyMethod::Buffer2Buffer));
	if (!ticket)
		return std::unexpected(ticket.error());

	std::memcpy(ticket->ptr, srcData.data(), srcData.size());

	auto uploadResult = CommitUploadBufferTicket(std::move(*ticket), dstBuffer, currentBufferState, targetBufferState, dstStart, opt_gpuMarker);
	if (!uploadResult)
		return std::unexpected(uploadResult.error());

	return *uploadResult;
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadTextureData(
	const st::WeakBlob& srcData, rhi::TextureHandle dstTexture, st::rhi::ResourceState currentState, st::rhi::ResourceState targetState,
	const rhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	const auto& texDesc = dstTexture->GetDesc();
	rhi::FormatInfo formatInfo = GetFormatInfo(texDesc.format);
	auto copyReq = m_Device->GetCopyableRequirements(texDesc);
	assert(copyReq.size >= srcData.size());

	auto ticket = RequestUploadTicket(copyReq.size, copyReq.alignment);
	if (!ticket)
		return std::unexpected(ticket.error());

	// Perform copy
	size_t srcOffset = 0;
	size_t dstOffset = 0;
	for (auto arraySlice = subresources.baseArraySlice; arraySlice < subresources.GetNumArraySlices(texDesc); ++arraySlice)
	{
		for (auto mipLevel = subresources.baseMipLevel; mipLevel < subresources.GetNumMipLevels(texDesc); ++mipLevel)
		{
			size_t width = texDesc.width >> mipLevel / formatInfo.blockSize;
			size_t srcRowSize = width * formatInfo.bytesPerBlock;

			rhi::SubresourceCopyableRequirements req = m_Device->GetSubresourceCopyableRequirements(texDesc, mipLevel, arraySlice);
			assert(srcRowSize == req.rowSizeBytes);
			dstOffset = req.offset;
			for (size_t row = 0; row < req.numRows; ++row)
			{
				std::memcpy((char*)ticket->ptr + dstOffset, srcData.data() + srcOffset, srcRowSize);
				srcOffset += srcRowSize;
				dstOffset += req.rowStride;
			}
		}
	}

	auto uploadResult = CommitUploadTextureTicket(std::move(*ticket), dstTexture, currentState, targetState, subresources, opt_gpuMarker);
	if (!uploadResult)
		return std::unexpected(uploadResult.error());

	return *uploadResult;
}

st::rhi::CommandListOwner st::gfx::DataUploader::GetCommandList()
{
	rhi::CommandListParams params{
		.queueType = rhi::QueueType::Graphics
	};
	auto commandList = m_Device->CreateCommandList(params, "DataUploader CommandList");
	commandList->Open();

	return commandList;
}

st::SignalListener st::gfx::DataUploader::FinishCommandList(rhi::CommandListOwner&& commandList, UploadTicket&& ticket)
{
	commandList->Close();

	st::SignalListener signal;
	{
		std::unique_lock lock{ m_InFlightCommandListsMutex };

		m_InFlightCommandListsCV.wait(lock, [&]
		{
			return !m_InFlightCommandLists.Full();
		});

		m_Device->ExecuteCommandList(commandList.get(), rhi::QueueType::Graphics, m_CommitFence.get(), m_CommitCount);

		m_InFlightCommandLists.Push(InFlightCommandListEntry{
			m_CommitCount, std::move(commandList), {}, std::move(ticket) });

		signal = m_InFlightCommandLists.Back().Signal.GetListener();
		++m_CommitCount;
	}
	m_InFlightCommandListsCV.notify_all();

	return signal;
}

void st::gfx::DataUploader::InsertPendingTicket(UploadTicket&& ticket)
{
	auto it = std::lower_bound(m_PendingTickets.begin(), m_PendingTickets.end(), ticket.idx, [](const UploadTicket& a, uint64_t idx)
		{
			return a.idx < idx;
		});
	m_PendingTickets.insert(it, std::move(ticket));
}

void st::gfx::DataUploader::OnCompletedTicket(UploadTicket&& ticket)
{
	std::scoped_lock lock{ m_UploadBufferMutex };

	if (ticket.idx == m_CompletedTicketIdx + 1)
	{
		m_UploadBufferHead = ticket.end;
		m_CompletedTicketIdx++;
		while (!m_PendingTickets.empty() && m_PendingTickets.begin()->idx == (m_CompletedTicketIdx + 1))
		{
			m_UploadBufferHead = m_PendingTickets.begin()->end;
			m_PendingTickets.erase(m_PendingTickets.begin());
			m_CompletedTicketIdx++;
		}
	}
	else
	{
		InsertPendingTicket(std::move(ticket));
	}

	if (m_UploadBufferHead == m_UploadBufferTail && m_PendingTickets.empty())
	{
		//m_UploadBufferHead = m_UploadBufferTail = 0;
	}
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
			std::vector<InFlightCommandListEntry> completedOnes;
			completedOnes.reserve(m_InFlightCommandLists.Size());

			uint64_t completedIdx = m_CommitFence->GetCompletedValue();

			while (!m_InFlightCommandLists.Empty() && m_InFlightCommandLists.Front().CompletedIdx <= completedIdx)
			{
				completedOnes.push_back(m_InFlightCommandLists.Pop().value());
			}

			m_InFlightCommandListsCV.notify_all();
			lock.unlock();

			for (auto& c : completedOnes)
			{
				m_Device->ReleaseImmediately(c.CommandList); // no need to queue the release
				OnCompletedTicket(std::move(c.Ticket));
				c.Signal.Signal();
			}

			lock.lock();
			std::this_thread::yield();
		}

		if (m_AsyncShutdown)
			break;
	}
}