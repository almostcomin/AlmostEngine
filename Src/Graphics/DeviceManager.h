#pragma once

#include "Core/Core.h"
#include <nvrhi/nvrhi.h>

struct SDL_Window;

namespace st::gfx
{

enum class GraphicsAPI : uint8_t
{
    D3D12
};

struct AdapterInfo
{
    typedef std::array<uint8_t, 16> UUID;
    typedef std::array<uint8_t, 8> LUID;

    std::string name;
    uint32_t vendorID = 0;
    uint32_t deviceID = 0;
    uint64_t dedicatedVideoMemory = 0;
    
    bool softwareAdapter = false;
    bool headless = false;

    std::optional<UUID> uuid;
    std::optional<LUID> luid;

    //nvrhi::RefCountPtr<IDXGIAdapter> dxgiAdapter;
    //VkPhysicalDevice vkPhysicalDevice = nullptr;
};

class DeviceManager
{
public:

    struct InitParams
    {
        SDL_Window* m_WindowHandle = nullptr;

        bool StartMaximized = false;        // ignores backbuffer width/height to be monitor size
        bool StartFullscreen = false;
        bool StartBorderless = false;

        bool DebugRuntime = false;
        bool GPUValidation = false;
        bool NvrhiValidationLayer = false;
        bool WarningsAsErrors = false;

        bool EnableComputeQueue = true;
        bool EnableCopyQueue = true;

        int SwapChainSampleCount = 1;
        int SwapChainSampleQuality = 0;
        int SwapChainBufferCount = 3;
        nvrhi::Format SwapChainFormat = nvrhi::Format::SRGBA8_UNORM;

        uint32_t RefreshRate = 0;           // 0 forces the native display's refresh rate

        int AdapterIndex = -1;              // -1 Get default adapter (first in the list)
    };

    static DeviceManager* Create(st::gfx::GraphicsAPI api);

    virtual bool Init(const InitParams& params) = 0;
    virtual void Shutdown() = 0;

    virtual bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) = 0;
    virtual bool CreateDevice() = 0;
    virtual bool CreateSwapChain() = 0;

    virtual void ReportLiveObjects() = 0;

    nvrhi::DeviceHandle GetDevice() { return m_nvrhiDevice; }

protected:

    nvrhi::DeviceHandle m_nvrhiDevice;
};

} // namespace st::graphics