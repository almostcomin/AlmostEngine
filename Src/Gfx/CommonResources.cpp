#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"

//#include <d3d12.h>
//#include <d3d12shader.h>
//#include <d3dx12.h>
//#include <wrl/client.h>
//#include <vector>
//#include <stdexcept>

namespace
{
	ID3D12RootSignature* CreateBindlessRootSignature()
	{
#if 0
        // --- Root constants: 64 x 32-bit constants en b0, visibles a todos los shaders
        CD3DX12_ROOT_PARAMETER1 rootParam{};
        rootParam.InitAsConstants(64, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

        // --- Static samplers (s0..s9)
        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[10];

        staticSamplers[0] = CD3DX12_STATIC_SAMPLER_DESC(
            0, D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

        staticSamplers[1] = CD3DX12_STATIC_SAMPLER_DESC(
            1, D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        staticSamplers[2] = CD3DX12_STATIC_SAMPLER_DESC(
            2, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        staticSamplers[3] = CD3DX12_STATIC_SAMPLER_DESC(
            3, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        staticSamplers[4] = CD3DX12_STATIC_SAMPLER_DESC(
            4, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

        staticSamplers[5] = CD3DX12_STATIC_SAMPLER_DESC(
            5, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        staticSamplers[6] = CD3DX12_STATIC_SAMPLER_DESC(
            6, D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

        staticSamplers[7] = CD3DX12_STATIC_SAMPLER_DESC(
            7, D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        staticSamplers[8] = CD3DX12_STATIC_SAMPLER_DESC(
            8, D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
        staticSamplers[8].MaxAnisotropy = 16;

        staticSamplers[9] = CD3DX12_STATIC_SAMPLER_DESC(
            9, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER);
        staticSamplers[9].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;

        // --- Root signature desc
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc{};
        rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSigDesc.Desc_1_1.NumParameters = 1;
        rootSigDesc.Desc_1_1.pParameters = &rootParam;
        rootSigDesc.Desc_1_1.NumStaticSamplers = _countof(staticSamplers);
        rootSigDesc.Desc_1_1.pStaticSamplers = staticSamplers;
        rootSigDesc.Desc_1_1.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

        // --- Serializar y crear
        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSigDesc, &signatureBlob, &errorBlob));

        ComPtr<ID3D12RootSignature> rootSignature;
        ThrowIfFailed(device->CreateRootSignature(
            0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)));

        rootSignature->SetName(L"Bindless Root Signature");

        return rootSignature;
#endif
        return nullptr;
	}
};

st::gfx::CommonResources::CommonResources(st::gfx::ShaderFactory* shaderFactory) :
	m_ShaderFactory(shaderFactory)
{
	//m_FullscreenVS = m_ShaderFactory->CreateShader("Shaders/fullscreen_vs.vso", nvrhi::ShaderType::Vertex);
}

st::gfx::CommonResources::~CommonResources()
{}

