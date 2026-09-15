#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

static const uint INVALID_INDEX = 0xFFFFFFFFu;

ConstantBuffer<interop::CullingConstants> StageConstants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(256, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
    if (DTid >= StageConstants.InstanceCount)
        return;
    
    ConstantBuffer<interop::SceneConstants> sceneConstants = ResourceDescriptorHeap[StageConstants.SceneDI];
    StructuredBuffer<interop::InstanceCullData> cullDataBuffer = ResourceDescriptorHeap[StageConstants.CullDataDI];
    StructuredBuffer<interop::BatchTableEntry> batchTable = ResourceDescriptorHeap[StageConstants.BatchTableDI];
    RWStructuredBuffer<interop::IndirectDrawCommand> cmds = ResourceDescriptorHeap[StageConstants.ArgsDI];
    RWStructuredBuffer<interop::VisibleInstancePayload> payload = ResourceDescriptorHeap[StageConstants.PayloadDI];
    
    const interop::InstanceCullData cullData = cullDataBuffer[DTid];
    if (cullData.BatchId == INVALID_INDEX)
        return;
    
    // Frustum vs sphere
    [unroll]
    for (int p = 0; p < 6; ++p)
    {
        const float4 plane = sceneConstants.frustumPlanes[p];
        if (dot(plane.xyz, cullData.BoundsSphere.xyz) + plane.w < -cullData.BoundsSphere.w)
            return;
    }
    
    const interop::BatchTableEntry bte = batchTable[cullData.BatchId];
    
    uint slot;
    InterlockedAdd(cmds[cullData.BatchId].InstanceCount, 1, slot); // Reserva room AND marks to render
    
    payload[bte.RegionOffset + slot].InstanceIndex = DTid;
    payload[bte.RegionOffset + slot].MeshIndex = bte.MeshIndex;
    payload[bte.RegionOffset + slot].MaterialIndex = bte.MaterialIndex;
    payload[bte.RegionOffset + slot].ExtraDataBaseIdx = bte.ExtraDataBaseIdx;
}