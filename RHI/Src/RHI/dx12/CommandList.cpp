#include "RHI/RHI_PCH.h"
#include "RHI/DxgiFormatMapping.h"
#include "RHI/Viewport.h"
#include "RHI/dx12/CommandList.h"
#include "RHI/dx12/Buffer.h"
#include "RHI/dx12/Texture.h"
#include "RHI/dx12/ResourceState.h"
#include "RHI/dx12/PipelineState.h"
#include "RHI/dx12/FrameBuffer.h"
#include "RHI/dx12/TimerQuery.h"
#include "RHI/dx12/GPUDevice.h"
#include "RHI/dx12/Utils.h"
#include <pix3.h>

namespace
{
	constexpr uint32_t ImageAspectToPlane(alm::rhi::ImageAspect value)
	{
		switch (value)
		{
		case alm::rhi::ImageAspect::Undef:
		case alm::rhi::ImageAspect::Color:
		case alm::rhi::ImageAspect::Depth:
			return 0;
		case alm::rhi::ImageAspect::Stencil:
			return 1;
		default:
			return 0;
		}
	}
} // anonymous namespace

void alm::rhi::dx12::CommandList::Open()
{
	m_D3d12CommandAllocator->Reset();
	m_D3d12Commandlist->Reset(m_D3d12CommandAllocator.Get(), nullptr);

	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	ID3D12DescriptorHeap* heaps[] = {
		gpuDevice->GetShaderResourceViewHeap()->GetHeap(),
		gpuDevice->GetSamperHeap()->GetHeap()
	};
	m_D3d12Commandlist->SetDescriptorHeaps(std::size(heaps), heaps);

	m_D3d12Commandlist->SetGraphicsRootSignature(gpuDevice->GetBindlessRootSignature());
	m_D3d12Commandlist->SetComputeRootSignature(gpuDevice->GetBindlessRootSignature());
		
	m_CurrentGraphicsPSO = nullptr;
	m_CurrentComputePSO = nullptr;

	m_DrawCalls = 0;
	m_DispatchCalls = 0;
	m_PrimitiveCount = 0;

	m_BeginTimerQueries.clear();
	m_EndTimerQueries.clear();
}

void alm::rhi::dx12::CommandList::Close()
{
	// Resolve timer queries
	if(!m_BeginTimerQueries.empty() || !m_EndTimerQueries.empty())
	{
		GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());

		std::vector<uint32_t> toResolveQueries;
		toResolveQueries.reserve(m_BeginTimerQueries.size() + m_EndTimerQueries.size());

		for (const TimerQueryHandle& handle : m_BeginTimerQueries)
		{
			auto* tq = alm::checked_cast<const TimerQuery*>(handle.get());
			toResolveQueries.push_back(tq->m_BeginQueryIndex);
		}
		for (const TimerQueryHandle& handle : m_EndTimerQueries)
		{
			auto* tq = alm::checked_cast<const TimerQuery*>(handle.get());
			toResolveQueries.push_back(tq->m_EndQueryIndex);
		}

		std::sort(toResolveQueries.begin(), toResolveQueries.end());

		for (int i = 0; i < toResolveQueries.size();)
		{
			int startIdx = i;
			int endIdx = i;

			while ((endIdx + 1 < toResolveQueries.size()) && (toResolveQueries[endIdx + 1] == toResolveQueries[endIdx] + 1))
			{
				++endIdx;
			}

			uint32_t firstQ = toResolveQueries[startIdx];
			uint32_t rangeQ = toResolveQueries[endIdx] - toResolveQueries[startIdx] + 1;

			m_D3d12Commandlist->ResolveQueryData(gpuDevice->GetQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP,
				firstQ, rangeQ, gpuDevice->GetQueryResolveBuffer(), firstQ * sizeof(uint64_t));

			i = endIdx + 1;
		}
	}

	[[maybe_unused]] HRESULT hr = m_D3d12Commandlist->Close();
	assert(hr == S_OK);
}

void alm::rhi::dx12::CommandList::WriteBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, size_t size)
{
	assert(srcOffset + size <= srcBuffer->GetDesc().sizeBytes);
	assert(dstOffset + size <= dstBuffer->GetDesc().sizeBytes);

	auto* d3d12SrcBuffer = checked_cast<Buffer*>(srcBuffer);
	auto* d3d12DstBuffer = checked_cast<Buffer*>(dstBuffer);

	m_D3d12Commandlist->CopyBufferRegion(d3d12DstBuffer->GetNativeResource(), dstOffset, d3d12SrcBuffer->GetNativeResource(), srcOffset, size);
}

