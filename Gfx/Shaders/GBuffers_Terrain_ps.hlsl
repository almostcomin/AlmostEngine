#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "TerrainMaterial.hlsli"
#include "HeightmapCommon.hlsli"

ConstantBuffer<interop::GBufferStageConstats> StageConstants : register(b0);
ConstantBuffer<interop::MultiInstanceDrawConstants> DrawConstants : register(b1);

struct PS_INPUT
{
    float4 pos                      : SV_POSITION;
    float3 normal                   : NORMAL;           // view space
    float4 tangent                  : TANGENT;          // xyz = tangent, w = handedness (-1 or +1)    
    float2 uv                       : TEXCOORD0;
    float3 posWorld                 : TEXCOORD1;
    nointerpolation uint patchIndex : PATCH_INDEX;
    float3 debugConnectionColor     : COLOR0;
};

struct PS_OUTPUT
{
    float4 GBuffer0 : SV_Target0;
    float4 GBuffer1 : SV_Target1;
    float4 GBuffer2 : SV_Target2;
    float4 GBuffer3 : SV_Target3;
};

[RootSignature(BindlessRootSignature)]
PS_OUTPUT main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    StructuredBuffer<interop::TerrainMaterialData> materialsBuffer = ResourceDescriptorHeap[sceneData.terrainMaterialsBufferDI];    
    StructuredBuffer<interop::HeightmapPatchData> patchDataBuffer = ResourceDescriptorHeap[sceneData.patchDataBufferDI];
    
    interop::TerrainMaterialData matData = materialsBuffer[DrawConstants.materialIndex];    
    interop::HeightmapPatchData patchData = patchDataBuffer[input.patchIndex];
    
    Texture2D<float> heightsTexture = ResourceDescriptorHeap[patchData.HeightmapTextureDI];
    
    float4 localPos = mul(patchData.InverseHeightmapMatrix, float4(input.posWorld, 1.0));
    float2 uv = float2(localPos.x, localPos.z);
    
    // Height sampled from heightmap
    float H = heightsTexture.SampleLevel(linearClampSampler, uv, 0).r;
    
    // Sampled normal
    float3 sampledNormal;
    {        
        // texel size
        float2 ts = float2(1.0 / patchData.TextureResolution.x, 1.0 / patchData.TextureResolution.y);
        // sample heights
        float hL = heightsTexture.SampleLevel(linearClampSampler, uv + float2(-ts.x,     0), patchData.MipLevel).r;
        float hR = heightsTexture.SampleLevel(linearClampSampler, uv + float2( ts.x,     0), patchData.MipLevel).r;
        float hD = heightsTexture.SampleLevel(linearClampSampler, uv + float2(    0, -ts.y), patchData.MipLevel).r;
        float hU = heightsTexture.SampleLevel(linearClampSampler, uv + float2(    0,  ts.y), patchData.MipLevel).r;
        
        float dHdU = (hR - hL) / (2.0 * patchData.CellSize);
        float dHdV = (hU - hD) / (2.0 * patchData.CellSize);
        
        float3 normalLocal = normalize(float3(-dHdU, 1.0, -dHdV));

        // transform to world
        float3x3 normalMatrix = float3x3(
            patchData.NormalMatrixCol0.xyz,
            patchData.NormalMatrixCol1.xyz,
            patchData.NormalMatrixCol2.xyz);
                
        sampledNormal = normalize(mul(normalMatrix, normalLocal));
    }
    
    PS_OUTPUT output;
    
    if (StageConstants.DebugChannel == DebugChannel_Heightmap_Heights)
    {
        output.GBuffer0 = float4(H, H, H, 1);
        output.GBuffer1 = float4(0, 0, 0, 1);
        output.GBuffer2 = float4(EncodeNormal(float3(0, 1, 0)), 1.0, 0.0);
        output.GBuffer3 = float4(0, 0, 0, 0);
    }
    else if (StageConstants.DebugChannel == DebugChannel_Heightmap_Slope)
    {
        const float slopeS = 1.0 - sampledNormal.y; // Sin of slope angle        
        output.GBuffer0 = float4(slopeS, slopeS, slopeS, 1);
        output.GBuffer1 = float4(0, 0, 0, 1);
        output.GBuffer2 = float4(EncodeNormal(float3(0, 1, 0)), 1.0, 0.0);
        output.GBuffer3 = float4(0, 0, 0, 0);
    }
    else if (StageConstants.DebugChannel == DebugChannel_Heightmap_Normals)
    {
        output.GBuffer0 = float4(sampledNormal * 0.5 + 0.5, 1);
        output.GBuffer1 = float4(0, 0, 0, 1);
        output.GBuffer2 = float4(EncodeNormal(float3(0, 1, 0)), 1.0, 0.0);
        output.GBuffer3 = float4(0, 0, 0, 0);
    }
    else if (StageConstants.DebugChannel == DebugChannel_Heightmap_Connections)
    {
        output.GBuffer0 = float4(input.debugConnectionColor, 1);
        output.GBuffer1 = float4(0, 0, 0, 1);
        output.GBuffer2 = float4(EncodeNormal(float3(0, 1, 0)), 1.0, 0.0);
        output.GBuffer3 = float4(0, 0, 0, 0);
    }
    else if (StageConstants.DebugChannel == DebugChannel_Heightmap_Resolution)
    {
        float2 uv2 = uv * patchData.TextureResolution * patchData.CellSize;
        output.GBuffer0 = CheckerEffect(uv2, patchData.CellSize, float4(0, 0, 0, 1), float4(1, 1, 1, 1));
        output.GBuffer1 = float4(0, 0, 0, 1);
        output.GBuffer2 = float4(EncodeNormal(float3(0, 1, 0)), 1.0, 0.0);
        output.GBuffer3 = float4(0, 0, 0, 0);
    }
    else
    {
        MaterialSample matSample = EvaluateTerrainMaterial(
            H,
            sampledNormal,
            input.normal,
            input.tangent,
            input.uv,
            matData);
        
        // GBuffer0: diffuseAlbedo.rgb + opacity.w
        output.GBuffer0 = float4(matSample.diffuseAlbedo, matSample.opacity);

        // GBuffer1: specularF0.rgb + occlusion.w
        output.GBuffer1 = float4(matSample.specularF0, matSample.occlusion);
    
        // GBuffer2: Normal.xy + roughnes.z + metalness.w
        // metalness is actually not needed since we have speculatF0, but keeping here so we can show metalness in MaterialChannels
        output.GBuffer2 = float4(EncodeNormal(matSample.normal), matSample.roughness, matSample.metalness);
    
        // GBuffer3: Emissive.rgb
        output.GBuffer3 = float4(matSample.emissiveColor, 0.0);
    }
        
    return output;
}
