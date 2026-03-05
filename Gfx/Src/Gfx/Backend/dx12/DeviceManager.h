#pragma once
#include "Gfx/DeviceManager.h"
#include <dxgi1_6.h>
#include "RHI/dx12/d3d12_headers.h"
#include "RHI/Texture.h"
#include "Core/ComPtr.h"

namespace st::gfx::dx12
{

struct ViewportSwapChain;

class DeviceManager : public st::gfx::DeviceManager
{
public:

	DeviceManager();
	~DeviceManager() override;

	bool ResizeSwapChain() override;

	// Can be called before Init to get a list of available adapters.
	bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override;

	uint64_t BeginFrame() override;
	bool Present() override;

	glm::ivec2 GetWindowDimensions() const override;

	st::rhi::FramebufferHandle GetViewportCurrentFramebuffer(ViewportSwapChainId id) override;
	uint32_t GetViewportCurrentBackBufferIndex(ViewportSwapChainId id) override;
	uint32_t GetCurrentBackBufferIndex() const override;
	st::rhi::TextureHandle GetCurrentBackBuffer() override;
	st::rhi::TextureHandle GetBackBuffer(uint32_t index) override;

	ViewportSwapChainId CreateViewportSwapChain(const ViewportSwapChainInitParams& initParams, const std::string& debugName) override;
	bool ResizeViewportSwapChain(ViewportSwapChainId id) override;
	void DestroyViewportSwapChain(ViewportSwapChainId id) override;

	rhi::ColorSpace GetColorSpace() const override;
	const std::string& GetBackEndHWName() const override { return m_RendererString; }

	void ReportLiveObjects() override;

private:

	bool InternalInit(const DeviceParams& params) override;
	void InternalShutdown() override;

	bool CreateDevice();
	bool CreateSwapChain();

	bool CreateRenderTargets();
	void ReleaseRenderTargets();

	void EnableDRED();

	bool CheckHDRSupport();
	void SetColorSpace(DXGI_FORMAT swapChainFormat, bool allowHdr);

	bool CreateViewportRenderTargets(ViewportSwapChain* vs);
	void PresentViewports();

private:

	ComPtr<IDXGIFactory2>		m_DxgiFactory2;
	ComPtr<IDXGIAdapter1>		m_DxgiAdapter1;
	ComPtr<ID3D12Device>		m_D3d12Device;
	ComPtr<IDXGISwapChain3>		m_SwapChain;

	ComPtr<ID3D12CommandQueue>	m_GraphicsQueue;
	ComPtr<ID3D12CommandQueue>	m_ComputeQueue;
	ComPtr<ID3D12CommandQueue>	m_CopyQueue;

	DXGI_SWAP_CHAIN_DESC1		m_SwapChainDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC	m_FullScreenDesc;
	bool						m_TearingSupported;
	std::vector<st::rhi::TextureOwner> m_SwapChainBuffers;

	ComPtr<ID3D12Fence>			m_FrameFence;
	std::vector<std::pair<HANDLE, uint64_t>> m_FrameFenceEvents;

	DXGI_COLOR_SPACE_TYPE		m_DxgiColorSpace;

	std::unordered_map<ViewportSwapChainId, std::unique_ptr<ViewportSwapChain>> m_ViewportSwapChains;

	std::string					m_RendererString;
};

}