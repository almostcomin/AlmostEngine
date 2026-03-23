#include "Gfx/Backend/dx12/DeviceManager.h"
#include "RHI/dx12/Device.h"
#include "RHI/DxgiFormatMapping.h"
#include "RHI/Format.h"
#include "Gfx/UploadBuffer.h"
#include "Gfx/CommonResources.h"
#include "Gfx/TextureCache.h"
#include "Gfx/DataUploader.h"
#include "Gfx/ShaderFactory.h"
#include <SDL3/SDL_video.h>

using namespace Microsoft::WRL;

#define HR_RETURN(hr, val) if(FAILED(hr)) { LOG_ERROR("HRESULT error code = {:#x}", hr); return val; }

namespace alm::gfx::dx12
{
    struct ViewportSwapChain
    {
        alm::gfx::ViewportSwapChainInitParams InitParams;
        int Width;
        int Height;

        std::vector<alm::rhi::FramebufferOwner> Framebuffers;

        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc;
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC	FullScreenDesc;

        ComPtr<IDXGISwapChain3> SwapChain;
        std::vector<alm::rhi::TextureOwner> Buffers;

        std::string DebugName;
    };
} // namespace st::gfx

namespace
{

std::string GetAdapterName(DXGI_ADAPTER_DESC1 const& aDesc)
{
    size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));

    std::string name;
    name.resize(length);
    WideCharToMultiByte(CP_ACP, 0, aDesc.Description, int(length), name.data(), int(name.size()), nullptr, nullptr);

    return name;
}

} // anonymous namespace

alm::gfx::dx12::DeviceManager::DeviceManager()
{}

alm::gfx::dx12::DeviceManager::~DeviceManager()
{}

bool alm::gfx::dx12::DeviceManager::ResizeSwapChain()
{
    ReleaseRenderTargets();

    m_SwapChainDesc.Width = m_BackBufferWidth;
    m_SwapChainDesc.Height = m_BackBufferHeight;
    const HRESULT hr = m_SwapChain->ResizeBuffers(m_DeviceParams.SwapChainBufferCount,
        m_BackBufferWidth,
        m_BackBufferHeight,
        m_SwapChainDesc.Format,
        m_SwapChainDesc.Flags);

    if (FAILED(hr))
    {
        LOG_FATAL("ResizeBuffers failed");
        return false;
    }

    bool ret = CreateRenderTargets();
    if (!ret)
    {
        LOG_FATAL("CreateRenderTarget failed");
        return false;
    }
    
    return true;
}

bool alm::gfx::dx12::DeviceManager::EnumerateAdapters(std::vector<AdapterInfo>& outAdapters)
{
    if (!m_DxgiFactory2)
        return false;
    outAdapters.clear();

    while (true)
    {
        ComPtr<IDXGIAdapter1> adapter;
        HRESULT hr = m_DxgiFactory2->EnumAdapters1(uint32_t(outAdapters.size()), &adapter);
        if (FAILED(hr))
            return true;

        DXGI_ADAPTER_DESC1 desc;
        hr = adapter->GetDesc1(&desc);
        if (FAILED(hr))
            return false;

        AdapterInfo adapterInfo;

        adapterInfo.name = GetAdapterName(desc);
        //adapterInfo.dxgiAdapter = adapter;
        adapterInfo.vendorID = desc.VendorId;
        adapterInfo.deviceID = desc.DeviceId;
        adapterInfo.dedicatedVideoMemory = desc.DedicatedVideoMemory;
        adapterInfo.softwareAdapter = desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;

        ComPtr<IDXGIOutput> output;
        adapter->EnumOutputs(0, &output);
        adapterInfo.headless = adapter->EnumOutputs(0, &output) == DXGI_ERROR_NOT_FOUND;

        AdapterInfo::LUID luid;
        static_assert(luid.size() == sizeof(desc.AdapterLuid));
        memcpy(luid.data(), &desc.AdapterLuid, luid.size());
        adapterInfo.luid = luid;

        outAdapters.push_back(std::move(adapterInfo));
    }

    return true;
}

uint64_t alm::gfx::dx12::DeviceManager::BeginFrame()
{
    auto bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
    WaitForSingleObject(m_FrameFenceEvents[bufferIndex].first, INFINITE);

    return m_FrameFenceEvents[bufferIndex].second;
}

