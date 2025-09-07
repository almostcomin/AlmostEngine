#pragma once
#include "Graphics/DeviceManager.h"
#include <nvrhi/nvrhi.h>
#include <dxgi1_5.h>
#include <directx/d3d12.h>

namespace st::gfx::dx12
{

class DeviceManager : public st::gfx::DeviceManager
{
public:

	bool Init(const InitParams& params) override;
	void Shutdown() override;

	bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override;
	bool CreateDevice() override;
	bool CreateSwapChain() override;

	void ReportLiveObjects() override;

private:

	bool CreateRenderTargets();
	void ReleaseRenderTargets();

private:

	InitParams								m_InitParams;

	nvrhi::RefCountPtr<IDXGIFactory2>		m_DxgiFactory2;
	nvrhi::RefCountPtr<IDXGIAdapter1>		m_DxgiAdapter1;
	nvrhi::RefCountPtr<ID3D12Device>		m_Device12;
	nvrhi::RefCountPtr<IDXGISwapChain3>		m_SwapChain;

	nvrhi::RefCountPtr<ID3D12CommandQueue>	m_GraphicsQueue;
	nvrhi::RefCountPtr<ID3D12CommandQueue>	m_ComputeQueue;
	nvrhi::RefCountPtr<ID3D12CommandQueue>	m_CopyQueue;

	DXGI_SWAP_CHAIN_DESC1					m_SwapChainDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC			m_FullScreenDesc;
	bool									m_TearingSupported;
	std::vector<nvrhi::RefCountPtr<ID3D12Resource>> m_SwapChainBuffers;
	std::vector<nvrhi::TextureHandle>               m_RhiSwapChainBuffers;

	nvrhi::RefCountPtr<ID3D12Fence>			m_FrameFence;
	std::vector<HANDLE>						m_FrameFenceEvents;

	std::string								m_RendererString;
};

}