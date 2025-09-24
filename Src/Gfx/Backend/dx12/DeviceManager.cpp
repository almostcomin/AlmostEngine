
#include "Gfx/Backend/dx12/DeviceManager.h"

#include <dxgidebug.h>
#include <nvrhi/d3d12.h>
#include <nvrhi/validation.h>
#include <SDL3/SDL_video.h>
#include "Core/Log.h"

#define HR_RETURN(hr) if(FAILED(hr)) { LOG_ERROR("HRESULT error code = 0x%08x", hr); return false; }

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

struct GfxMessageCallback : public nvrhi::IMessageCallback
{
    void message(nvrhi::MessageSeverity severity, const char* messageText) override
    {
        switch (severity)
        {
        case nvrhi::MessageSeverity::Info:
            LOG_INFO(messageText);
            break;
        case nvrhi::MessageSeverity::Warning:
            LOG_WARNING(messageText);
            break;
        case nvrhi::MessageSeverity::Error:
            LOG_ERROR(messageText);
            break;
        case nvrhi::MessageSeverity::Fatal:
            LOG_FATAL(messageText);
            break;
        }
    }
};
GfxMessageCallback g_GfxMessageCallback;

bool st::gfx::dx12::DeviceManager::Init(const DeviceParams& params)
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

void st::gfx::dx12::DeviceManager::Shutdown()
{
    st::gfx::DeviceManager::Shutdown();

    ReleaseRenderTargets();

    m_nvrhiDevice = nullptr;

    for (auto fenceEvent : m_FrameFenceEvents)
    {
        WaitForSingleObject(fenceEvent, INFINITE);
        CloseHandle(fenceEvent);
    }
    m_FrameFenceEvents.clear();

    m_FrameFence = nullptr;
    m_SwapChain = nullptr;
    m_GraphicsQueue = nullptr;
    m_ComputeQueue = nullptr;
    m_CopyQueue = nullptr;
    m_Device12 = nullptr;
    m_DxgiAdapter1 = nullptr;
    m_DxgiFactory2 = nullptr;

    m_RendererString.clear();

    if (m_DeviceParams.DebugRuntime)
    {
        ReportLiveObjects();
    }
}

bool st::gfx::dx12::DeviceManager::ResizeSwapChain()
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

bool st::gfx::dx12::DeviceManager::EnumerateAdapters(std::vector<AdapterInfo>& outAdapters)
{
    if (!m_DxgiFactory2)
        return false;
    outAdapters.clear();

    while (true)
    {
        nvrhi::RefCountPtr<IDXGIAdapter1> adapter;
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

        nvrhi::RefCountPtr<IDXGIOutput> output;
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

bool st::gfx::dx12::DeviceManager::BeginFrame()
{
    auto bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
    WaitForSingleObject(m_FrameFenceEvents[bufferIndex], INFINITE);
    return true;
}

bool st::gfx::dx12::DeviceManager::Present()
{
    auto bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

    UINT presentFlags = 0;
    if (!m_DeviceParams.VSyncEnabled && m_FullScreenDesc.Windowed && m_TearingSupported)
        presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

    HRESULT result = m_SwapChain->Present(m_DeviceParams.VSyncEnabled ? 1 : 0, presentFlags);

    m_FrameFence->SetEventOnCompletion(m_FrameCount, m_FrameFenceEvents[bufferIndex]);
    m_GraphicsQueue->Signal(m_FrameFence, m_FrameCount);
    m_FrameCount++;

    return SUCCEEDED(result);
}

bool st::gfx::dx12::DeviceManager::CreateDevice()
{
    if (m_DeviceParams.DebugRuntime)
    {
        nvrhi::RefCountPtr<ID3D12Debug> pDebug;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));

        if (SUCCEEDED(hr))
            pDebug->EnableDebugLayer();
        else
            LOG_WARNING("Cannot enable DX12 debug runtime, ID3D12Debug is not available.");
    }

    if (m_DeviceParams.GPUValidation)
    {
        nvrhi::RefCountPtr<ID3D12Debug3> debugController3;
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
        m_DxgiAdapter1,
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&m_Device12));

    if (FAILED(hr))
    {
        LOG_ERROR("D3D12CreateDevice failed, error code = 0x%08x", hr);
        return false;
    }

    if (m_DeviceParams.DebugRuntime)
    {
        nvrhi::RefCountPtr<ID3D12InfoQueue> pInfoQueue;
        m_Device12->QueryInterface(&pInfoQueue);

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
        hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_GraphicsQueue));
        m_GraphicsQueue->SetName(L"Graphics Queue");

        if (m_DeviceParams.EnableComputeQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ComputeQueue));
            HR_RETURN(hr);
            m_ComputeQueue->SetName(L"Compute Queue");
        }

        if (m_DeviceParams.EnableCopyQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CopyQueue));
            HR_RETURN(hr);
            m_CopyQueue->SetName(L"Copy Queue");
        }
    }

    nvrhi::d3d12::DeviceDesc deviceDesc;
    deviceDesc.errorCB = &g_GfxMessageCallback;
    deviceDesc.pDevice = m_Device12;
    deviceDesc.pGraphicsCommandQueue = m_GraphicsQueue;
    deviceDesc.pComputeCommandQueue = m_ComputeQueue;
    deviceDesc.pCopyCommandQueue = m_CopyQueue;
    deviceDesc.logBufferLifetime = false;// m_DeviceParams.logBufferLifetime;
    deviceDesc.enableHeapDirectlyIndexed = true;// m_DeviceParams.enableHeapDirectlyIndexed;

    m_nvrhiDevice = nvrhi::d3d12::createDevice(deviceDesc);

    if (m_DeviceParams.NvrhiValidationLayer)
    {
        m_nvrhiDevice = nvrhi::validation::createValidationLayer(m_nvrhiDevice);
    }

    return true;
}