bool alm::gfx::dx12::DeviceManager::Present()
{
    auto bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

    UINT presentFlags = 0;
    if (!m_DeviceParams.VSyncEnabled && m_FullScreenDesc.Windowed && m_TearingSupported)
        presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

    HRESULT result = m_SwapChain->Present(m_DeviceParams.VSyncEnabled ? 1 : 0, presentFlags);
    if (FAILED(result))
    {
        rhi::dx12::CheckDRED(m_Device->GetNativeDevice());
    }
    assert(SUCCEEDED(result));

    PresentViewports();

    ResetEvent(m_FrameFenceEvents[bufferIndex].first);
    m_FrameFence->SetEventOnCompletion(m_FrameIndex, m_FrameFenceEvents[bufferIndex].first);
    m_FrameFenceEvents[bufferIndex].second = m_FrameIndex;
    m_GraphicsQueue->Signal(m_FrameFence.Get(), m_FrameIndex);

    m_FrameIndex++; // Next frame
    m_Device->NextFrame();

    return SUCCEEDED(result);
}

bool alm::gfx::dx12::DeviceManager::InternalInit(const DeviceParams& params)
{
    m_DeviceParams = params;

    HRESULT hres = CreateDXGIFactory2(m_DeviceParams.DebugRuntime ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&m_DxgiFactory2));
    if (hres != S_OK)
    {
        LOG_ERROR("ERROR in CreateDXGIFactory2.\n"
            "For more info, get log from debug D3D runtime: (1) Install DX SDK, and enable Debug D3D from DX Control Panel Utility. (2) Install and start DbgView. (3) Try running the program again.\n");
        return false;
    }

    CreateDevice();
    CreateSwapChain();

    // reset the back buffer size state to enforce a resize event
    m_BackBufferWidth = 0;
    m_BackBufferHeight = 0;
    UpdateWindowSize();

    return true;
};

void alm::gfx::dx12::DeviceManager::InternalShutdown()
{
    ReleaseRenderTargets();

    for (auto fenceEvent : m_FrameFenceEvents)
    {
        WaitForSingleObject(fenceEvent.first, INFINITE);
        CloseHandle(fenceEvent.first);
    }
    m_FrameFenceEvents.clear();

    m_FrameFence = nullptr;
    m_SwapChain = nullptr;
    m_GraphicsQueue = nullptr;
    m_ComputeQueue = nullptr;
    m_CopyQueue = nullptr;
    m_DxgiAdapter1 = nullptr;
    m_DxgiFactory2 = nullptr;
    m_D3d12Device = nullptr;

    m_RendererString.clear();

    m_Device->Shutdown();
    m_Device.reset();

    if (m_DeviceParams.DebugRuntime)
    {
        ReportLiveObjects();
    }
}

