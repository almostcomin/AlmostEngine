#include "RHI/RHI_PCH.h"
#include "RHI/DxgiFormatMapping.h"
#include "RHI/Viewport.h"
#include "RHI/dx12/CommandList.h"
#include "RHI/dx12/Buffer.h"
#include "RHI/dx12/Texture.h"
#include "RHI/dx12/ResourceState.h"
#include "RHI/dx12/PipelineState.h"
#include "RHI/dx12/FrameBuffer.h"
#include "RHI/dx12/GPUDevice.h"
#include "RHI/dx12/Utils.h"
#include <pix3.h>

namespace
{
	constexpr uint32_t ImageAspectToPlane(st::rhi::ImageAspect value)
	{
		switch (value)
		{
		case st::rhi::ImageAspect::Undef:
		case st::rhi::ImageAspect::Color:
		case st::rhi::ImageAspect::Depth:
			return 0;
		case st::rhi::ImageAspect::Stencil:
			return 1;
		default:
			return 0;
		}
	}
} // anonymous namespace

void st::rhi::dx12::CommandList::Open()
{
	m_D3d12CommandAllocator->Reset();
	m_D3d12Commandlist->Reset(m_D3d12CommandAllocator.Get(), nullptr);

	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	ID3D12DescriptorHeap* heaps[] = {
		gpuDevice->GetShaderResourceViewHeap()->GetHeap(),
		gpuDevice->GetSamperHeap()->GetHeap()
	};
	m_D3d12Commandlist->SetDescriptorHeaps(std::size(heaps), heaps);
	m_CurrentPSO = nullptr;
}

void st::rhi::dx12::CommandList::Close()
{
	[[maybe_unused]] HRESULT hr = m_D3d12Commandlist->Close();
	assert(hr == S_OK);
}

void st::rhi::dx12::CommandList::WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size)
{
	assert(srcOffset + size <= srcBuffer->GetDesc().sizeBytes);
	assert(dstOffset + size <= dstBuffer->GetDesc().sizeBytes);

	auto* d3d12SrcBuffer = checked_cast<Buffer*>(srcBuffer);
	auto* d3d12DstBuffer = checked_cast<Buffer*>(dstBuffer);

	m_D3d12Commandlist->CopyBufferRegion(d3d12DstBuffer->GetNativeResource(), dstOffset, d3d12SrcBuffer->GetNativeResource(), srcOffset, size);
}

