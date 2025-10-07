#pragma once

#include <expected>
#include <mutex>
#include <nvrhi/nvrhi.h>
#include "Core/Blob.h"

namespace st::gfx
{

class DataUploader
{
public:

	DataUploader(nvrhi::DeviceHandle device);

	bool Init();

	/// Uploads data to a buffer object.
	/// 
	/// @param srcData Pointer to the source data to copy from.
	/// @param dstBuffer Destination buffer where the data will be copied.
	/// @param dstCurrentBufferState Current state of the buffer object.
	/// @param dstBufferTargetState Desired final state of the buffer object after the copy.
	/// @param dstStart Offset from the start of dstBuffer where the data will be copied.
	/// @param dstSize Amount of data to copy. If negative, the entire buffer will be copied.
	/// @param opt_gpuMarker Optional GPU marker.
	/// 
	/// @return On success, returns an `nvrhi::EventQueryHandle` representing the GPU event query for the copy operation.
	///         On failure, returns a `std::string` describing the error.
	std::expected<nvrhi::EventQueryHandle, std::string> UploadBufferData(
		st::Blob&& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
		size_t dstStart = 0, int dstSize = -1, const char* opt_gpuMarker = nullptr);

	/// Uploads data to a buffer object.
	/// 
	/// @param srcData Pointer to the source data to copy from.
	/// @param dstBuffer Destination buffer where the data will be copied.
	/// @param dstCurrentBufferState Current state of the buffer object.
	/// @param dstBufferTargetState Desired final state of the buffer object after the copy.
	/// @param dstStart Offset from the start of dstBuffer where the data will be copied.
	/// @param dstSize Amount of data to copy. If negative, the entire buffer will be copied.
	/// @param opt_gpuMarker Optional GPU marker.
	/// 
	/// @return On success, returns an `nvrhi::EventQueryHandle` representing the GPU event query for the copy operation.
	///         On failure, returns a `std::string` describing the error.
	std::expected<nvrhi::EventQueryHandle, std::string> UploadBufferData(
		const st::WeakBlob& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
		size_t dstStart = 0, int dstSize = -1, const char* opt_gpuMarker = nullptr);

	/// Uploads data to a texture object.
	/// 
	/// @param dstTexture Destination texture where the data will be copied.
	/// @param currentTextureState Current state of the texture object.
	/// @param textureTargetState Desired final state of the texture object after the copy.
	/// @param srcData Pointer to the source data to copy from.
	/// @param dstStart Offset from the start of dstTexture where the data will be copied.
	/// @param dstSize Amount of data to copy. If negative, the entire texture will be copied.
	/// @param opt_gpuMarker Optional GPU marker.
	/// 
	/// @return On success, returns an `nvrhi::EventQueryHandle` representing the GPU event query for the copy operation.
	///         On failure, returns a `std::string` describing the error.
	std::expected<nvrhi::EventQueryHandle, std::string> UploadTextureData(
		const st::WeakBlob& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
		const nvrhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker = nullptr);

private:

	std::expected<nvrhi::EventQueryHandle, std::string> UploadBufferDataInternal(
		const st::IBlob& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState, 
		size_t dstStart, int dstSize, const char* opt_gpuMarker);

	std::expected<nvrhi::EventQueryHandle, std::string> UploadTextureDataInternal(
		const st::IBlob& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
		const nvrhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker = nullptr);

private:

	nvrhi::CommandListHandle m_CommandList;
	std::mutex m_UploadMutex;

	nvrhi::DeviceHandle m_Device;
};

}