#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::ApplyExposureConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= Constants.TextureDim))
        return;
        
    Texture2D<float4> sceneColor = ResourceDescriptorHeap[Constants.InputSceneColorTextureDI];
    Texture2D<float> exposureFactor = ResourceDescriptorHeap[Constants.InputExposureTextureDI];
    RWTexture2D<float4> exposedColor = ResourceDescriptorHeap[Constants.OutputExposedColorTextureDI];

    float4 color = sceneColor[DTid];
    color.rgb *= exposureFactor[uint2(0, 0)];
    exposedColor[DTid] = color;
}