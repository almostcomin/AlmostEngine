#pragma once

#include <optional>
#include <memory>
#include "Core/Math/glm_config.h"
#include "Core/Common.h"
#include "RHI/Format.h"
#include "RHI/FrameBuffer.h"

struct SDL_Window;

namespace st::gfx
{
    class ShaderFactory;
    class DataUploader;
    class TextureCache;
    class CommonResources;
    class UploadBuffer;
}

namespace st::rhi
{
    class Device;
    class ITexture;
}

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
};

class DeviceManager : st::noncopyable_nonmovable
{
public:

    struct DeviceParams
    {
        SDL_Window* WindowHandle = nullptr;

        bool StartMaximized = false;        // ignores backbuffer width/height to be monitor size
        bool StartFullscreen = false;
        bool StartBorderless = false;

        bool DebugRuntime = false;
        bool GPUValidation = false;
        bool WarningsAsErrors = false;

        bool VSyncEnabled = false;

        bool EnableComputeQueue = true;
        bool EnableCopyQueue = true;

        int SwapChainSampleCount = 1;
        int SwapChainSampleQuality = 0;
        int SwapChainBufferCount = 3;
        // Format can be forced setting it here. If UNKNOWN then will be set automatically to SRGBA8_UNORM (sdr) or R10G10B10A2_UNORM (hdr)
        st::rhi::Format SwapChainFormat = st::rhi::Format::UNKNOWN;
        bool ForceSDR = false; // Force SDR or allow hdr. Only if SwapChainFormat is set to UNKNOWN

        uint32_t RefreshRate = 0;           // 0 forces the native display's refresh rate

        int AdapterIndex = -1;              // -1 Get default adapter (first in the list)
    };

    static DeviceManager* Create(st::gfx::GraphicsAPI api);

    virtual ~DeviceManager();

    bool Init(const DeviceParams& params);
    void Shutdown();

    void Update();

    virtual bool ResizeSwapChain() = 0;

    virtual bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) = 0;

    // Begin frame checks for the completion of the next frame index and waits for it if needed
    // It does not update frameCount. That is done after present.
    // Returns the frame index completed.
    virtual uint64_t BeginFrame() = 0;
    virtual bool Present() = 0;

    virtual void ReportLiveObjects() = 0;

    virtual glm::ivec2 GetWindowDimensions() const = 0;
    
    st::rhi::FramebufferHandle GetCurrentFramebuffer();

    virtual uint32_t GetCurrentBackBufferIndex() const = 0;
    virtual st::rhi::TextureHandle GetCurrentBackBuffer() = 0;
    virtual st::rhi::TextureHandle GetBackBuffer(uint32_t index) = 0;
    
    bool IsWindowVisible() const { return m_WindowVisible; }

    // Returns true if window size has changed, else returns false
    // New window size can be obtained calling GetWindowDimensions
    bool UpdateWindowSize();

    void Render(std::function<void(void)> cb);

    st::gfx::ShaderFactory* GetShaderFactory() { return m_ShaderFactory.get(); }
    st::gfx::DataUploader* GetDataUploader() { return m_DataUploader.get(); }
    st::gfx::TextureCache* GetTextureCache() { return m_TextureCache.get(); }
    st::gfx::CommonResources* GetCommonResources() { return m_CommonResources.get(); }
    st::gfx::UploadBuffer* GetUploadBuffer() { return m_UploadBuffer.get(); }

    uint64_t GetFrameIndex() const { return m_FrameIndex; }
    uint32_t GetSwapchainBufferCount() const { return m_SwapChainFramebuffers.size(); }
    uint32_t GetFrameModuleIndex() const;

    virtual rhi::ColorSpace GetColorSpace() const = 0;
    virtual const std::string& GetBackEndHWName() const = 0;

    float GetGPUFrameTime();

    st::rhi::Device* GetDevice() { return m_Device.get(); }

protected:

    DeviceManager() = default;

    DeviceParams m_DeviceParams;

    // Starts with one to avoid potential issues
    // To be updated by backend implementations
    uint64_t m_FrameIndex = 1;

    std::vector<st::rhi::FramebufferOwner> m_SwapChainFramebuffers;

    uint32_t m_BackBufferWidth = 0;
    uint32_t m_BackBufferHeight = 0;
    bool m_WindowVisible = true;

    std::unique_ptr<st::rhi::Device> m_Device;

private:

    virtual bool InternalInit(const DeviceParams& params) = 0;
    virtual void InternalShutdown() = 0;

private:

    std::unique_ptr<st::gfx::ShaderFactory> m_ShaderFactory;
    std::unique_ptr<st::gfx::DataUploader> m_DataUploader;
    std::unique_ptr<st::gfx::TextureCache> m_TextureCache;
    std::unique_ptr<st::gfx::CommonResources> m_CommonResources;
    std::unique_ptr<st::gfx::UploadBuffer> m_UploadBuffer;

    static const uint32_t QueuedFramesCount = 3;
    rhi::TimerQueryOwner m_FrameTimers[QueuedFramesCount];
    int m_NextTimerToUse = 0;

    // Begin & End command lists
    std::vector<rhi::CommandListOwner> m_BeginCommandLists;
    std::vector<rhi::CommandListOwner> m_EndCommandLists;
};

} // namespace st::graphics