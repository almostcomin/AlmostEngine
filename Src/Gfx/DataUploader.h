#pragma once

#include <expected>
#include <mutex>
#include <nvrhi/nvrhi.h>

namespace st::gfx
{

class DataUploader
{
public:

	DataUploader(nvrhi::DeviceHandle device);

	bool Init();

	std::expected<nvrhi::EventQueryHandle, std::string> UploadData(
		nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState, 
		std::vector<uint8_t>&& srcData, size_t start, int dataSize, const char* opt_gpuMarker = nullptr);

private:

	nvrhi::CommandListHandle m_CommandList;
	std::mutex m_UploadMutex;

	nvrhi::DeviceHandle m_Device;
};

}