void alm::rhi::dx12::CommandList::WriteTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& subresources, IBuffer* srcBuffer, uint64_t srcOffset)
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

			GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
			uint64_t totalBytes = 0;
			gpuDevice->GetD3d12Device()->GetCopyableFootprints(
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

void alm::rhi::dx12::CommandList::CopyTextureToTexture(ITexture* dstTexture, ITexture* srcTexture)
{
	m_D3d12Commandlist->CopyResource(dstTexture->GetNativeResource(), srcTexture->GetNativeResource());
}

void alm::rhi::dx12::CommandList::CopyTextureToTexture(ITexture* dstTexture, const rhi::TextureSubresourceSet& dstSubresources,
	ITexture* srcTexture, const rhi::TextureSubresourceSet& srcSubresources)
{
	// Only single slice copy supported
	assert(dstSubresources == AllSubresources);
	assert(srcSubresources == AllSubresources);

	const auto& dstDesc = dstTexture->GetDesc();
	const auto& srcDesc = srcTexture->GetDesc();
	assert(dstDesc.depth == 1 && dstDesc.arraySize == 1 && dstDesc.mipLevels == 1);
	assert(srcDesc.depth == 1 && srcDesc.arraySize == 1 && srcDesc.mipLevels == 1);
	assert(srcDesc.width == dstDesc.width && srcDesc.height == dstDesc.height);
	
	D3D12_TEXTURE_COPY_LOCATION dst =
	{
		dstTexture->GetNativeResource(),
		D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
		D3D12CalcSubresource(0, 0, 0, dstDesc.mipLevels, dstDesc.arraySize)
	};

	D3D12_TEXTURE_COPY_LOCATION src =
	{
		srcTexture->GetNativeResource(),
		D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
		D3D12CalcSubresource(0, 0, 0, srcDesc.mipLevels, srcDesc.arraySize)
	};

	m_D3d12Commandlist->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

void alm::rhi::dx12::CommandList::CopyBufferToBuffer(IBuffer* dstBuffer, uint64_t dstOffset, IBuffer* srcBuffer, uint64_t srcOffset, uint64_t size)
{
	const auto& dstDesc = dstBuffer->GetDesc();
	const auto& srcDesc = srcBuffer->GetDesc();
	assert(dstOffset + size <= dstDesc.sizeBytes);
	assert(srcOffset + size <= srcDesc.sizeBytes);

	m_D3d12Commandlist->CopyBufferRegion(
		dstBuffer->GetNativeResource(), dstOffset,
		srcBuffer->GetNativeResource(), srcOffset,
		size);
}

void alm::rhi::dx12::CommandList::CopyTextureToBuffer(IBuffer* dstBuffer, ITexture* srcTexture, const rhi::TextureSubresourceSet& srcSubresources)
{
	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	assert(srcSubresources == AllSubresources);

	const auto& srcDesc = srcTexture->GetDesc();
	const auto& dstDesc = dstBuffer->GetDesc();
	ID3D12Resource* d3d12Src = srcTexture->GetNativeResource();
	D3D12_RESOURCE_DESC d3d12SrcDesc = d3d12Src->GetDesc();

	uint32_t numberOfResources = (srcDesc.dimension == rhi::TextureDimension::Texture3D) ? 1u : srcDesc.arraySize;
	numberOfResources *= srcDesc.mipLevels;
	numberOfResources *= 1;// numberOfPlanes;

	UINT64 totalResourceSize = 0;
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layout;
	layout.resize(numberOfResources);
	gpuDevice->GetD3d12Device()->GetCopyableFootprints(&d3d12SrcDesc, 0, numberOfResources, 0,
		layout.data(), nullptr, nullptr, &totalResourceSize);
	assert(totalResourceSize == dstDesc.sizeBytes);

	for (uint32_t i = 0; i < numberOfResources; ++i)
	{
		D3D12_TEXTURE_COPY_LOCATION src =
		{
			srcTexture->GetNativeResource(),
			D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			D3D12CalcSubresource(0, 0, 0, srcDesc.mipLevels, srcDesc.arraySize)
		};

		D3D12_TEXTURE_COPY_LOCATION dst =
		{
			.pResource = dstBuffer->GetNativeResource(),
			.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint = layout[i]
		};

		m_D3d12Commandlist->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}
}

void alm::rhi::dx12::CommandList::PushBarriers(std::span<const Barrier> barriers)
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

void alm::rhi::dx12::CommandList::SetPipelineState(IGraphicsPipelineState* pso)
{
	if (!m_CurrentGraphicsPSO || m_CurrentGraphicsPSO.get() != pso)
	{
		GraphicsPipelineState* impl = checked_cast<GraphicsPipelineState*>(pso);

		m_D3d12Commandlist->SetPipelineState(impl->GetD3d12PSO());
		m_D3d12Commandlist->IASetPrimitiveTopology(GetPrimitiveTopology(impl->GetDesc().primTopo, impl->GetDesc().patchControlPoints));

		m_CurrentGraphicsPSO = static_pointer_cast<IGraphicsPipelineState>(pso->weak_from_this());
		m_CurrentComputePSO = nullptr;
	}
}

void alm::rhi::dx12::CommandList::SetPipelineState(IComputePipelineState* pso)
{
	if (!m_CurrentComputePSO || m_CurrentComputePSO.get() != pso)
	{
		m_D3d12Commandlist->SetPipelineState(pso->GetNativeResource());

		m_CurrentComputePSO = static_pointer_cast<IComputePipelineState>(pso->weak_from_this());
		m_CurrentGraphicsPSO = nullptr;
	}
}

void alm::rhi::dx12::CommandList::SetViewport(const alm::rhi::ViewportState& vp)
{
	D3D12_VIEWPORT d3d12Viewport[c_MaxViewports];
	for (int i = 0; i < vp.viewports.size(); ++i)
	{
		d3d12Viewport[i].TopLeftX = vp.viewports[0].minX;
		d3d12Viewport[i].TopLeftY = vp.viewports[0].minY;
		d3d12Viewport[i].Width = vp.viewports[0].Width();
		d3d12Viewport[i].Height = vp.viewports[0].Height();
		d3d12Viewport[i].MinDepth = vp.viewports[i].minZ;
		d3d12Viewport[i].MaxDepth = vp.viewports[i].maxZ;
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

void alm::rhi::dx12::CommandList::SetStencilRef(uint8_t value)
{
	m_D3d12Commandlist->OMSetStencilRef(value);
}

void alm::rhi::dx12::CommandList::SetBlendFactor(const float4& value)
{
	m_D3d12Commandlist->OMSetBlendFactor(&value.x);
}

void alm::rhi::dx12::CommandList::PushConstants(uint32_t slot, const void* data, size_t sizeBytes, size_t offsetBytes, bool isCompute)
{
	assert(IsAligned(sizeBytes, sizeof(uint32_t)));
	assert(IsAligned(offsetBytes, sizeof(uint32_t)));
	assert((sizeBytes + offsetBytes) < (slot == 0 ? 32 : 16) * sizeof(uint32_t));

	if (isCompute)
	{
		m_D3d12Commandlist->SetComputeRoot32BitConstants(slot, sizeBytes / 4, data, offsetBytes / 4);
	}
	else
	{
		m_D3d12Commandlist->SetGraphicsRoot32BitConstants(slot, sizeBytes / 4, data, offsetBytes / 4);
	}
}

void alm::rhi::dx12::CommandList::BeginRenderPass(rhi::IFramebuffer* _fb, const std::vector<RenderPassOp>& rtvRenderPassOp, 
	const RenderPassOp& depthRenderPassOp, const RenderPassOp& stencilRenderPassOp, RenderPassFlags rpFlags)
{
	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	
	m_CurrentFB = alm::checked_pointer_cast<rhi::IFramebuffer>(_fb->weak_from_this());
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
			depthRenderPassOp.loadOp, depthRenderPassOp.clearValue, fb->dsvTexture->GetDesc().format);
		DSV.DepthEndingAccess = GetRenderPassEngindAccess(depthRenderPassOp.storeOp);

		DSV.StencilBeginningAccess = GetRenderPassBeginningAccess(
			stencilRenderPassOp.loadOp, stencilRenderPassOp.clearValue, fb->dsvTexture->GetDesc().format);
		DSV.StencilEndingAccess = GetRenderPassEngindAccess(stencilRenderPassOp.storeOp);
	}

	D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE;
	if (has_any_flag(rpFlags, RenderPassFlags::AllowUAVWrites))
	{
		flags |= D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
	}

	m_D3d12Commandlist->BeginRenderPass(fb->RTVs.size(), RTVs, fb->DSV == c_InvalidDescriptorIndex ? nullptr : &DSV, flags);

	// Set fullscreen viewport
	SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
		(float)fb->GetFramebufferInfo().width, (float)fb->GetFramebufferInfo().height }));
}

