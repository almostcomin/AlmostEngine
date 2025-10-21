#pragma once

#include <expected>
#include <mutex>
#include <nvrhi/nvrhi.h>
#include "Core/Blob.h"
#include "Core/RingBuffer.h"
#include "Core/Signal.h"

struct ID3D12Fence; // TODO

namespace st::gfx
{

class DataUploader
{
public:

	DataUploader(nvrhi::DeviceHandle device);

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
	/// @return On success, returns an `SignalListener` representing the GPU event query for the copy operation.
	///         On failure, returns a `std::string` describing the error.
	std::expected<SignalListener, std::string> UploadBufferData(
		st::Blob&& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
		size_t dstStart = 0, int dstSize = -1, const char* opt_gpuMarker = nullptr);

	std::expected<SignalListener, std::string> UploadBufferData(
		st::SharedBlob&& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
		size_t dstStart = 0, int dstSize = -1, const char* opt_gpuMarker = nullptr);

	std::expected<SignalListener, std::string> UploadBufferData(
		const st::WeakBlob& srcData, nvrhi::BufferHandle dstBuffer, nvrhi::ResourceStates dstCurrentBufferState, nvrhi::ResourceStates dstBufferTargetState,
		size_t dstStart = 0, int dstSize = -1, const char* opt_gpuMarker = nullptr);

	/// Uploads data to a texture object.
	/// 
	/// @param srcData Pointer to the source data to copy from. Densely packed.
	/// @param dstTexture Destination texture where the data will be copied.
	/// @param currentTextureState Current state of the texture object.
	/// @param textureTargetState Desired final state of the texture object after the copy.
	/// @param subresources Subresources to copy
	/// @param opt_gpuMarker Optional GPU marker.
	/// 
	/// @return On success, returns an `SignalListener` representing the GPU event query for the copy operation.
	///         On failure, returns a `std::string` describing the error.
	std::expected<SignalListener, std::string> UploadTextureData(
		st::Blob&& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
		const nvrhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker = nullptr);

	std::expected<SignalListener, std::string> UploadTextureData(
		st::SharedBlob&& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
		const nvrhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker = nullptr);

	std::expected<SignalListener, std::string> UploadTextureData(
		const st::WeakBlob& srcData, nvrhi::TextureHandle dstTexture, nvrhi::ResourceStates currentTextureState, nvrhi::ResourceStates textureTargetState,
		const nvrhi::TextureSubresourceSet& subresources, const char* opt_gpuMarker = nullptr);

	void ProcessRenderingThreadCommands();
	void RunGarbageCollector();

private: /* types */

	struct ThreadLocalData
	{
		nvrhi::CommandListHandle CommandList;
		uint64_t CommitIdx;

		ThreadLocalData(nvrhi::DeviceHandle device);
	};

private: /* methods */

	std::pair<ThreadLocalData*, SignalListener> GetThreadLocalData();

private: /* */
	
	struct SignalDataType
	{
		uint64_t CommitIdx;
		SignalEmitter Signal;
	};

	st::RingBuffer<SignalDataType, 16> m_Signals;

	std::unordered_map<std::thread::id, std::unique_ptr<ThreadLocalData>> m_ThreadLocal;
	std::mutex m_ThreadLocalMutex;

	uint64_t m_CommitCount;
	nvrhi::RefCountPtr<ID3D12Fence>	m_CommitFence;

	nvrhi::DeviceHandle m_Device;
};

}