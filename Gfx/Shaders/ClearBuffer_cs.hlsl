#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::ClearBufferConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Constants.bufferElementCount)
        return;
    
    RWByteAddressBuffer buffer = ResourceDescriptorHeap[Constants.bufferDI];
    buffer.Store(DTid.x * 4, Constants.clearValue);
}