void alm::rhi::dx12::CommandList::EndRenderPass()
{
	m_D3d12Commandlist->EndRenderPass();
	m_CurrentFB = {};
}

void alm::rhi::dx12::CommandList::Draw(uint32_t vertexCount)
{
	m_D3d12Commandlist->DrawInstanced(vertexCount, 1, 0, 0);
	
	++m_DrawCalls;
	m_PrimitiveCount += GetPrimitiveCount(vertexCount, m_CurrentGraphicsPSO->GetDesc().primTopo);
}

void alm::rhi::dx12::CommandList::DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertex)
{
	m_D3d12Commandlist->DrawInstanced(vertexCountPerInstance, instanceCount, startVertex, 0);

	++m_DrawCalls;
	m_PrimitiveCount += GetPrimitiveCount(vertexCountPerInstance, m_CurrentGraphicsPSO->GetDesc().primTopo) * instanceCount;
}

void alm::rhi::dx12::CommandList::Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
{
	m_D3d12Commandlist->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);

	++m_DispatchCalls;
}

void alm::rhi::dx12::CommandList::Discard(IBuffer* buffer)
{
	m_D3d12Commandlist->DiscardResource(buffer->GetNativeResource(), nullptr);
}

void alm::rhi::dx12::CommandList::Discard(ITexture* texture, int mipLevel, int arraySlice)
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

