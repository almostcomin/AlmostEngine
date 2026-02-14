#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::ClearTextureConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= Constants.textureDim))
        return;
    
    RWTexture2D<float4> tex = ResourceDescriptorHeap[Constants.textureDI];
    tex[DTid] = Constants.clearValue;
}