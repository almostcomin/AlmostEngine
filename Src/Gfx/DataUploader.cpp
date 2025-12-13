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
		.shaderUsage = rapi::BufferShaderUsage::None,
		.sizeBytes = c_UploadBufferSize,
		.debugName = "UploadBuffer"
	}, rapi::ResourceState::COMMON);

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
		m_InFlightCommandListsCV.notify_one();
		m_ProcessInFlightCommandsThread.join();
	}

	assert(m_UploadBufferHead == m_UploadBufferTail);
	assert(m_InFlightCommandLists.Empty());

	m_Device->ReleaseImmediately(m_CommitFence);
	m_Device->ReleaseImmediately(m_UploadBuffer);
}

std::expected<st::gfx::DataUploader::UploadTicket, std::string> st::gfx::DataUploader::RequestUploadTicket(const rapi::BufferDesc& desc)
{
	auto storageReq = m_Device->GetStorageRequirements(desc);
	return RequestUploadTicket(storageReq.size, storageReq.alignment);
}

std::expected<st::gfx::DataUploader::UploadTicket, std::string> st::gfx::DataUploader::RequestUploadTicket(const rapi::TextureDesc& desc)
{
	auto storageReq = m_Device->GetCopyableRequirements(desc);
	return RequestUploadTicket(storageReq.size, storageReq.alignment);
}

