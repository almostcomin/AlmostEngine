// clang-format off

#include "BindlessRS.hlsli"
#include "Interop/ConstantBuffers.h"
#include "Interop/RenderResources.h"

struct VSInput
{
    float4 position : SV_Position;
    float2 textureCoord : TEXTURE_COORD;
};

ConstantBuffer<interop::TriangleRenderResources> renderResources : register(b0);

[RootSignature(BindlessRootSignature)] 
float4 main(VSInput input) : SV_Target
{
    Texture2D<float4> testTexture = ResourceDescriptorHeap[renderResources.textureIndex];
    return testTexture.Sample(pointClampSampler, input.textureCoord);
}