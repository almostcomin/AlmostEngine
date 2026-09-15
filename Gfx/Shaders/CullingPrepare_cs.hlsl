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
    RWStructuredBuffer<interop::IndirectDrawCommand> cmds = ResourceDescriptorHeap[StageConstants.ArgsDI];
    
    const interop::BatchTableEntry bte = batchTable[DTid];
    interop::IndirectDrawCommand cmd;
    cmd.VertexCountPerInstance = bte.IndexCount;
    cmd.InstanceCount = 0;
    cmd.StartVertexLocation = 0;
    cmd.StartInstanceLocation = bte.RegionOffset;
    cmds[DTid] = cmd;
}