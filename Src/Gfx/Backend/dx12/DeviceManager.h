#pragma once
#include "Gfx/DeviceManager.h"
#include <nvrhi/nvrhi.h>
#include <dxgi1_5.h>
#include <directx/d3d12.h>

namespace st::gfx::dx12
{

class DeviceManager : public st::gfx::DeviceManager
{
public:

	bool Init(const DeviceParams& params) override;
	void Shutdown() override;

	bool ResizeSwapChain() override;

	// Can be called before Init to get a list of available adapters.
	bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override;

	bool BeginFrame() override;
	bool Present() override;

	glm::ivec2 GetWindowDimensions() const override;

	uint32_t GetCurrentBackBufferIndex() const override;
	nvrhi::ITexture* GetCurrentBackBuffer();
	nvrhi::ITexture* GetBackBuffer(uint32_t index);

	void ReportLiveObjects() override;

private:

	bool CreateDevice();
	bool CreateSwapChain();

	bool CreateRenderTargets();
	void ReleaseRenderTargets();

private:

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
	std::vector<nvrhi::RefCountPtr<ID3D12Resource>> m_SwapChainNativeBuffers;
	std::vector<nvrhi::TextureHandle>		m_SwapChainBuffers;

	nvrhi::RefCountPtr<ID3D12Fence>			m_FrameFence;
	std::vector<HANDLE>						m_FrameFenceEvents;

	std::string								m_RendererString;
};

}