bool alm::gfx::dx12::DeviceManager::CreateDevice()
{
    if (m_DeviceParams.DebugRuntime)
    {
        ComPtr<ID3D12Debug> pDebug;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));

        if (SUCCEEDED(hr))
            pDebug->EnableDebugLayer();
        else
            LOG_WARNING("Cannot enable DX12 debug runtime, ID3D12Debug is not available.");
    }

    if (m_DeviceParams.GPUValidation)
    {
        ComPtr<ID3D12Debug3> debugController3;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController3));

        if (SUCCEEDED(hr))
            debugController3->SetEnableGPUBasedValidation(true);
        else
            LOG_WARNING("Cannot enable GPU-based validation, ID3D12Debug3 is not available.");
    }

    int adapterIndex = m_DeviceParams.AdapterIndex;
    if (adapterIndex < 0)
        adapterIndex = 0;
    if (FAILED(m_DxgiFactory2->EnumAdapters1(adapterIndex, &m_DxgiAdapter1)))
    {
        if (adapterIndex == 0)
            LOG_ERROR("Cannot find any DXGI adapters in the system.");
        else
        {
            LOG_ERROR("The specified DXGI adapter {} does not exist.", adapterIndex);
        }

        return false;
    }

    {
        DXGI_ADAPTER_DESC1 aDesc;
        m_DxgiAdapter1->GetDesc1(&aDesc);
        m_RendererString = GetAdapterName(aDesc);
    }

    //--- Create device
    HRESULT hr = D3D12CreateDevice(
        m_DxgiAdapter1.Get(),
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&m_D3d12Device));

    if (FAILED(hr))
    {
        LOG_ERROR("D3D12CreateDevice failed, error code = 0x%08x", hr);
        return false;
    }

    if (m_DeviceParams.DebugRuntime)
    {
        ComPtr<ID3D12InfoQueue> pInfoQueue;
        m_D3d12Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue));

        if (pInfoQueue)
        {
#ifdef _DEBUG
            if (m_DeviceParams.WarningsAsErrors)
                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif

            D3D12_MESSAGE_ID disableMessageIDs[] = {
                D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH, // descriptor validation doesn't understand acceleration structures
                D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED, // NGX currently generates benign resource creation warnings
            };

            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.pIDList = disableMessageIDs;
            filter.DenyList.NumIDs = sizeof(disableMessageIDs) / sizeof(disableMessageIDs[0]);
            pInfoQueue->AddStorageFilterEntries(&filter);
        }
    }

    //--- Create queues
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc;
        ZeroMemory(&queueDesc, sizeof(queueDesc));
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.NodeMask = 1;
        hr = m_D3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_GraphicsQueue));
        m_GraphicsQueue->SetName(L"Graphics Queue");

        if (m_DeviceParams.EnableComputeQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            hr = m_D3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ComputeQueue));
            HR_RETURN(hr, false);
            m_ComputeQueue->SetName(L"Compute Queue");
        }

        if (m_DeviceParams.EnableCopyQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            hr = m_D3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CopyQueue));
            HR_RETURN(hr, false);
            m_CopyQueue->SetName(L"Copy Queue");
        }
    }

    alm::rhi::dx12::DeviceDesc deviceDesc;

    deviceDesc.pDevice = m_D3d12Device.Get();
    deviceDesc.pGraphicsCommandQueue = m_GraphicsQueue.Get();
    deviceDesc.pComputeCommandQueue = m_ComputeQueue.Get();
    deviceDesc.pCopyCommandQueue = m_CopyQueue.Get();

    m_Device = alm::rhi::dx12::CreateDevice(deviceDesc);

    return true;
}

bool alm::gfx::dx12::DeviceManager::CreateSwapChain()
{
    if (SDL_GetWindowSize(m_DeviceParams.WindowHandle, (int*)&m_BackBufferWidth, (int*)&m_BackBufferHeight) == false)
    {
        LOG_ERROR("SDL_GetWindowSize failed");
        return false;
    }

    if (m_DeviceParams.SwapChainFormat == rhi::Format::UNKNOWN)
    {
        m_DeviceParams.SwapChainFormat = m_DeviceParams.ForceSDR ? 
            rhi::Format::SRGBA8_UNORM : rhi::Format::R10G10B10A2_UNORM;
    }

    m_SwapChainDesc = {};
    m_SwapChainDesc.Width = m_BackBufferWidth;
    m_SwapChainDesc.Height = m_BackBufferHeight;
    m_SwapChainDesc.SampleDesc.Count = m_DeviceParams.SwapChainSampleCount;
    m_SwapChainDesc.SampleDesc.Quality = m_DeviceParams.SwapChainSampleQuality;
    m_SwapChainDesc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    m_SwapChainDesc.BufferCount = m_DeviceParams.SwapChainBufferCount;
    m_SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    m_SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Special processing for sRGB swap chain formats.
    // DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
    // So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.
    switch (m_DeviceParams.SwapChainFormat)
    {
    case alm::rhi::Format::SRGBA8_UNORM:
        m_SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case alm::rhi::Format::SBGRA8_UNORM:
        m_SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    default:        
        m_SwapChainDesc.Format = alm::rhi::GetDxgiFormatMapping(m_DeviceParams.SwapChainFormat).srvFormat;
        break;
    }

    ComPtr<IDXGIFactory5> pDxgiFactory5;
    m_TearingSupported = false;
    if (SUCCEEDED(m_DxgiFactory2->QueryInterface(IID_PPV_ARGS(&pDxgiFactory5))))
    {
        BOOL supported = 0;
        if (SUCCEEDED(pDxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))))
            m_TearingSupported = (supported != 0);
    }
    if (m_TearingSupported)
    {
        m_SwapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    m_FullScreenDesc = {};
    m_FullScreenDesc.RefreshRate.Numerator = m_DeviceParams.RefreshRate;
    m_FullScreenDesc.RefreshRate.Denominator = 1;
    m_FullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    m_FullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    m_FullScreenDesc.Windowed = !m_DeviceParams.StartFullscreen;

    SDL_PropertiesID windowProps = SDL_GetWindowProperties(m_DeviceParams.WindowHandle);
    HWND hWnd = (HWND)SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    ComPtr<IDXGISwapChain1> pSwapChain1;
    HRESULT hr = m_DxgiFactory2->CreateSwapChainForHwnd(m_GraphicsQueue.Get(), hWnd, &m_SwapChainDesc, &m_FullScreenDesc, nullptr, &pSwapChain1);
    HR_RETURN(hr, false);

    hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
    HR_RETURN(hr, false);

    const bool hdr = !m_DeviceParams.ForceSDR && CheckHDRSupport();
    SetColorSpace(m_SwapChainDesc.Format, hdr);

    if (!CreateRenderTargets())
        return false;

    hr = m_D3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FrameFence));
    HR_RETURN(hr, false);

    for (UINT bufferIndex = 0; bufferIndex < m_SwapChainDesc.BufferCount; bufferIndex++)
    {
        m_FrameFenceEvents.push_back({ CreateEvent(nullptr, true, true, nullptr), 0 });
    }

    return true;
}

