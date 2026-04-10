#include "Gfx/GfxPCH.h"
#include "Gfx/TextureLoader.h"
#include "RHI/dx12/GpuDevice.h"
#include "RHI/dx12/ResourceState.h"
#include <DirectXTex.h>
#include <filesystem>

bool alm::gfx::SaveDDSTexture(rhi::TextureHandle texture, rhi::ResourceState currentState, rhi::ResourceState targetState, rhi::Device* device,
    const std::string& filename)
{
    ID3D12Resource* nativeTexture = texture->GetNativeResource();
    assert(nativeTexture);
    auto* gpuDevice = checked_cast<alm::rhi::dx12::GpuDevice*>(device);
    ID3D12CommandQueue* commandQueue = gpuDevice->GetQueue(alm::rhi::QueueType::Graphics)->d3d12Queue.Get();
    assert(commandQueue);

    DirectX::ScratchImage image;
    HRESULT hr = DirectX::CaptureTexture(commandQueue, nativeTexture, false, image,
        rhi::dx12::MapResourceState(currentState), rhi::dx12::MapResourceState(targetState));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed capturing texture, hr [{:#x}]", hr);
        return false;
    }

    std::wstring ws_filename = ToWide(filename.c_str());
    hr = DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, ws_filename.c_str());
    if (FAILED(hr))
    {
        LOG_ERROR("Failed saving texture, hr [{:#x}]", hr);
        return false;
    }

    return true;
}
