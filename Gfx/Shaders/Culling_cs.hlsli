//--------------------------------------------------------------------------
// GPU cull, dispatch 2/2:
// Frustum-culls every static instance, fused for both frustums in one dispatch (single InstanceCullData load):
//   camera -> frustumPlanes      -> camera args/payload
//   shadow -> shadowCasterPlanes -> shadow args/payload
//
// 1 thread = 1 static instance. For each visible instance:
// InterlockedAdd(cmds[batchIndex].InstanceCount) reserves the slot AND is the visibility mark;
// payload is written at [PayloadRegionOffset + slot].
//
// Consumed via ExecuteIndirect: one command per BatchTableEntry, zero InstanceCount = no-op.
// VS reads payload[startInstance + instanceID].
//
// Guards: BatchIndex == INVALID_INDEX -> unassigned/erased slot.
//         Flags without RF_VISIBLE    -> hidden instance, never scatters.
//
// RETRIEVE_STATS must be defined by the entry file (Culling_cs.hlsl / CullingStats_cs.hlsl):
// when 1, the kernel accumulates per-instance stats into the CullingStats buffer
// (the stage must run its reset + readback blocks; see GPUCullingRenderStage).
//--------------------------------------------------------------------------

// ALM_REQUIRE_SM(6.8)

#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "RenderFlags.hlsli"

static const uint INVALID_INDEX = 0xFFFFFFFFu;

ConstantBuffer<interop::CullingConstants> StageConstants : register(b0);

bool TestSphereFrustum(float4 p[6], float4 s)
{
    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        if (dot(p[i].xyz, s.xyz) + p[i].w < -s.w)
            return false;
    }
    return true;
}

void Scatter(
    RWStructuredBuffer<interop::IndirectDrawCommand> cmdsBuffer,
    RWStructuredBuffer<interop::VisibleInstancePayload> payloadBuffer,
    interop::BatchTableEntry bte,
    uint batchIndex, uint instanceIdx)
{
    uint slot;
    InterlockedAdd(cmdsBuffer[batchIndex].InstanceCount, 1, slot); // Reserve room AND marks to render
        
    payloadBuffer[bte.PayloadRegionOffset + slot].InstanceIndex = instanceIdx;
    payloadBuffer[bte.PayloadRegionOffset + slot].MeshIndex = bte.MeshIndex;
    payloadBuffer[bte.PayloadRegionOffset + slot].MaterialIndex = bte.MaterialIndex;
    payloadBuffer[bte.PayloadRegionOffset + slot].ExtraDataBaseIdx = bte.ExtraDataBaseIdx;
}

[RootSignature(BindlessRootSignature)]
[numthreads(256, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
    if (DTid >= StageConstants.TotalInstanceCount)
        return;
    
    ConstantBuffer<interop::SceneConstants> sceneConstants = ResourceDescriptorHeap[StageConstants.SceneDI];
    StructuredBuffer<interop::InstanceCullData> cullDataBuffer = ResourceDescriptorHeap[StageConstants.CullDataDI];
    StructuredBuffer<interop::BatchTableEntry> batchTable = ResourceDescriptorHeap[StageConstants.BatchTableDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> cameraArgs = ResourceDescriptorHeap[StageConstants.SparseCameraArgsDI];
    RWStructuredBuffer<interop::VisibleInstancePayload> cameraPayload = ResourceDescriptorHeap[StageConstants.CameraPayloadDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> shadowArgs = ResourceDescriptorHeap[StageConstants.SparseShadowArgsDI];
    RWStructuredBuffer<interop::VisibleInstancePayload> shadowPayload = ResourceDescriptorHeap[StageConstants.ShadowPayloadDI];
#if RETRIEVE_STATS
    RWStructuredBuffer<interop::CullingStats> stats = ResourceDescriptorHeap[StageConstants.StatsBufferDI];
#endif
    
    uint slot = (DTid < StageConstants.StaticInstanceCount) ?
        DTid : StageConstants.StaticInstanceCapacity + (DTid - StageConstants.StaticInstanceCount);
    
    const interop::InstanceCullData cullData = cullDataBuffer[slot];
    if (cullData.BatchIndex == INVALID_INDEX)
        return;
    // Hidden instance: never scatters (camera or shadow), even if its cull data is stale
    if (!(cullData.Flags & RF_VISIBLE))
        return;

    interop::BatchTableEntry bte = batchTable[cullData.BatchIndex];

#if RETRIEVE_STATS
    InterlockedAdd(stats[0].Buckets[bte.BucketIdx].CameraTotalInstances, 1);
    InterlockedAdd(stats[0].Buckets[bte.BucketIdx].CameraTotalPrimitives, bte.IndexCount / 3);
#endif
    if (TestSphereFrustum(sceneConstants.frustumPlanes, cullData.BoundsSphere))
    {
        Scatter(cameraArgs, cameraPayload, bte, cullData.BatchIndex, DTid);
#if RETRIEVE_STATS
        InterlockedAdd(stats[0].Buckets[bte.BucketIdx].CameraVisibleInstances, 1);
        InterlockedAdd(stats[0].Buckets[bte.BucketIdx].CameraVisiblePrimitives, bte.IndexCount / 3);
#endif
    }

    if (StageConstants.ShadowEnabled)
    {
#if RETRIEVE_STATS
        InterlockedAdd(stats[0].Buckets[bte.BucketIdx].ShadowTotalInstances, 1);
        InterlockedAdd(stats[0].Buckets[bte.BucketIdx].ShadowTotalPrimitives, bte.IndexCount / 3);
#endif
        if ((cullData.Flags & RF_CAST_SHADOWS) &&
            TestSphereFrustum(sceneConstants.shadowCasterPlanes, cullData.BoundsSphere))
        {
            Scatter(shadowArgs, shadowPayload, bte, cullData.BatchIndex, DTid);
#if RETRIEVE_STATS
            InterlockedAdd(stats[0].Buckets[bte.BucketIdx].ShadowVisibleInstances, 1);
            InterlockedAdd(stats[0].Buckets[bte.BucketIdx].ShadowVisiblePrimitives, bte.IndexCount / 3);
#endif
        }
    }
}