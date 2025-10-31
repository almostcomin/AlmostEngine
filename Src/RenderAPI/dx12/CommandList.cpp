#include "RenderAPI/dx12/CommandList.h"
#include "RenderAPI/dx12/Buffer.h"
#include "RenderAPI/dx12/Texture.h"
#include "RenderAPI/dx12/ResourceState.h"
#include "Core/Util.h"
#include <pix_win.h>
#include <cassert>
#include <vector>

namespace
{
	constexpr uint32_t ImageAspectToPlane(st::rapi::ImageAspect value)
	{
		switch (value)
		{
		case st::rapi::ImageAspect::Undef:
			return 0;
		case st::rapi::ImageAspect::Color:
			return 0;
		case st::rapi::ImageAspect::Depth:
			return 0;
		case st::rapi::ImageAspect::Stencil:
			return 1;
		default:
			return 0;
		}
	}
} // anonymous namespace

void st::rapi::dx12::CommandList::WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size)
{
	assert(srcOffset + size <= srcBuffer->GetDesc().sizeBytes);
	assert(dstOffset + size <= dstBuffer->GetDesc().sizeBytes);

	auto* d3d12SrcBuffer = checked_cast<Buffer*>(srcBuffer);
	auto* d3d12DstBuffer = checked_cast<Buffer*>(dstBuffer);

	m_D3d12Commandlist->CopyBufferRegion(d3d12DstBuffer->GetNativeResource(), dstOffset, d3d12SrcBuffer->GetNativeResource(), srcOffset, size);
}

void st::rapi::dx12::CommandList::PushBarriers(std::span<Barrier> barriers)
{
	std::vector<D3D12_RESOURCE_BARRIER> d3d12Barriers;
	d3d12Barriers.reserve(barriers.size());

	std::vector<ID3D12Resource*> discards;

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
			if (barrier.buffer.state_before == ResourceState::UNDEFINED)
			{
				discards.push_back(barrier.buffer.buffer->GetNativeResource());
			}
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
					(UINT)std::max(0, barrier.image.mip),
					(UINT)std::max(0, barrier.image.slice),
					PlaneSlice,
					barrier.image.texture->desc.mip_levels,
					barrier.image.texture->desc.array_size
				);
			}
			else
			{
			}
			break;

		case Barrier::Type::ALIASING:
			break;
		default:
			assert(0);
		}
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