bool st::gfx::dx12::DeviceManager::CreateSwapChain()
{
    if (SDL_GetWindowSize(m_DeviceParams.WindowHandle, (int*)&m_BackBufferWidth, (int*)&m_BackBufferHeight) == false)
    {
        LOG_ERROR("SDL_GetWindowSize failed");
        return false;
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
    switch (m_DeviceParams.SwapChainFormat)  // NOLINT(clang-diagnostic-switch-enum)
    {
    case nvrhi::Format::SRGBA8_UNORM:
        m_SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case nvrhi::Format::SBGRA8_UNORM:
        m_SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    default:
        m_SwapChainDesc.Format = nvrhi::d3d12::convertFormat(m_DeviceParams.SwapChainFormat);
        break;
    }

    nvrhi::RefCountPtr<IDXGIFactory5> pDxgiFactory5;
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

    nvrhi::RefCountPtr<IDXGISwapChain1> pSwapChain1;
    HRESULT hr = m_DxgiFactory2->CreateSwapChainForHwnd(m_GraphicsQueue, hWnd, &m_SwapChainDesc, &m_FullScreenDesc, nullptr, &pSwapChain1);
    HR_RETURN(hr);

    hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
    HR_RETURN(hr);

    if (!CreateRenderTargets())
        return false;

    hr = m_Device12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FrameFence));
    HR_RETURN(hr);

    for (UINT bufferIndex = 0; bufferIndex < m_SwapChainDesc.BufferCount; bufferIndex++)
    {
        m_FrameFenceEvents.push_back(CreateEvent(nullptr, false, true, nullptr));
    }

    return true;
}

void st::gfx::dx12::DeviceManager::ReportLiveObjects()
{
    nvrhi::RefCountPtr<IDXGIDebug> pDebug;
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

glm::ivec2 st::gfx::dx12::DeviceManager::GetWindowDimensions() const
{
    return { m_SwapChainDesc.Width, m_SwapChainDesc.Height };
}

uint32_t st::gfx::dx12::DeviceManager::GetCurrentBackBufferIndex() const
{
    return m_SwapChain->GetCurrentBackBufferIndex();
}

nvrhi::ITexture* st::gfx::dx12::DeviceManager::GetCurrentBackBuffer()
{
    return m_SwapChainBuffers[m_SwapChain->GetCurrentBackBufferIndex()].Get();
}

nvrhi::ITexture* st::gfx::dx12::DeviceManager::GetBackBuffer(uint32_t index)
{
    return m_SwapChainBuffers[index].Get();
}

bool st::gfx::dx12::DeviceManager::CreateRenderTargets()
{
    m_SwapChainNativeBuffers.resize(m_SwapChainDesc.BufferCount);
    m_SwapChainBuffers.resize(m_SwapChainDesc.BufferCount);

    for (UINT n = 0; n < m_SwapChainDesc.BufferCount; n++)
    {
        const HRESULT hr = m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_SwapChainNativeBuffers[n]));
        HR_RETURN(hr)

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = m_SwapChainDesc.Width;
        textureDesc.height = m_SwapChainDesc.Height;
        textureDesc.sampleCount = m_DeviceParams.SwapChainSampleCount;
        textureDesc.sampleQuality = m_DeviceParams.SwapChainSampleQuality;
        textureDesc.format = m_DeviceParams.SwapChainFormat;
        textureDesc.debugName = "SwapChainBuffer";
        textureDesc.isRenderTarget = true;
        textureDesc.isUAV = false;
        textureDesc.initialState = nvrhi::ResourceStates::Present;
        textureDesc.keepInitialState = true;

        m_SwapChainBuffers[n] = m_nvrhiDevice->createHandleForNativeTexture(
            nvrhi::ObjectTypes::D3D12_Resource, nvrhi::Object(m_SwapChainNativeBuffers[n]), textureDesc);
    }

    return true;
}

void st::gfx::dx12::DeviceManager::ReleaseRenderTargets()
{
    if (m_nvrhiDevice)
    {
        // Make sure that all frames have finished rendering
        m_nvrhiDevice->waitForIdle();

        // Release all in-flight references to the render targets
        m_nvrhiDevice->runGarbageCollection();
    }

    // Set the events so that WaitForSingleObject in OneFrame will not hang later
    for (auto e : m_FrameFenceEvents)
        SetEvent(e);

    // Release the old buffers because ResizeBuffers requires that
    m_SwapChainBuffers.clear();
    m_SwapChainNativeBuffers.clear();
}