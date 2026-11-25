//=============================================================================
// Transmissive back-to-front sort + merged draw emit
//-----------------------------------------------------------------------------
// Sorts the transmissive instances staged during culling (see Culling_cs)
// far to near and emits the merged transmissive draw: one IndirectDrawCommand
// per instance (InstanceCount = 1) plus its reordered InstancePayload, so a
// single ExecuteIndirect renders all transmissive domains back to front, as
// their order-dependent blend (dst = T0 + F * dst) requires.
//
// Staging keys are ~asuint(distSq to camWorldPos): ascending key order renders
// far to near. Entries beyond the staged count are padded with UINT_MAX, sort
// last and emit nothing. The staged count buffer doubles as the count buffer
// of the merged ExecuteIndirect (maxCount clamps it to CAPACITY).
//
// Bitonic sorting network: compare-exchange stages whose direction is set by
// the current sequence size. Runs in groupshared memory:
// - Exactly one dispatch of one group: fixed cost, no readback, no indirect
//   dispatch. Always dispatched over the full capacity.
// - One thread handles two elements (one per half); each compare-exchange
//   pair is processed by exactly one thread (j > i), with a full group
//   barrier after every stride.
// - N must be a power of two. Complexity O(N * log2(N)^2).
//
// https://tamnd.github.io/brain/wiki/algorithms/01/11/318/
//=============================================================================

#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

// ALM_REQUIRE_SM(6.8)

#define N interop::SORTED_TRANSMISSIVE_CAPACITY   // (power of 2)

groupshared uint gsKeys[N];
groupshared uint gsSrc[N];

ConstantBuffer<interop::CullingConstants> StageConstants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(1024, 1, 1)]
void main(uint GTid : SV_GroupThreadID)
{
    RWStructuredBuffer<interop::IndirectDrawCommand> argsBuffer =
        ResourceDescriptorHeap[StageConstants.CompactedCameraArgsDI];
    RWStructuredBuffer<interop::InstancePayload> payloadBuffer =
        ResourceDescriptorHeap[StageConstants.CameraPayloadDI];
    RWStructuredBuffer<uint> countsBuffer =
        ResourceDescriptorHeap[StageConstants.CameraCountsDI];
    RWStructuredBuffer<interop::StagedTransmissiveInstance> stagedBuffer =
        ResourceDescriptorHeap[StageConstants.TransmissiveStagingDI];
    RWStructuredBuffer<uint> stagedCountBuffer =
        ResourceDescriptorHeap[StageConstants.TransmissiveStagedCountDI];
    
    const uint stagedCount = min(stagedCountBuffer[0], N);
    
    // Load: each thread handles two elements, one from each half.
    uint indices[2] = { GTid, GTid + 1024 };
    [unroll]
    for (uint e = 0; e < 2; ++e)
    {
        uint i = indices[e];

        // Invalid elements receive the maximum key, so they sort last.
        gsKeys[i] = (i < stagedCount)
            ? stagedBuffer[i].key
            : 0xFFFFFFFFu;

        gsSrc[i] = i;
    }

    GroupMemoryBarrierWithGroupSync();

    // Bitonic sort: key = ~asuint(distSq) -> far to near.
    for (uint size = 2; size <= N; size <<= 1)
    {
        for (uint stride = size >> 1; stride > 0; stride >>= 1)
        {
            [unroll]
            for (uint e = 0; e < 2; ++e)
            {
                uint i = indices[e];
                uint j = i ^ stride;

                // Process each compare-exchange pair only once.
                if (j > i)
                {
                    bool ascending = ((i & size) == 0);

                    uint keyI = gsKeys[i];
                    uint keyJ = gsKeys[j];

                    if ((ascending && keyI > keyJ) ||
                        (!ascending && keyI < keyJ))
                    {
                        gsKeys[i] = keyJ;
                        gsKeys[j] = keyI;

                        uint src = gsSrc[i];
                        gsSrc[i] = gsSrc[j];
                        gsSrc[j] = src;
                    }
                }
            }

            GroupMemoryBarrierWithGroupSync();
        }
    }
    
    // Emit
    [unroll]
    for (uint e = 0; e < 2; ++e)
    {
        const uint dst = indices[e];
        if (dst < stagedCount)
        {
            const interop::StagedTransmissiveInstance si = stagedBuffer[gsSrc[dst]];            
            uint slot = interop::BACK_TO_FRONT_BASE + dst;
            
            payloadBuffer[slot] = si.payload;
            
            argsBuffer[slot].VertexCountPerInstance = si.indexCount;
            argsBuffer[slot].InstanceCount = 1;
            argsBuffer[slot].StartVertexLocation = 0;
            argsBuffer[slot].StartInstanceLocation = slot;
        }
    }
    
    if (GTid == 0)
    {
        countsBuffer[interop::BACK_TO_FRONT_BASE] = stagedCount;
    }
}