#pragma once
#include "Gfx/DeviceManager.h"
#include <dxgi1_5.h>
#include "RenderAPI/dx12/d3d12_headers.h"
#include "RenderAPI/Texture.h"
#include "Core/ComPtr.h"

namespace st::gfx::dx12
{

class DeviceManager : public st::gfx::DeviceManager
{
public:

	bool ResizeSwapChain() override;

	// Can be called before Init to get a list of available adapters.
	bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override;

	bool BeginFrame() override;
	bool Present() override;

	glm::ivec2 GetWindowDimensions() const override;

	uint32_t GetCurrentBackBufferIndex() const override;
	st::rapi::ITexture* GetCurrentBackBuffer() override;
	st::rapi::ITexture* GetBackBuffer(uint32_t index) override;

	void ReportLiveObjects() override;

private:

	bool InternalInit(const DeviceParams& params) override;
	void InternalShutdown() override;

	bool CreateDevice();
	bool CreateSwapChain();

	bool CreateRenderTargets();
	void ReleaseRenderTargets();

private:

	ComPtr<IDXGIFactory2>		m_DxgiFactory2;
	ComPtr<IDXGIAdapter1>		m_DxgiAdapter1;
	ComPtr<ID3D12Device>		m_D3d12Device;
	ComPtr<IDXGISwapChain3>		m_SwapChain;

	ComPtr<ID3D12CommandQueue>	m_GraphicsQueue;
	ComPtr<ID3D12CommandQueue>	m_ComputeQueue;
	ComPtr<ID3D12CommandQueue>	m_CopyQueue;

	DXGI_SWAP_CHAIN_DESC1						m_SwapChainDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC				m_FullScreenDesc;
	bool										m_TearingSupported;
	std::vector<ComPtr<ID3D12Resource>> m_SwapChainNativeBuffers;
	std::vector<st::rapi::TextureHandle>		m_SwapChainBuffers;

	ComPtr<ID3D12Fence>			m_FrameFence;
	std::vector<HANDLE>							m_FrameFenceEvents;

	std::string									m_RendererString;
};

}