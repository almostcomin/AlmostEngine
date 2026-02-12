#pragma once

#include <expected>
#include <mutex>
#include <queue>
#include "Core/Blob.h"
#include "Core/RingBuffer.h"
#include "RHI/Buffer.h"
#include "RHI/Texture.h"
#include "RHI/ResourceState.h"
#include "Core/Signal.h"
#include "Core/Common.h"
#include "RHI/TypeForwards.h"

namespace st::rhi
{
	class Device;
}

namespace st::gfx
{
	class ShaderFactory;
}

namespace st::gfx
{

class DataUploader
{
public:

	enum class GenMipsMethod
	{
		None = 0,
		GenMips_Color,
		GenMips_NormalMap
	};

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
		UploadTicket(void* _ptr, uint64_t _start, uint64_t _end, uint64_t _alignedStart, size_t _size, uint64_t _idx)
			: ptr(_ptr), start(_start), end(_end), aligned_start(_alignedStart), size(_size), idx(_idx) 
		{};

		void* ptr = nullptr;	// aligned ptr to mem
		uint64_t start = 0;		// start position of the reservation
		uint64_t end = 0;		// end position of the reservation, can be higher than the requested one due to alignment
		uint64_t aligned_start;	// start position of the reservation, but aligned following the requirements
		uint64_t size;			// size of the requested reservation
		uint64_t idx = UINT64_MAX; // ticket index
	};

	DataUploader(ShaderFactory* shaderFactory, rhi::Device* device);
	~DataUploader();

	std::expected<UploadTicket, std::string> RequestUploadTicket(const rhi::BufferDesc& desc);
	std::expected<UploadTicket, std::string> RequestUploadTicket(const rhi::TextureDesc& desc, const rhi::TextureSubresourceSet& subresources);
	std::expected<UploadTicket, std::string> RequestUploadTicket(size_t size, size_t alignment);

	std::expected<SignalListener, std::string> CommitUploadBufferTicket(UploadTicket&& ticket, rhi::BufferHandle dstBuffer,
		rhi::ResourceState currentBufferState, rhi::ResourceState targetBufferState, size_t dstStart = 0, const char* opt_gpuMarker = nullptr);

	/// @param subresources Subresources to copy. If any GenMipsMethod, will generate mips in all mips from the last mip in subresources
	std::expected<SignalListener, std::string> CommitUploadTextureTicket(UploadTicket&& ticket, rhi::TextureHandle dstTexture, 
		rhi::ResourceState currentState, rhi::ResourceState targetState, const rhi::TextureSubresourceSet& subresources, 
		GenMipsMethod genMipsMethod = GenMipsMethod::None, const char* opt_gpuMarker = nullptr);

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
		const st::WeakBlob& srcData, rhi::BufferHandle dstBuffer, rhi::ResourceState currentBufferState, rhi::ResourceState targetBufferState,
		size_t dstStart = 0, const char* opt_gpuMarker = nullptr);

	/// Uploads data to a texture object.
	/// 
	/// @param srcData Pointer to the source data to copy from. Densely packed.
	/// @param dstTexture Destination texture where the data will be copied.
	/// @param currentState Current state of the texture object.
	/// @param targetState Desired final state of the texture object after the copy.
	/// @param subresources Subresources to copy. If any GenMipsMethod, will generate mips in all mips from the last mip in subresources
	/// @param generateMips Will upload data for the first mip only
	/// @param opt_gpuMarker Optional GPU marker
	/// 
	/// @return On success, returns an `SignalListener` representing the GPU event query for the copy operation.
	///         On failure, returns a `std::string` describing the error.
	std::expected<SignalListener, std::string> UploadTextureData(
		const st::WeakBlob& srcData, rhi::TextureHandle dstTexture, rhi::ResourceState currentState, rhi::ResourceState targetState,
		const rhi::TextureSubresourceSet& subresources, GenMipsMethod genMipsMethod = GenMipsMethod::None, const char* opt_gpuMarker = nullptr);

private: /* types */

private: /* methods */

	rhi::CommandListOwner GetCommandList();
	SignalListener FinishCommandList(rhi::CommandListOwner&& commandList, UploadTicket&& ticket,
		std::vector<std::pair<rhi::TextureSampledView, rhi::TextureStorageView>>&& mipGenViews = {});

	void InsertPendingTicket(UploadTicket&& ticket);
	void OnCompletedTicket(UploadTicket&& ticket);

	void GenerateMips(rhi::ITexture* texture, GenMipsMethod method, rhi::ResourceState currentState, rhi::ResourceState targetState, 
		const rhi::TextureSubresourceSet& subresources, rhi::ICommandList* commandList, std::vector<std::pair<rhi::TextureSampledView, rhi::TextureStorageView>>& tempViews);

	void AsyncUpdate();

private: /* */
	
	rhi::BufferOwner m_UploadBuffer;
	uint64_t m_UploadBufferHead;
	uint64_t m_UploadBufferTail;
	std::mutex m_UploadBufferMutex;
	
	uint64_t m_NextTicketIdx;
	uint64_t m_CompletedTicketIdx;
	std::vector<UploadTicket> m_PendingTickets; // ordered

	struct InFlightCommandListEntry
	{
		uint64_t CompletedIdx;
		rhi::CommandListOwner CommandList;
		SignalEmitter Signal;
		UploadTicket Ticket;
		std::vector<std::pair<rhi::TextureSampledView, rhi::TextureStorageView>> MipGenViews; // used during mipgen
	};
	RingBuffer<InFlightCommandListEntry, 1024> m_InFlightCommandLists;
	std::mutex m_InFlightCommandListsMutex;

	uint64_t m_CommitCount;
	rhi::FenceOwner m_CommitFence;

	std::thread m_ProcessInFlightCommandsThread;
	std::condition_variable m_InFlightCommandListsCV;
	std::atomic<bool> m_AsyncShutdown;

	rhi::ShaderOwner m_GenMips_CS;
	rhi::ShaderOwner m_GenMipsRenorm_CS;

	rhi::ComputePipelineStateOwner m_GenMips_PSO;
	rhi::ComputePipelineStateOwner m_GenMipsRenorm_PSO;

	st::rhi::Device* m_Device;
};

}