alm::rhi::ColorSpace alm::gfx::dx12::DeviceManager::GetColorSpace() const
{
    switch (m_DxgiColorSpace)
    {
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
        return rhi::ColorSpace::SRGB;
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        return rhi::ColorSpace::HDR10_ST2084;
    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        return rhi::ColorSpace::HDR_LINEAR;
    }

    // Not initialized?
    assert(0);
    return rhi::ColorSpace::SRGB;
}

void alm::gfx::dx12::DeviceManager::ReportLiveObjects()
{
    ComPtr<IDXGIDebug> pDebug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));

    if (pDebug)
    {
        DXGI_DEBUG_RLO_FLAGS flags = (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_IGNORE_INTERNAL | DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL);
        HRESULT hr = pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);
        if (FAILED(hr))
        {
            LOG_ERROR("ReportLiveObjects failed, HRESULT = 0x%08x", hr);
        }
    }
}

glm::ivec2 alm::gfx::dx12::DeviceManager::GetWindowDimensions() const
{
    return { m_SwapChainDesc.Width, m_SwapChainDesc.Height };
}

alm::rhi::FramebufferHandle alm::gfx::dx12::DeviceManager::GetViewportCurrentFramebuffer(ViewportSwapChainId id)
{
    auto it = m_ViewportSwapChains.find(id);
    assert(it != m_ViewportSwapChains.end());

    return it->second->Framebuffers[it->second->SwapChain->GetCurrentBackBufferIndex()].get_weak();
}

uint32_t alm::gfx::dx12::DeviceManager::GetCurrentBackBufferIndex() const
{
    return m_SwapChain->GetCurrentBackBufferIndex();
}

uint32_t alm::gfx::dx12::DeviceManager::GetViewportCurrentBackBufferIndex(ViewportSwapChainId id)
{
    auto it = m_ViewportSwapChains.find(id);
    assert(it != m_ViewportSwapChains.end());

    return it->second->SwapChain->GetCurrentBackBufferIndex();
}

alm::rhi::TextureHandle alm::gfx::dx12::DeviceManager::GetCurrentBackBuffer()
{
    return m_SwapChainBuffers[m_SwapChain->GetCurrentBackBufferIndex()].get_weak();
}

alm::rhi::TextureHandle alm::gfx::dx12::DeviceManager::GetBackBuffer(uint32_t index)
{
    return m_SwapChainBuffers[index].get_weak();
}

