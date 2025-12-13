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
#include "Core/Util.h"
#include <map>

namespace st::rapi
{
	class Device;
}

namespace st::gfx
{

class DataUploader
{
public:

	struct UploadTicket
	{
		friend class st::gfx::DataUploader;

		UploadTicket(const UploadTicket&) = delete;
		UploadTicket& operator=(const UploadTicket&) = delete;

		UploadTicket(UploadTicket&&) = default;
		UploadTicket& operator=(UploadTicket&&) = default;

		void* GetPtr() { return ptr; }

	private:

		UploadTicket() = default;
		UploadTicket(void* _ptr, uint64_t _start, size_t _size, uint64_t _idx)
			: ptr(_ptr), start(_start), size(_size), idx(_idx) 
		{};

		void* ptr = nullptr;	// aligned ptr to mem
		uint64_t start = 0;
		size_t size = 0;
		uint64_t idx = UINT64_MAX;
	};

	DataUploader(rapi::Device* device);
	~DataUploader();

	std::expected<UploadTicket, std::string> RequestUploadTicket(const rapi::BufferDesc& desc);
	std::expected<UploadTicket, std::string> RequestUploadTicket(const rapi::TextureDesc& desc);
	std::expected<UploadTicket, std::string> RequestUploadTicket(size_t size, size_t alignment);

	std::expected<SignalListener, std::string> CommitUploadBufferTicket(UploadTicket&& ticket, rapi::BufferHandle dstBuffer,
		rapi::ResourceState currentBufferState, rapi::ResourceState targetBufferState, size_t dstStart = 0, const char* opt_gpuMarker = nullptr);

	std::expected<SignalListener, std::string> CommitUploadTextureTicket(UploadTicket&& ticket, rapi::TextureHandle dstTexture, 
		rapi::ResourceState currentState, rapi::ResourceState targetState, const rapi::TextureSubresourceSet& subresources, const char* opt_gpuMarker = nullptr);

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

private: /* types */

private: /* methods */

	rapi::CommandListHandle GetCommandList();
	SignalListener FinishCommandList(rapi::CommandListHandle commandList, UploadTicket&& ticket);

	void OnCompletedTiket(UploadTicket&& ticket);

	void AsyncUpdate();

private: /* */
	
	rapi::BufferHandle m_UploadBuffer;
	uint64_t m_UploadBufferHead;
	uint64_t m_UploadBufferTail;
	std::mutex m_UploadBufferMutex;
	
	uint64_t m_NextTicketIdx;
	uint64_t m_CompletedTicketIdx;
	std::map<uint64_t, UploadTicket> m_PendingTickets;

	struct InFlightCommandListEntry
	{
		uint64_t CompletedIdx;
		rapi::CommandListHandle CommandList;
		SignalEmitter Signal;
		UploadTicket Ticket;
	};
	RingBuffer<InFlightCommandListEntry, 256> m_InFlightCommandLists;
	std::mutex m_InFlightCommandListsMutex;

	uint64_t m_CommitCount;
	rapi::FenceHandle m_CommitFence;

	std::thread m_ProcessInFlightCommandsThread;
	std::condition_variable m_InFlightCommandListsCV;
	std::atomic<bool> m_AsyncShutdown;

	st::rapi::Device* m_Device;
};

}