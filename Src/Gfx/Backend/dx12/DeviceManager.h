#pragma once
#include "Gfx/DeviceManager.h"
#include <dxgi1_5.h>
#include <directx/d3d12.h>
#include <wrl/client.h>
#include "RenderAPI/Texture.h"

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

	Microsoft::WRL::ComPtr<IDXGIFactory2>		m_DxgiFactory2;
	Microsoft::WRL::ComPtr<IDXGIAdapter1>		m_DxgiAdapter1;
	Microsoft::WRL::ComPtr<ID3D12Device>		m_D3d12Device;
	Microsoft::WRL::ComPtr<IDXGISwapChain3>		m_SwapChain;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue>	m_GraphicsQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>	m_ComputeQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>	m_CopyQueue;

	DXGI_SWAP_CHAIN_DESC1						m_SwapChainDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC				m_FullScreenDesc;
	bool										m_TearingSupported;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_SwapChainNativeBuffers;
	std::vector<st::rapi::TextureHandle>		m_SwapChainBuffers;

	Microsoft::WRL::ComPtr<ID3D12Fence>			m_FrameFence;
	std::vector<HANDLE>							m_FrameFenceEvents;

	std::string									m_RendererString;
};

}