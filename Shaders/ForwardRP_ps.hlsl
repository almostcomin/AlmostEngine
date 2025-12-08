#include "Interop/ForwardRP.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop:: ForwardRP> CB : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::MaterialCB> materialData = ResourceDescriptorHeap[CB.MaterialCBIndex];
        
    Texture2D albedo = ResourceDescriptorHeap[materialData.textureIndex];
    
    float4 out_col = albedo.Sample(linearWrapSampler, input.uv);
    return out_col;
}
