#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::LinearizeDepthConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Constants.width || DTid.y >= Constants.height)
        return;
    
    Texture2D<float> srcDepthTex = ResourceDescriptorHeap[Constants.srcDepthTexDI];
    RWTexture2D<float> dstDepthTex = ResourceDescriptorHeap[Constants.outLinearDepthTexDI];
    
    float depth = srcDepthTex[DTid];
    // Reverse-Z, infinite far
    float linearDepth = Constants.nearPlaneDist / max(depth, 0.0000001f);
    
    dstDepthTex[DTid] = linearDepth;
}