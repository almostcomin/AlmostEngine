#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::WBOITResolveStageConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    Texture2D<float4> accumTex = ResourceDescriptorHeap[Constants.accumDI];
    Texture2D<float> revealageTex = ResourceDescriptorHeap[Constants.revealageDI];
    
    float4 accum = accumTex.SampleLevel(pointClampSampler, input.uv, 0);
    float revealage = revealageTex.SampleLevel(pointClampSampler, input.uv, 0);

    float3 color = accum.rgb / max(accum.a, 1e-5) * (1.0 - revealage);
    return float4(color, 1.0 - revealage);
}