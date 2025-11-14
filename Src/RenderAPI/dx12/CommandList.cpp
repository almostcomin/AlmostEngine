#include "RenderAPI/dx12/CommandList.h"
#include "RenderAPI/dx12/Buffer.h"
#include "RenderAPI/dx12/Texture.h"
#include "RenderAPI/dx12/ResourceState.h"
#include "RenderAPI/dx12/DxgiFormat.h"
#include "Core/Util.h"
#include <pix_win.h>
#include <cassert>
#include <vector>
#include "Core/Log.h"

namespace
{
	constexpr uint32_t ImageAspectToPlane(st::rapi::ImageAspect value)
	{
		switch (value)
		{
		case st::rapi::ImageAspect::Undef:
		case st::rapi::ImageAspect::Color:
		case st::rapi::ImageAspect::Depth:
			return 0;
		case st::rapi::ImageAspect::Stencil:
			return 1;
		default:
			return 0;
		}
	}
} // anonymous namespace

void st::rapi::dx12::CommandList::Open()
{
	m_D3d12Commandlist->Reset(m_D3d12CommandAllocator.Get(), nullptr);
}

void st::rapi::dx12::CommandList::Close()
{
	m_D3d12Commandlist->Close();
}

void st::rapi::dx12::CommandList::WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size)
{
	assert(srcOffset + size <= srcBuffer->GetDesc().sizeBytes);
	assert(dstOffset + size <= dstBuffer->GetDesc().sizeBytes);

	auto* d3d12SrcBuffer = checked_cast<Buffer*>(srcBuffer);
	auto* d3d12DstBuffer = checked_cast<Buffer*>(dstBuffer);

	m_D3d12Commandlist->CopyBufferRegion(d3d12DstBuffer->GetNativeResource(), dstOffset, d3d12SrcBuffer->GetNativeResource(), srcOffset, size);
}

void st::rapi::dx12::CommandList::WriteTexture(ITexture* dstTexture, const rapi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset)
{
	const TextureDesc& desc = dstTexture->GetDesc();
	uint32_t lastArraySlice = subresources.ResolveLastArraySlice(desc);
	uint32_t lastMip = subresources.ResolveLastMipLevel(desc);
	ID3D12Resource* d3d12Texture = dstTexture->GetNativeResource();
	D3D12_RESOURCE_DESC d3d12Desc = d3d12Texture->GetDesc();

	for (uint32_t arraySlice = subresources.baseArraySlice; arraySlice <= lastArraySlice; ++arraySlice)
	{
		for (int mip = subresources.baseMipLevel; mip <= lastMip; ++mip)
		{
			D3D12_TEXTURE_COPY_LOCATION dst = 
			{
				dstTexture->GetNativeResource(),
				D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				D3D12CalcSubresource(mip, arraySlice, 0, desc.mipLevels, desc.arraySize)
			};

			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = srcBuffer->GetNativeResource();
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint.Offset = srcOffset;
			src.PlacedFootprint.Footprint.Format = GetDxgiFormatMapping(desc.format).resourceFormat;
			src.PlacedFootprint.Footprint.Width = desc.width >> mip;
			src.PlacedFootprint.Footprint.Height = desc.height >> mip;
			src.PlacedFootprint.Footprint.Depth = desc.depth;

			uint64_t totalBytes = 0;
			m_Device->GetCopyableFootprints(
				&d3d12Desc,
				D3D12CalcSubresource(mip, arraySlice, 0, desc.mipLevels, desc.arraySize),
				1,
				srcOffset,
				&src.PlacedFootprint,
				nullptr,
				nullptr,
				&totalBytes);

			m_D3d12Commandlist->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			
			srcOffset += totalBytes;
		}
	}
}

