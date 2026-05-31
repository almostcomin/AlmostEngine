#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::GBufferStageConstats> StageConstants : register(b0);
ConstantBuffer<interop::MultiInstanceDrawConstants> DrawConstants : register(b1);

struct VS_OUTPUT
{
    float4 pos                      : SV_POSITION;
    float3 normal                   : NORMAL;           // view space
    float4 tangent                  : TANGENT;          // xyz = tangent, w = handedness (-1 or +1)    
    float3 normalWorld              : NORMAL1;
    float2 uv                       : TEXCOORD0;
    float3 posWorld                 : TEXCOORD1;
    nointerpolation uint patchIndex : PATCH_INDEX;
};

void GetLocalSpaceNormalTangent(Texture2D<float> heightsTexture, uint2 textureRes, float2 uv, uint mipLevel, out float3 normal, out float4 tangent)
{
    uint texWidth = textureRes.x << mipLevel;
    uint texHeight = textureRes.y << mipLevel;
    float2 texelSize = float2(1.0 / texWidth, 1.0 / texHeight);
    
    float hL = heightsTexture.SampleLevel(linearClampSampler, uv + float2(-texelSize.x, 0), mipLevel).r;
    float hR = heightsTexture.SampleLevel(linearClampSampler, uv + float2(texelSize.x, 0), mipLevel).r;
    float hD = heightsTexture.SampleLevel(linearClampSampler, uv + float2(0, -texelSize.y), mipLevel).r;
    float hU = heightsTexture.SampleLevel(linearClampSampler, uv + float2(0, texelSize.y), mipLevel).r;
    
    // Gradient in UV space
    float dHdU = (hR - hL) * 0.5 / texelSize.x;
    float dHdV = (hU - hD) * 0.5 / texelSize.y;

    normal = normalize(float3(-dHdU, 1.0, -dHdV));
    
    // Local tangent: X direction in UV coordinates
    tangent.xyz = normalize(float3(1.0, dHdU, 0.0));
    tangent.w = 1.0;
}

[RootSignature(BindlessRootSignature)]
VS_OUTPUT main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[StageConstants.sceneDI];
    StructuredBuffer<interop::InstanceData> instancesDataBuffer = ResourceDescriptorHeap[sceneData.instanceBufferDI];
    StructuredBuffer<interop::MeshData> meshesDataBuffer = ResourceDescriptorHeap[sceneData.meshesBufferDI];
    StructuredBuffer<interop::HeightmapPatchData> patchDataBuffer = ResourceDescriptorHeap[sceneData.patchDataBufferDI];
    
    uint instanceIndex = instanceID + DrawConstants.rawInstanceBaseIdx;
    interop::InstanceData instanceData = instancesDataBuffer[instanceIndex];
    interop::MeshData meshData = meshesDataBuffer[DrawConstants.meshIndex];
            
    ByteAddressBuffer indexBuffer = ResourceDescriptorHeap[meshData.indexBufferDI];
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[meshData.vertexBufferDI];
    
    // Patch data
    uint patchIndex = instanceID + DrawConstants.extraDataBaseIdx;
    interop::HeightmapPatchData patchData = patchDataBuffer[patchIndex];
    Texture2D<float> heightsTexture = ResourceDescriptorHeap[patchData.HeightmapTextureDI];
    
    // Fetch vertex data
    uint baseIndex = GetIndex(indexBuffer, meshData.indexOffsetBytes, meshData.indexSize, vertexID);
    uint vertexBufferOffset = meshData.vertexBufferOffsetBytes + (baseIndex * meshData.vertexStride);
    
    // Build local pos
    float2 pos2 = LoadVertexAttributeFloat2(vertexBuffer, vertexBufferOffset, meshData.vertexPositionOffset);
    
    float2 uv = patchData.MinUV + pos2 * patchData.CellSize;
    uv /= patchData.DataNormSize;
    
    float H = heightsTexture.SampleLevel(linearClampSampler, uv, patchData.MipLevel).r;
    float3 pos = float3(pos2.x, H, pos2.y);
    
    // Normal & Tangent
    float3 normal;
    float4 tangent;
    GetLocalSpaceNormalTangent(heightsTexture, patchData.TextureResolution, uv, patchData.MipLevel, normal, tangent);
            
    // Transform
    float4 posWorld = mul(instanceData.modelMatrix, float4(pos, 1.0f));
    float4 posClip = mul(sceneData.camViewProjMatrix, posWorld);
        
    // Normal
    const float3x3 normalMatrix = (float3x3)transpose(instanceData.inverseModelMatrix);
    float3 normalWorld = normalize(mul(normalMatrix, normal));
    float3 normalView = normalize(mul((float3x3)sceneData.camViewMatrix, normalWorld));
    
    // Tangent
    float3 tangentWorld = normalize(mul(instanceData.modelMatrix, float4(tangent.xyz, 0.0)).xyz);
    float3 tangentView = normalize(mul((float3x3)sceneData.camViewMatrix, tangentWorld));
            
    // Output
    VS_OUTPUT output;
    output.pos = posClip;
    output.normal = normalView;
    output.tangent = float4(tangentView, tangent.w);
    output.normalWorld = normalWorld;
    output.uv = uv;
    output.posWorld = posWorld.xyz;
    output.patchIndex = patchIndex;
    
    return output;
}
