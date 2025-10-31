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

st::gfx::DataUploader::ThreadLocalData::ThreadLocalData(rapi::Device* device)
{
	rapi::CommandListParams params{
		.queueType = rapi::QueueType::Graphics
	};
	CommandList = device->CreateCommandList(params);

	CommitIdx = 0;
}

// m_CommitCount starts with 1 since 0 is interpreted as not-initialized
st::gfx::DataUploader::DataUploader(rapi::Device* device) : 
	m_CommitCount{ 1 }, m_CommitFence{ device->CreateFence() }, m_Device { device }
{
	m_UploadBuffer = m_Device->CreateBuffer(rapi::BufferDesc{
		.usage = rapi::BufferUsage::UploadBuffer,
		.sizeBytes = c_UploadBufferSize
	});
	m_UploadBufferHead = 0;
	m_UploadBufferTail = 0;
	m_UploadBufferUsed = 0;
}

std::expected<st::SignalListener, std::string>  st::gfx::DataUploader::UploadBufferData(
	st::Blob&& srcData, st::rapi::BufferHandle dstBuffer, st::rapi::ResourceState dstCurrentBufferState, st::rapi::ResourceState dstBufferTargetState,
	size_t dstStart, int dstSize, const char* opt_gpuMarker)
{
	auto result = UploadBufferData(st::WeakBlob{ srcData }, dstBuffer, dstCurrentBufferState, dstBufferTargetState, dstStart, dstSize, opt_gpuMarker);
	if (result)
	{
		// Wait async to end to free srcData.
		std::thread([this, event = *result, capturedData = std::move(srcData)]() 
		{
			event.Wait();
		}).detach();
	}

	return result;
}

std::expected<st::SignalListener, std::string>  st::gfx::DataUploader::UploadBufferData(
	st::SharedBlob&& srcData, st::rapi::BufferHandle dstBuffer, st::rapi::ResourceState dstCurrentBufferState, st::rapi::ResourceState dstBufferTargetState,
	size_t dstStart, int dstSize, const char* opt_gpuMarker)
{
	auto result = UploadBufferData(st::WeakBlob{ srcData }, dstBuffer, dstCurrentBufferState, dstBufferTargetState, dstStart, dstSize, opt_gpuMarker);
	if (result)
	{
		// Wait async to end to free srcData.
		std::thread([this, event = *result, capturedData = std::move(srcData)]() 
		{
			event.Wait();
		}).detach();
	}

	return result;
}


std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadBufferData(
	const st::WeakBlob& srcData, st::rapi::BufferHandle dstBuffer, st::rapi::ResourceState dstCurrentBufferState, st::rapi::ResourceState dstBufferTargetState,
	size_t dstStart, const char* opt_gpuMarker)
{
	st::rapi::BufferDesc desc = dstBuffer->GetDesc();
	assert(dstStart + srcData.size() <= desc.sizeBytes);

	uint64_t uploadBufferOffset = RequestUploadBufferSpace(srcData.size());
	if (uploadBufferOffset == c_FailedToReserveSpace)
	{
		return std::unexpected("Failed to reserve memory in upload buffer");
	}
	void* uploadBufferMem = m_UploadBuffer->Map(uploadBufferOffset, srcData.size());
	if (!uploadBufferMem)
	{
		return std::unexpected("Failed to map upload buffer");
	}	
	std::memcpy(uploadBufferMem, srcData.data(), srcData.size());

	auto [threadLocal, future] = GetThreadLocalData();
	auto commandList = threadLocal->CommandList;

	if (opt_gpuMarker)
		commandList->BeginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());

	commandList->WriteBuffer(dstBuffer.get(), dstStart, m_UploadBuffer.get(), uploadBufferOffset, srcData.size());



	commandList->commitBarriers();

	if (opt_gpuMarker)
		commandList->endMarker();

	nvrhi::EventQueryHandle event = m_Device->createEventQuery();
	m_Device->setEventQuery(event, nvrhi::CommandQueue::Graphics);

	return future;
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadTextureData(
	st::Blob&& srcData, nvrhi::TextureHandle dstTexture, st::rapi::ResourceState currentTextureState, st::rapi::ResourceState textureTargetState,
	const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	auto result = UploadTextureData(st::WeakBlob{ srcData }, dstTexture, currentTextureState, textureTargetState, subresources, opt_gpuMarker);
	if (result)
	{
		// Wait async to end to free srcData.
		std::thread([this, event = *result, capturedData = std::move(srcData)]()
		{
			event.Wait();
		}).detach();
	}

	return result;
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadTextureData(
	st::SharedBlob&& srcData, nvrhi::TextureHandle dstTexture, st::rapi::ResourceState currentTextureState, st::rapi::ResourceState textureTargetState,
	const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	auto result = UploadTextureData(st::WeakBlob{ srcData }, dstTexture, currentTextureState, textureTargetState, subresources, opt_gpuMarker);
	if (result)
	{
		// Wait async to end to free srcData.
		std::thread([this, event = *result, capturedData = std::move(srcData)]()
		{
			event.Wait();
		}).detach();
	}

	return result;
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadTextureData(
	const st::WeakBlob& srcData, nvrhi::TextureHandle dstTexture, st::rapi::ResourceState currentTextureState, st::rapi::ResourceState textureTargetState,
	const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	auto [threadLocal, future] = GetThreadLocalData();
	auto commandList = threadLocal->CommandList;

	commandList->beginTrackingTextureState(dstTexture, {}, currentTextureState);

	if (opt_gpuMarker)
		commandList->beginMarker(std::format("UploadTextureData [{}]", opt_gpuMarker).c_str());

	nvrhi::TextureDesc desc = dstTexture->getDesc();

	ID3D12Device* d3d12Device = m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device);
	ID3D12Resource* d3d12Texture = dstTexture->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);

	uint64_t requiredSize = 0;
	D3D12_RESOURCE_DESC d3d12Desc = d3d12Texture->GetDesc();
	d3d12Device->GetCopyableFootprints(
		&d3d12Desc,
		subresources.baseArraySlice,
		subresources.numArraySlices == rapi::TextureSubresourceSet::AllArraySlices ? d3d12Desc.DepthOrArraySize : subresources.numArraySlices,
		0, nullptr, nullptr, nullptr, &requiredSize);
	assert(requiredSize == srcData.size());

	for (uint32_t arraySlice = 0; arraySlice < desc.arraySize; arraySlice++)
	{
		for (uint32_t mipLevel = 0; mipLevel < desc.mipLevels; mipLevel++)
		{
			const int rowPich = (desc.width >> mipLevel) * st::gfx::BitsPerPixel(desc.format) / 8;
			commandList->writeTexture(dstTexture, arraySlice, mipLevel, srcData.data(), rowPich);
		}
	}

	commandList->setPermanentTextureState(dstTexture, textureTargetState);
	commandList->commitBarriers();

	if (opt_gpuMarker)
		commandList->endMarker();

	return future;
}

