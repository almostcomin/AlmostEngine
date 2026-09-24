//--------------------------------------------------------------------------
// GPU cull, dispatch 1/2:
// Reset & prefill indirect draw args from the static BatchTable (CPU-built, scene-change-only).
//
// 1 thread = 1 batch = 1 command. Writes camera + shadow args buffers (same layout, must match 
// D3D12_DRAW_ARGUMENTS, 16 bytes).
//
// InstanceCount always starts at 0 (Culling_cs increments it atomically every frame).
// StartInstanceLocation = PayloadRegionOffset: payload region base, recovered in the VS via SV_StartInstanceLocation.

//--------------------------------------------------------------------------

#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::CullingConstants> StageConstants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(256, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
    if (DTid >= StageConstants.BatchCount)
        return;
    
    StructuredBuffer<interop::BatchTableEntry> batchTable = ResourceDescriptorHeap[StageConstants.BatchTableDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> cameraArgs = ResourceDescriptorHeap[StageConstants.CameraArgsDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> shadowArgs = ResourceDescriptorHeap[StageConstants.ShadowArgsDI];
    
    const interop::BatchTableEntry bte = batchTable[DTid];
    
    interop::IndirectDrawCommand cmd;
    cmd.VertexCountPerInstance = bte.IndexCount;
    cmd.InstanceCount = 0;
    cmd.StartVertexLocation = 0;
    cmd.StartInstanceLocation = bte.PayloadRegionOffset;
    
    cameraArgs[DTid] = cmd;
    shadowArgs[DTid] = cmd;
}