std::expected<st::gfx::DataUploader::UploadTicket, std::string> st::gfx::DataUploader::RequestUploadTicket(size_t size, size_t alignment)
{
	std::scoped_lock lock{ m_UploadBufferMutex };

	auto head = m_UploadBufferHead;
	auto tail = m_UploadBufferTail;
	auto reqSize = size;
	
	bool found = false;
	if (tail >= head)
	{
		reqSize = size + AlignUp(tail, alignment) - tail;
		// Check if there is enough space from the tail to the end
		if (c_UploadBufferSize - tail >= reqSize)
			found = true;
		else
		{
			tail = 0; // Lets try with the tail at the begining
			reqSize = size; // Since the buffer is aligned to 64kb and thats the max aligned we allow
		}
	}
	if (!found)
	{
		assert(head >= tail);
		if (head - tail >= reqSize)
		{
			found = true;
		}
	}

	if (found)
	{
		uint64_t retOffset = tail;
		m_UploadBufferTail = tail + reqSize;

		void* ptr = m_UploadBuffer->Map(retOffset + AlignUp(retOffset, alignment), size);
		if (!ptr)
		{
			LOG_ERROR("Failed to map upload buffer");
			return std::unexpected("Failed to map upload buffer");
		}

		return UploadTicket{ ptr, retOffset, reqSize, m_NextTicketIdx++ };
	}

	LOG_ERROR("Not enough space in upload buffer, requested: {}", size);
	return std::unexpected("Not enough space in upload buffer");
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::CommitUploadBufferTicket(UploadTicket&& ticket, rapi::BufferHandle dstBuffer,
	rapi::ResourceState currentBufferState, rapi::ResourceState targetBufferState, size_t dstStart, const char* opt_gpuMarker)
{
	m_UploadBuffer->Unmap(ticket.start, ticket.size);

	auto commandList = GetCommandList();

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());

	if (currentBufferState != rapi::ResourceState::COPY_DST)
	{
		commandList->PushBarrier(
			rapi::Barrier::Buffer(dstBuffer.get(), currentBufferState, rapi::ResourceState::COPY_DST));
	}

	commandList->WriteBuffer(dstBuffer.get(), dstStart, m_UploadBuffer.get(), ticket.start, ticket.size);

	if (targetBufferState != rapi::ResourceState::COPY_DST)
	{
		commandList->PushBarrier(
			rapi::Barrier::Buffer(dstBuffer.get(), rapi::ResourceState::COPY_DST, targetBufferState));
	}

	if (opt_gpuMarker)
		commandList->EndMarker();

	return FinishCommandList(commandList, std::move(ticket));
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::CommitUploadTextureTicket(UploadTicket&& ticket, rapi::TextureHandle dstTexture,
	rapi::ResourceState currentState, rapi::ResourceState targetState, const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	m_UploadBuffer->Unmap(ticket.start, ticket.size);

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

	commandList->WriteTexture(dstTexture.get(), subresources, m_UploadBuffer.get(), ticket.start);

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

	return FinishCommandList(commandList, std::move(ticket));
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadBufferData(
	const st::WeakBlob& srcData, st::rapi::BufferHandle dstBuffer, st::rapi::ResourceState currentBufferState, st::rapi::ResourceState targetBufferState,
	size_t dstStart, const char* opt_gpuMarker)
{
	st::rapi::BufferDesc desc = dstBuffer->GetDesc();
	assert(dstStart + srcData.size() <= desc.sizeBytes);

	auto ticket = RequestUploadTicket(srcData.size(), m_Device->GetCopyDataAlignment(rapi::CopyMethod::Buffer2Buffer));
	if (!ticket)
		return std::unexpected(ticket.error());

	std::memcpy(ticket->ptr, srcData.data(), srcData.size());

	auto uploadResult = CommitUploadBufferTicket(std::move(*ticket), dstBuffer, currentBufferState, targetBufferState, dstStart, opt_gpuMarker);
	if (!uploadResult)
		return std::unexpected(uploadResult.error());

	return *uploadResult;
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadTextureData(
	const st::WeakBlob& srcData, rapi::TextureHandle dstTexture, st::rapi::ResourceState currentState, st::rapi::ResourceState targetState,
	const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	auto copyReq = m_Device->GetCopyableRequirements(dstTexture->GetDesc());
	assert(copyReq.size == srcData.size());

	auto ticket = RequestUploadTicket(srcData.size(), copyReq.alignment);
	if (!ticket)
		return std::unexpected(ticket.error());

	std::memcpy(ticket->ptr, srcData.data(), srcData.size());

	auto uploadResult = CommitUploadTextureTicket(std::move(*ticket), dstTexture, currentState, targetState, subresources, opt_gpuMarker);
	if (!uploadResult)
		return std::unexpected(uploadResult.error());

	return *uploadResult;
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

st::SignalListener st::gfx::DataUploader::FinishCommandList(rapi::CommandListHandle commandList, UploadTicket&& ticket)
{
	commandList->Close();

	st::SignalListener signal;
	{
		std::lock_guard lock{ m_InFlightCommandListsMutex };

		m_Device->ExecuteCommandList(commandList.get(), rapi::QueueType::Graphics, m_CommitFence.get(), m_CommitCount);

		m_InFlightCommandLists.Push(InFlightCommandListEntry{
			m_CommitCount, commandList, {}, std::move(ticket) });

		signal = m_InFlightCommandLists.Back().Signal.GetListener();
		++m_CommitCount;
	}
	m_InFlightCommandListsCV.notify_one();

	return signal;
}

void st::gfx::DataUploader::OnCompletedTiket(UploadTicket&& ticket)
{
	std::scoped_lock lock{ m_UploadBufferMutex };

	if (ticket.idx == m_CompletedTicketIdx + 1)
	{
		m_UploadBufferHead = ticket.start + ticket.size;
		m_CompletedTicketIdx++;
		while (!m_PendingTickets.empty() && m_PendingTickets.begin()->first == (m_CompletedTicketIdx + 1))
		{
			m_UploadBufferHead = m_PendingTickets.begin()->second.start + m_PendingTickets.begin()->second.size;
			m_CompletedTicketIdx++;
			m_PendingTickets.erase(m_PendingTickets.begin());
		}
	}
	else
	{
		m_PendingTickets.emplace(ticket.idx, std::move(ticket));
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
			//std::vector<SignalEmitter> toSignal;
			//std::vector<rapi::CommandListHandle> commandLists;

			std::vector<InFlightCommandListEntry> completedOnes;
			completedOnes.reserve(m_InFlightCommandLists.Size());

			uint64_t completedIdx = m_CommitFence->GetCompletedValue();

			while (!m_InFlightCommandLists.Empty() && m_InFlightCommandLists.Front().CompletedIdx <= completedIdx)
			{
				completedOnes.push_back(m_InFlightCommandLists.Pop().value());
			}

			lock.unlock();

			for (auto& c : completedOnes)
			{
				m_Device->ReleaseImmediately(c.CommandList); // no need to queue the release
				OnCompletedTiket(std::move(c.Ticket));
				c.Signal.Signal();
			}

			lock.lock();
			std::this_thread::yield();
		}

		if (m_AsyncShutdown)
			break;
	}
}