alm::gfx::ViewportSwapChainId alm::gfx::dx12::DeviceManager::CreateViewportSwapChain(const ViewportSwapChainInitParams& initParams, const std::string& debugName)
{
    std::unique_ptr<ViewportSwapChain> vpsc{ new ViewportSwapChain };

    vpsc->InitParams = initParams;
    vpsc->DebugName = debugName;

    if (SDL_GetWindowSize(initParams.WindowHandle, (int*)&vpsc->Width, (int*)&vpsc->Height) == false)
    {
        LOG_ERROR("SDL_GetWindowSize failed");
        return nullptr;
    }

    vpsc->SwapChainDesc = m_SwapChainDesc;
    vpsc->SwapChainDesc.Width = vpsc->Width;
    vpsc->SwapChainDesc.Height = vpsc->Height;
    vpsc->SwapChainDesc.Format = alm::rhi::GetDxgiFormatMapping(initParams.Format).srvFormat;

    vpsc->FullScreenDesc = m_FullScreenDesc;
    vpsc->FullScreenDesc.Windowed = true;

    SDL_PropertiesID windowProps = SDL_GetWindowProperties(initParams.WindowHandle);
    HWND hWnd = (HWND)SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    ComPtr<IDXGISwapChain1> pSwapChain1;
    HRESULT hr = m_DxgiFactory2->CreateSwapChainForHwnd(m_GraphicsQueue.Get(), hWnd, &vpsc->SwapChainDesc, &vpsc->FullScreenDesc, nullptr, &pSwapChain1);
    HR_RETURN(hr, nullptr);

    hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&vpsc->SwapChain));
    HR_RETURN(hr, nullptr);

    // Create viewport render targets
    CreateViewportRenderTargets(vpsc.get());

    ViewportSwapChainId id = vpsc.get();
    m_ViewportSwapChains.emplace(id, std::move(vpsc));

    return id;
}

bool alm::gfx::dx12::DeviceManager::ResizeViewportSwapChain(ViewportSwapChainId id)
{
    auto it = m_ViewportSwapChains.find(id);
    assert(it != m_ViewportSwapChains.end());

    ViewportSwapChain* vs = it->second.get();

    if (m_Device)
    {
        // Make sure that all frames have finished rendering
        m_Device->WaitForIdle();
    }
    // Set the events so that WaitForSingleObject in OneFrame will not hang later
    for (auto e : m_FrameFenceEvents)
        SetEvent(e.first);

    for (auto& fb : vs->Framebuffers)
    {
        m_Device->ReleaseImmediately(std::move(fb));
    }
    vs->Framebuffers.clear();

    for (auto& texture : vs->Buffers)
    {
        m_Device->ReleaseImmediately(std::move(texture));
    }
    vs->Buffers.clear();

    if (SDL_GetWindowSize(vs->InitParams.WindowHandle, (int*)&vs->Width, (int*)&vs->Height) == false)
    {
        LOG_ERROR("SDL_GetWindowSize failed");
        return false;
    }

    vs->SwapChainDesc.Width = vs->Width;
    vs->SwapChainDesc.Height = vs->Height;
    const HRESULT hr = vs->SwapChain->ResizeBuffers(
        m_DeviceParams.SwapChainBufferCount,
        vs->SwapChainDesc.Width,
        vs->SwapChainDesc.Height,
        vs->SwapChainDesc.Format,
        vs->SwapChainDesc.Flags);
    if (FAILED(hr))
    {
        LOG_FATAL("Swapchain ResizeBuffers failed");
        return false;
    }

    if(!CreateViewportRenderTargets(vs))
    {
        LOG_FATAL("CreateRenderTarget failed");
        return false;
    }

    return true;
}

void alm::gfx::dx12::DeviceManager::DestroyViewportSwapChain(ViewportSwapChainId id)
{
    auto it = m_ViewportSwapChains.find(id);
    assert(it != m_ViewportSwapChains.end());

    m_ViewportSwapChains.erase(it);
}

bool alm::gfx::dx12::DeviceManager::CreateRenderTargets()
{
    m_SwapChainBuffers.resize(m_SwapChainDesc.BufferCount);

    for (UINT n = 0; n < m_SwapChainDesc.BufferCount; n++)
    {
        ComPtr<ID3D12Resource> nativeBuffer;
        const HRESULT hr = m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&nativeBuffer));
        HR_RETURN(hr, false);        
        
        rhi::TextureDesc textureDesc;
        textureDesc.width = m_SwapChainDesc.Width;
        textureDesc.height = m_SwapChainDesc.Height;
        textureDesc.sampleCount = m_DeviceParams.SwapChainSampleCount;
        textureDesc.sampleQuality = m_DeviceParams.SwapChainSampleQuality;
        textureDesc.format = m_DeviceParams.SwapChainFormat;
        textureDesc.shaderUsage = rhi::TextureShaderUsage::ColorTarget;

        std::stringstream ss;
        ss << "SwapChainBackBuffer[" << n << "]";

        m_SwapChainBuffers[n] = m_Device->CreateHandleForNativeTexture(
            nativeBuffer.Get(), textureDesc, ss.str());
    }

    return true;
}

