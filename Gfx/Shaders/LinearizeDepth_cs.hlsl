#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

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
    float linearDepth;
    if (depth < 1e-7f)
    {
        linearDepth = INFINITE_DEPTH;
    }
    else
    {
        linearDepth = Constants.nearPlaneDist / depth;
    }

    dstDepthTex[DTid] = linearDepth;
}