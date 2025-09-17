#include "Gfx/DataUploader.h"
#include <format>
#include <future>

st::gfx::DataUploader::DataUploader(nvrhi::DeviceHandle device) : m_Device(device)
{
}

bool st::gfx::DataUploader::Init()
{
	nvrhi::CommandListParameters params;
	params.queueType = nvrhi::CommandQueue::Graphics;

	m_CommandList = m_Device->createCommandList(params);

	return true;
}

std::expected<nvrhi::EventQueryHandle, std::string>  st::gfx::DataUploader::UploadData(
	nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState, 
	std::vector<uint8_t>&& srcData, size_t start, int dataSize, const char* opt_gpuMarker)
{
	std::scoped_lock lock{ m_UploadMutex };

	m_CommandList->open();
	m_CommandList->beginTrackingBufferState(dstBuffer, dstCurrentBufferState);

	nvrhi::BufferDesc desc = dstBuffer->getDesc();

	if (dataSize < 0)
	{
		dataSize = desc.byteSize - start;
	}

	assert(start < desc.byteSize);
	assert(dataSize <= desc.byteSize - start);

	if (opt_gpuMarker)
	{
		m_CommandList->beginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());
	}
	else
	{
		m_CommandList->beginMarker("UploadBufferData");
	}

	m_CommandList->writeBuffer(dstBuffer, srcData.data(), dataSize, start);

	m_CommandList->setPermanentBufferState(dstBuffer, dstBufferTargetState);

	m_CommandList->commitBarriers();

	m_CommandList->endMarker();

	m_CommandList->close();
	m_Device->executeCommandList(m_CommandList, nvrhi::CommandQueue::Graphics);

	nvrhi::EventQueryHandle event = m_Device->createEventQuery();
	m_Device->setEventQuery(event, nvrhi::CommandQueue::Graphics);

	// Wait async to end to free srcData.
	std::thread([this, event, srcData = std::move(srcData)]() {
		m_Device->waitEventQuery(event);
	}).detach();

	return event;
}