void alm::gfx::dx12::DeviceManager::ReleaseRenderTargets()
{
    if (m_Device)
    {
        // Make sure that all frames have finished rendering
        m_Device->WaitForIdle();
    }

    // Set the events so that WaitForSingleObject in OneFrame will not hang later
    for (auto e : m_FrameFenceEvents)
        SetEvent(e.first);

    // Release the old buffers because ResizeBuffers requires that
    for (auto& texture : m_SwapChainBuffers)
    {
        m_Device->ReleaseImmediately(std::move(texture));
    }
    m_SwapChainBuffers.clear();
}

void alm::gfx::dx12::DeviceManager::EnableDRED()
{
    ID3D12DeviceRemovedExtendedDataSettings1* pDredSettings;

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings)))) 
    {
        pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
}

bool alm::gfx::dx12::DeviceManager::CheckHDRSupport()
{
    // HDR display query: https://docs.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range

    ComPtr<IDXGIOutput> dxgiOutput;
    if (SUCCEEDED(m_SwapChain->GetContainingOutput(&dxgiOutput)))
    {
        ComPtr<IDXGIOutput6> output6;
        if(SUCCEEDED(dxgiOutput->QueryInterface(IID_PPV_ARGS(&output6))))
        {
            DXGI_OUTPUT_DESC1 desc1;
            if (SUCCEEDED(output6->GetDesc1(&desc1)))
            {
                if (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void alm::gfx::dx12::DeviceManager::SetColorSpace(DXGI_FORMAT swapChainFormat, bool allowHdr)
{
    // Ensure correct color space:
    //	https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HDR/src/D3D12HDR.cpp

    m_DxgiColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709; // fallback

    DXGI_COLOR_SPACE_TYPE result;
    switch (swapChainFormat)
    {
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        // This format is either HDR10 (ST.2084), or SDR (SRGB)
        result = allowHdr ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        // This format is HDR (Linear):
        result = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
        break;
    default:
        // Anything else will be SDR (SRGB):
        result = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        break;
    }

    UINT colorSpaceSupport = 0;
    if (SUCCEEDED(m_SwapChain->CheckColorSpaceSupport(result, &colorSpaceSupport)))
    {
        if (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
        {
            HRESULT hr = m_SwapChain->SetColorSpace1(result);
            if (SUCCEEDED(hr))
            {
                m_DxgiColorSpace = result;
            }
        }
    }
}

bool alm::gfx::dx12::DeviceManager::CreateViewportRenderTargets(ViewportSwapChain* vs)
{
    vs->Buffers.resize(vs->SwapChainDesc.BufferCount);
    vs->Framebuffers.resize(vs->SwapChainDesc.BufferCount);

    for (UINT n = 0; n < vs->Buffers.size(); n++)
    {
        ComPtr<ID3D12Resource> nativeBuffer;
        const HRESULT hr = vs->SwapChain->GetBuffer(n, IID_PPV_ARGS(&nativeBuffer));
        HR_RETURN(hr, false);

        rhi::TextureDesc textureDesc;
        textureDesc.width = vs->Width;
        textureDesc.height = vs->Height;
        textureDesc.sampleCount = vs->SwapChainDesc.SampleDesc.Count;
        textureDesc.sampleQuality = vs->SwapChainDesc.SampleDesc.Quality;
        textureDesc.format = vs->InitParams.Format;
        textureDesc.shaderUsage = rhi::TextureShaderUsage::ColorTarget;

        vs->Buffers[n] = m_Device->CreateHandleForNativeTexture(
            nativeBuffer.Get(), textureDesc, std::format("{} - BackBuffer[{}]", vs->DebugName, n));

        vs->Framebuffers[n] = m_Device->CreateFramebuffer(alm::rhi::FramebufferDesc()
            .AddColorAttachment(vs->Buffers[n].get_weak()), std::format("Viewport Swapchain FrameBuffer[{}]", n));
    }

    return true;
}

void alm::gfx::dx12::DeviceManager::PresentViewports()
{
    for (const auto& vp : m_ViewportSwapChains)
    {
        vp.second->SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
}