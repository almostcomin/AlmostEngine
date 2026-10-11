//--------------------------------------------------------------------------

//--------------------------------------------------------------------------

// ALM_REQUIRE_SM(6.8)

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
    RWStructuredBuffer<interop::IndirectDrawCommand> sparseCameraArgs = ResourceDescriptorHeap[StageConstants.SparseCameraArgsDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> compactedCameraArgs = ResourceDescriptorHeap[StageConstants.CompactedCameraArgsDI];    
    RWStructuredBuffer<uint> cameraCounts = ResourceDescriptorHeap[StageConstants.CameraCountsDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> sparseShadowArgs = ResourceDescriptorHeap[StageConstants.SparseShadowArgsDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> compactedShadowArgs = ResourceDescriptorHeap[StageConstants.CompactedShadowArgsDI];    
    RWStructuredBuffer<uint> shadowCounts = ResourceDescriptorHeap[StageConstants.ShadowCountsDI];    

    const interop::BatchTableEntry bte = batchTable[DTid];
    const uint bucketBase = bte.BucketFirstBatchIndex;
    
    if (sparseCameraArgs[DTid].InstanceCount > 0)
    {
        uint slot;
        InterlockedAdd(cameraCounts[bucketBase], 1, slot);
        compactedCameraArgs[bucketBase + slot] = sparseCameraArgs[DTid];
    }
    
    if (sparseShadowArgs[DTid].InstanceCount > 0)
    {
        uint slot;
        InterlockedAdd(shadowCounts[bucketBase], 1, slot);
        compactedShadowArgs[bucketBase + slot] = sparseShadowArgs[DTid];
    }
}