void st::rhi::dx12::CommandList::WriteTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset)
{
	const TextureDesc& desc = dstTexture->GetDesc();
	uint32_t lastArraySlice = subresources.GetLastArraySlice(desc);
	uint32_t lastMip = subresources.GetLastMipLevel(desc);
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

			GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
			uint64_t totalBytes = 0;
			gpuDevice->GetNativeDevice()->GetCopyableFootprints(
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

void st::rhi::dx12::CommandList::PushBarriers(std::span<const Barrier> barriers)
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

void st::rhi::dx12::CommandList::PushBarrier(const Barrier& barrier)
{
	PushBarriers(std::span{ &barrier, 1 });
}

void st::rhi::dx12::CommandList::SetPipelineState(IGraphicsPipelineState* pso)
{
	if (!m_CurrentPSO || m_CurrentPSO.get() != pso)
	{
		GraphicsPipelineState* impl = checked_cast<GraphicsPipelineState*>(pso);

		// TODO: Do not always set root signature
		m_D3d12Commandlist->SetGraphicsRootSignature(impl->GetD3d12Desc().pRootSignature);
		m_D3d12Commandlist->SetPipelineState(pso->GetNativeResource());
		m_D3d12Commandlist->IASetPrimitiveTopology(GetPrimitiveTopology(impl->GetDesc().primTopo, impl->GetDesc().patchControlPoints));

		m_CurrentPSO = static_pointer_cast<IGraphicsPipelineState>(pso->weak_from_this());
	}
}

void st::rhi::dx12::CommandList::SetViewport(const st::rhi::ViewportState& vp)
{
	D3D12_VIEWPORT d3d12Viewport[c_MaxViewports];
	for (int i = 0; i < vp.viewports.size(); ++i)
	{
		d3d12Viewport[i].TopLeftX = vp.viewports[0].minX;
		d3d12Viewport[i].TopLeftY = vp.viewports[0].minY;
		d3d12Viewport[i].Width = vp.viewports[0].Width();
		d3d12Viewport[i].Height = vp.viewports[0].Height();
		d3d12Viewport[i].MinDepth = vp.viewports[i].minZ;
		d3d12Viewport[i].MaxDepth = vp.viewports[i].maxX;
	}
	m_D3d12Commandlist->RSSetViewports(vp.viewports.size(), d3d12Viewport);

	D3D12_RECT sr[c_MaxViewports];
	for (int i = 0; i < vp.scissorRects.size(); ++i)
	{
		sr[i].left = vp.scissorRects[i].min.x;
		sr[i].top = vp.scissorRects[i].min.y;
		sr[i].right = vp.scissorRects[i].max.x;
		sr[i].bottom = vp.scissorRects[i].max.y;
	}
	m_D3d12Commandlist->RSSetScissorRects(vp.scissorRects.size(), sr);
}

void st::rhi::dx12::CommandList::SetStencilRef(uint8_t value)
{
	m_D3d12Commandlist->OMSetStencilRef(value);
}

void st::rhi::dx12::CommandList::SetBlendFactor(const float4& value)
{
	m_D3d12Commandlist->OMSetBlendFactor(&value.x);
}

void st::rhi::dx12::CommandList::PushConstants(const void* data, size_t sizeBytes, size_t offsetBytes)
{
	assert(IsAligned(sizeBytes, sizeof(uint32_t)));
	assert(IsAligned(offsetBytes, sizeof(uint32_t)));

	m_D3d12Commandlist->SetGraphicsRoot32BitConstants(0, sizeBytes / 4, data, offsetBytes / 4);
}

void st::rhi::dx12::CommandList::BeginRenderPass(rhi::IFramebuffer* _fb, const std::vector<RenderPassOp>& rtvRenderPassOp, 
	const RenderPassOp& dsvRenderPassOp, RenderPassFlags rpFlags)
{
	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	
	m_CurrentFB = st::checked_pointer_cast<rhi::IFramebuffer>(_fb->weak_from_this());
	Framebuffer* fb = checked_cast<Framebuffer*>(_fb);

	D3D12_RENDER_PASS_RENDER_TARGET_DESC RTVs[c_MaxRenderTargets] = {};
	for (int rtv_idx = 0; rtv_idx < fb->RTVs.size(); ++rtv_idx)
	{
		RTVs[rtv_idx].cpuDescriptor = gpuDevice->GetRenderTargetViewHeap()->GetCpuHandle(fb->RTVs[rtv_idx]);
		RTVs[rtv_idx].BeginningAccess = GetRenderPassBeginningAccess(
			rtvRenderPassOp[rtv_idx].loadOp, rtvRenderPassOp[rtv_idx].clearValue, fb->rtvTextures[rtv_idx]->GetDesc().format);
		RTVs[rtv_idx].EndingAccess = GetRenderPassEngindAccess(rtvRenderPassOp[rtv_idx].storeOp);
	}

	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC DSV = {};
	if (fb->DSV != c_InvalidDescriptorIndex)
	{
		DSV.cpuDescriptor = gpuDevice->GetDepthStencilViewHeap()->GetCpuHandle(fb->DSV);
		DSV.DepthBeginningAccess = GetRenderPassBeginningAccess(
			dsvRenderPassOp.loadOp, dsvRenderPassOp.clearValue, fb->dsvTexture->GetDesc().format);
		DSV.DepthEndingAccess = GetRenderPassEngindAccess(dsvRenderPassOp.storeOp);
	}

	D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE;
	if (hasFlag(rpFlags, RenderPassFlags::AllowUAVWrites))
	{
		flags |= D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
	}

	m_D3d12Commandlist->BeginRenderPass(fb->RTVs.size(), RTVs, fb->DSV == c_InvalidDescriptorIndex ? nullptr : &DSV, flags);
}

void st::rhi::dx12::CommandList::EndRenderPass()
{
	m_D3d12Commandlist->EndRenderPass();
	m_CurrentFB = {};
}

void st::rhi::dx12::CommandList::Draw(uint32_t vertexCount)
{
	m_D3d12Commandlist->DrawInstanced(vertexCount, 1, 0, 0);
}

void st::rhi::dx12::CommandList::DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount)
{
	m_D3d12Commandlist->DrawInstanced(vertexCountPerInstance, instanceCount, 0, 0);
}

void st::rhi::dx12::CommandList::Discard(IBuffer* buffer)
{
	m_D3d12Commandlist->DiscardResource(buffer->GetNativeResource(), nullptr);
}

void st::rhi::dx12::CommandList::Discard(ITexture* texture, int mipLevel, int arraySlice)
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

void st::rhi::dx12::CommandList::BeginMarker(const char* str)
{
	PIXBeginEvent(m_D3d12Commandlist.Get(), 0, str);
}

void st::rhi::dx12::CommandList::EndMarker()
{
	PIXEndEvent(m_D3d12Commandlist.Get());
}

void st::rhi::dx12::CommandList::Release(Device* device)
{
	assert(!m_CurrentFB);
	m_D3d12CommandAllocator.Reset();
	m_D3d12Commandlist.Reset();
}