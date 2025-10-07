#include "Gfx/DataUploader.h"
#include "Core/Log.h"
#include <nvrhi/d3d12.h>

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

std::expected<nvrhi::EventQueryHandle, std::string>  st::gfx::DataUploader::UploadBufferData(
	st::Blob&& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
	size_t dstStart, int dstSize, const char* opt_gpuMarker)
{
	auto result = UploadBufferDataInternal(srcData, dstBuffer, dstCurrentBufferState, dstBufferTargetState, dstStart, dstSize, opt_gpuMarker);
	if (result)
	{
		// Wait async to end to free srcData.
		std::thread([this, event = *result, capturedData = std::move(srcData)]() 
		{
			m_Device->waitEventQuery(event);
		}).detach();
	}

	return result;
}

std::expected<nvrhi::EventQueryHandle, std::string> st::gfx::DataUploader::UploadBufferData(
	const st::WeakBlob& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
	size_t dstStart, int dstSize, const char* opt_gpuMarker)
{
	return UploadBufferDataInternal(srcData, dstBuffer, dstCurrentBufferState, dstBufferTargetState, dstStart, dstSize, opt_gpuMarker);
}

std::expected<nvrhi::EventQueryHandle, std::string> st::gfx::DataUploader::UploadTextureData(
	const st::WeakBlob& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
	const nvrhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	return UploadTextureDataInternal(srcData, dstTexture, currentTextureState, textureTargetState, subresources, opt_gpuMarker);
}

std::expected<nvrhi::EventQueryHandle, std::string> st::gfx::DataUploader::UploadBufferDataInternal(
	const st::IBlob& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState, 
	size_t dstStart, int dstSize, const char* opt_gpuMarker)
{
	nvrhi::BufferDesc desc = dstBuffer->getDesc();

	if (dstSize < 0)
	{
		dstSize = desc.byteSize - dstStart;
	}

	assert(dstStart < desc.byteSize);
	assert(dstSize <= desc.byteSize - dstStart);

	std::scoped_lock lock{ m_UploadMutex };

	m_CommandList->open();
	m_CommandList->beginTrackingBufferState(dstBuffer, dstCurrentBufferState);

	if (opt_gpuMarker)
	{
		m_CommandList->beginMarker(std::format("UploadBufferData [{}]", opt_gpuMarker).c_str());
	}
	else
	{
		m_CommandList->beginMarker("UploadBufferData");
	}

	m_CommandList->writeBuffer(dstBuffer, srcData.data(), dstSize, dstStart);

	m_CommandList->setPermanentBufferState(dstBuffer, dstBufferTargetState);

	m_CommandList->commitBarriers();

	m_CommandList->endMarker();

	m_CommandList->close();
	m_Device->executeCommandList(m_CommandList, nvrhi::CommandQueue::Graphics);

	nvrhi::EventQueryHandle event = m_Device->createEventQuery();
	m_Device->setEventQuery(event, nvrhi::CommandQueue::Graphics);

	return event;
}

std::expected<nvrhi::EventQueryHandle, std::string> st::gfx::DataUploader::UploadTextureDataInternal(
	const st::IBlob& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
	const nvrhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker)
{
	std::scoped_lock lock{ m_UploadMutex };

	m_CommandList->open();
	m_CommandList->beginTrackingTextureState(dstTexture, {}, currentTextureState);

	nvrhi::TextureDesc desc = dstTexture->getDesc();

	ID3D12Device* d3d12Device = m_Device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device);
	ID3D12Resource* d3d12Texture = dstTexture->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);

	uint64_t requiredSize = 0;
	D3D12_RESOURCE_DESC d3d12Desc = d3d12Texture->GetDesc();
	d3d12Device->GetCopyableFootprints(
		&d3d12Desc,
		subresources.baseArraySlice,
		subresources.numArraySlices == nvrhi::TextureSubresourceSet::AllArraySlices ? d3d12Desc.DepthOrArraySize : subresources.numArraySlices,
		0, nullptr, nullptr, nullptr, &requiredSize);
	assert(requiredSize == srcData.size());

	/*
		for (uint32_t arraySlice = 0; arraySlice < desc.arraySize; arraySlice++)
		{
			for (uint32_t mipLevel = 0; mipLevel < desc.mipLevels; mipLevel++)
			{
				const TextureSubresourceData& layout = texture->dataLayout[arraySlice][mipLevel];

				commandList->writeTexture(texture->texture, arraySlice, mipLevel, dataPointer + layout.dataOffset,
					layout.rowPitch, layout.depthPitch);
			}
		}
	*/

	return std::unexpected("TODO");
}