void st::rapi::dx12::CommandList::PushBarriers(std::span<const Barrier> barriers)
{
	std::vector<D3D12_RESOURCE_BARRIER> d3d12Barriers;
	d3d12Barriers.reserve(barriers.size());

	for (const auto& barrier : barriers)
	{
		D3D12_RESOURCE_BARRIER d3d12Barrier = {};
		switch (barrier.type)
		{

		case Barrier::Type::MEMORY:
			d3d12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			d3d12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			d3d12Barrier.UAV.pResource = barrier.memory.resource->GetNativeResource();
			break;

		case Barrier::Type::BUFFER:
			d3d12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			d3d12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			d3d12Barrier.Transition.pResource = barrier.buffer.buffer->GetNativeResource();
			d3d12Barrier.Transition.StateBefore = MapResourceState(barrier.buffer.state_before);
			d3d12Barrier.Transition.StateAfter = MapResourceState(barrier.buffer.state_after);
			d3d12Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			break;

		case Barrier::Type::TEXTURE:
			d3d12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			d3d12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			d3d12Barrier.Transition.pResource = barrier.texture.texture->GetNativeResource();
			d3d12Barrier.Transition.StateBefore = MapResourceState(barrier.texture.layout_before);
			d3d12Barrier.Transition.StateAfter = MapResourceState(barrier.texture.layout_after);
			if (barrier.texture.mip >= 0 || barrier.texture.slice >= 0 || barrier.texture.aspect != ImageAspect::Undef)
			{
				const uint32_t planeSlice = ImageAspectToPlane(barrier.texture.aspect);
				d3d12Barrier.Transition.Subresource = D3D12CalcSubresource(
					(UINT)std::max(0, barrier.texture.mip),
					(UINT)std::max(0, barrier.texture.slice),
					planeSlice,
					barrier.texture.texture->GetDesc().mipLevels,
					barrier.texture.texture->GetDesc().arraySize
				);
			}
			else
			{
				d3d12Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			}
			break;

		case Barrier::Type::ALIASING:
			d3d12Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			d3d12Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			d3d12Barrier.Aliasing.pResourceBefore = barrier.aliasing.resource_before->GetNativeResource();
			d3d12Barrier.Aliasing.pResourceAfter = barrier.aliasing.resource_after->GetNativeResource();
			break;

		default:
			assert(0);
		}

#if _DEBUG
		if (d3d12Barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION && m_Type != QueueType::Graphics)
		{
			bool pixelStateBarrier = (d3d12Barrier.Transition.StateBefore & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) ||
				(d3d12Barrier.Transition.StateAfter & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			if (pixelStateBarrier)
			{
				LOG_ERROR("Only graphics queue can set pixel shader barriers");
			}
		}
#endif

		d3d12Barriers.push_back(d3d12Barrier);
	}

	if (!d3d12Barriers.empty())
	{
		m_D3d12Commandlist->ResourceBarrier(d3d12Barriers.size(), d3d12Barriers.data());
	}
}

void st::rapi::dx12::CommandList::PushBarrier(const Barrier& barrier)
{
	PushBarriers(std::span{ &barrier, 1 });
}

void st::rapi::dx12::CommandList::Discard(IBuffer* buffer)
{
	m_D3d12Commandlist->DiscardResource(buffer->GetNativeResource(), nullptr);
}

void st::rapi::dx12::CommandList::Discard(ITexture* texture, int mipLevel, int arraySlice)
{
	if (mipLevel == -1 && arraySlice == -1)
	{
		m_D3d12Commandlist->DiscardResource(texture->GetNativeResource(), nullptr);
	}
	else
	{
		D3D12_DISCARD_REGION region = {};
		region.FirstSubresource = D3D12CalcSubresource(
			std::max(0, mipLevel), std::max(0, arraySlice), 0, texture->GetDesc().mipLevels, texture->GetDesc().arraySize);
		region.NumSubresources = 1;
		m_D3d12Commandlist->DiscardResource(texture->GetNativeResource(), &region);		
	}
}

void st::rapi::dx12::CommandList::BeginMarker(const char* str)
{
	PIXBeginEvent(m_D3d12Commandlist.Get(), 0, str);
}

void st::rapi::dx12::CommandList::EndMarker()
{
	PIXEndEvent(m_D3d12Commandlist.Get());
}