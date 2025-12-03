#pragma once

#include <expected>
#include <mutex>
#include <queue>
#include "Core/Blob.h"
#include "Core/RingBuffer.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Texture.h"
#include "RenderAPI/ResourceState.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/Fence.h"
#include "Core/Signal.h"

namespace st::rapi
{
	class Device;
}

namespace st::gfx
{

class DataUploader
{
public:

	DataUploader(rapi::Device* device);
	~DataUploader();

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
		const st::WeakBlob& srcData, rapi::BufferHandle dstBuffer, rapi::ResourceState currentBufferState, rapi::ResourceState targetBufferState,
		size_t dstStart = 0, const char* opt_gpuMarker = nullptr);

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
		const st::WeakBlob& srcData, rapi::TextureHandle dstTexture, rapi::ResourceState currentState, rapi::ResourceState targetState,
		const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker = nullptr);

	void ProcessRenderingThreadCommands();
	void RunGarbageCollector();

private: /* types */

private: /* methods */

	rapi::CommandListHandle GetNewCommandList();
	uint64_t RequestUploadBufferSpace(size_t size);

private: /* */
	
	std::unordered_map<std::thread::id, rapi::CommandListHandle> m_ThreadLocal;
	std::mutex m_ThreadLocalMutex;

	rapi::BufferHandle m_UploadBuffer;
	uint64_t m_UploadBufferHead;
	uint64_t m_UploadBufferTail;
	std::mutex m_UploadBufferMutex;

	struct UploadBufferCompletionEntry
	{
		uint64_t CompletedIdx;
		uint64_t BufferPosition;
	};
	RingBuffer<UploadBufferCompletionEntry, 32> m_UploadBufferCompletion;

	struct InFlightCommandListEntry
	{
		uint64_t CompletedIdx;
		std::vector<rapi::CommandListHandle> CommandLists;
		SignalEmitter Signal;
	};
	RingBuffer<InFlightCommandListEntry, 32> m_InFlightCommandLists;

	uint64_t m_CommitCount;
	rapi::FenceHandle m_CommitFence;
	SignalEmitter m_CurrentSignal;

	st::rapi::Device* m_Device;
};

}