void st::gfx::DataUploader::ProcessRenderingThreadCommands()
{
	std::scoped_lock lock{ m_ThreadLocalMutex };

	std::vector<nvrhi::ICommandList*> commandListsToExecute;
	for (auto it = m_ThreadLocal.begin(); it != m_ThreadLocal.end(); ++it)
	{
		ThreadLocalData* threadLocal = it->second.get();

		if (threadLocal->CommitIdx == m_CommitCount)
		{
			// Commit requested this frame.
			threadLocal->CommandList->close();
			commandListsToExecute.push_back(threadLocal->CommandList.Get());
		}
	}

	if (!commandListsToExecute.empty())
	{
		m_Device->executeCommandLists(commandListsToExecute.data(), commandListsToExecute.size(), nvrhi::CommandQueue::Graphics);

		ID3D12CommandQueue* queue = m_Device->getNativeQueue(nvrhi::ObjectTypes::D3D12_CommandQueue, nvrhi::CommandQueue::Graphics);
		queue->Signal(m_CommitFence, m_CommitCount);
		++m_CommitCount;
	}
}

void st::gfx::DataUploader::RunGarbageCollector()
{
	std::vector<st::SignalEmitter> toSignal;

	{
		std::scoped_lock lock{ m_ThreadLocalMutex };
		uint64_t completedIdx = m_CommitFence->GetCompletedValue();

		// Check wich thread local is not needed stale and can be removed
		std::vector<std::unordered_map<std::thread::id, std::unique_ptr<ThreadLocalData>>::iterator> toRemove;
		for (auto it = m_ThreadLocal.begin(); it != m_ThreadLocal.end(); ++it)
		{
			if (it->second->CommitIdx <= completedIdx)
				toRemove.push_back(it);
		}
		for (auto& it : toRemove)
		{
			m_ThreadLocal.erase(it);
		}

		// Signal completed data
		while (!m_Signals.Empty() && m_Signals.Front().CommitIdx <= completedIdx)
		{
			toSignal.emplace_back(std::move(m_Signals.Front().Signal));
			m_Signals.Pop();
		}
	}

	for (auto& signal : toSignal)
	{
		signal.Signal();
	}	
}

std::pair<st::gfx::DataUploader::ThreadLocalData*, st::SignalListener> st::gfx::DataUploader::GetThreadLocalData()
{
	std::scoped_lock lock{ m_ThreadLocalMutex };

	auto this_thread_id = std::this_thread::get_id();
	ThreadLocalData* threadLocal = nullptr;

	auto it = m_ThreadLocal.find(this_thread_id);
	if (it == m_ThreadLocal.end())
	{
		threadLocal = new ThreadLocalData{ m_Device };
		m_ThreadLocal.insert(std::pair<std::thread::id, std::unique_ptr<ThreadLocalData>>{ this_thread_id, threadLocal});
	}
	else
	{
		threadLocal = it->second.get();
	}

	if (threadLocal->CommitIdx != m_CommitCount)
	{
		// Since this is the first time we request this command buffer this frame, open it and keep it open until the final submit.
		threadLocal->CommandList->open();
		threadLocal->CommitIdx = m_CommitCount;
	}

	// Create the promise
	if (m_Signals.Empty() || m_Signals.Back().CommitIdx < m_CommitCount)
	{
		m_Signals.Push(SignalDataType{ m_CommitCount, {} });
	}
	assert(m_Signals.Back().CommitIdx == m_CommitCount);

	return { threadLocal, m_Signals.Back().Signal.GetListener() };
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
		return m_UploadBufferTail;
	}
	return c_FailedToReserveSpace;
}