void alm::rhi::dx12::CommandList::ClearRenderTarget(TextureColorTargetView rtView, const float4& color) 
{
	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	const rhi::dx12::DescriptorHeap* rtHeap = gpuDevice->GetRenderTargetViewHeap();
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtHeap->GetCpuHandle(rtView.GetIdx());
	if (handle.ptr == 0u)
	{
		LOG_ERROR("Invalid render target view");
		return;
	}
	m_D3d12Commandlist->ClearRenderTargetView(handle, (FLOAT*)&color.x, 0, NULL);
}

void alm::rhi::dx12::CommandList::BeginMarker(const char* str)
{
	PIXBeginEvent(m_D3d12Commandlist.Get(), 0, str);
}

void alm::rhi::dx12::CommandList::EndMarker()
{
	PIXEndEvent(m_D3d12Commandlist.Get());
}

void alm::rhi::dx12::CommandList::BeginTimerQuery(ITimerQuery* query)
{
	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	auto* tq = alm::checked_cast<TimerQuery*>(query);

	m_D3d12Commandlist->EndQuery(gpuDevice->GetQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, tq->m_BeginQueryIndex);

	m_BeginTimerQueries.push_back(static_pointer_cast<TimerQuery>(query->weak_from_this()));
}

void alm::rhi::dx12::CommandList::EndTimerQuery(ITimerQuery* query)
{
	GpuDevice* gpuDevice = checked_cast<GpuDevice*>(GetDevice());
	auto* tq = alm::checked_cast<TimerQuery*>(query);

	m_D3d12Commandlist->EndQuery(gpuDevice->GetQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, tq->m_EndQueryIndex);

	m_EndTimerQueries.push_back(static_pointer_cast<ITimerQuery>(query->weak_from_this()));
}

void alm::rhi::dx12::CommandList::OnExecuted(alm::rhi::dx12::Queue& queue)
{
	for (const auto& it : m_BeginTimerQueries)
	{
		static_pointer_cast<TimerQuery>(it)->OnBeginExecuted();
	}
	for (const auto& it : m_EndTimerQueries)
	{
		static_pointer_cast<TimerQuery>(it)->OnEndExecuted(queue);
	}
}

void alm::rhi::dx12::CommandList::Release(Device* device)
{
	assert(!m_CurrentFB);
	m_D3d12CommandAllocator.Reset();
	m_D3d12Commandlist.Reset();
}