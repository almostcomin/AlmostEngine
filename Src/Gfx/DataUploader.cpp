#include "Gfx/DataUploader.h"
#include "Core/Log.h"
#include "Gfx/Util.h"
#include <nvrhi/d3d12.h>

st::gfx::DataUploader::ThreadLocalData::ThreadLocalData(nvrhi::DeviceHandle device)
{
	nvrhi::CommandListParameters params{
		.enableImmediateExecution = false
	};
	CommandList = device->createCommandList(params);

	CommitIdx = 0;
}

// m_CommitCount starts with 1 since 0 is interpreted as not-initialized
st::gfx::DataUploader::DataUploader(nvrhi::DeviceHandle device) : m_CommitCount { 1 }, m_Device{ device }
{
	ID3D12Device* d3d12Device = m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device);
	HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_CommitFence));
	assert(SUCCEEDED(hr));
}

std::expected<st::SignalListener, std::string>  st::gfx::DataUploader::UploadBufferData(
	st::Blob&& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
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
	st::SharedBlob&& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
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
	const st::WeakBlob& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
	size_t dstStart, int dstSize, const char* opt_gpuMarker)
{
	nvrhi::BufferDesc desc = dstBuffer->getDesc();

	if (dstSize < 0)
	{
		dstSize = desc.byteSize - dstStart;
	}

	assert(dstStart < desc.byteSize);
	assert(dstSize <= desc.byteSize - dstStart);

	auto [threadLocal, future] = GetThreadLocalData();
	auto commandList = threadLocal->CommandList;

	//commandList->open();
	commandList->beginTrackingBufferState(dstBuffer, dstCurrentBufferState);

	if (opt_gpuMarker)
		commandList->beginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());

	commandList->writeBuffer(dstBuffer, srcData.data(), dstSize, dstStart);

	commandList->setPermanentBufferState(dstBuffer, dstBufferTargetState);

	commandList->commitBarriers();

	if (opt_gpuMarker)
		commandList->endMarker();

	nvrhi::EventQueryHandle event = m_Device->createEventQuery();
	m_Device->setEventQuery(event, nvrhi::CommandQueue::Graphics);

	return future;
}

std::expected<st::SignalListener, std::string> st::gfx::DataUploader::UploadTextureData(
	st::Blob&& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
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
	st::SharedBlob&& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
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
	const st::WeakBlob& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
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