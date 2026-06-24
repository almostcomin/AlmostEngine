#ifndef __GBUFFERS_COMMON_H__
#define __GBUFFERS_COMMON_H__

#include "Material.hlsli"

struct GBufferData
{
    float4 GBuffer0 : SV_Target0; // diffuseAlbedo.rgb + opacity.a
    float4 GBuffer1 : SV_Target1; // specularF0.rgb + occlusion.a
    float4 GBuffer2 : SV_Target2; // normal.xy + roughness.z + metalness.w
    float4 GBuffer3 : SV_Target3; // emissive.rgb
};

MaterialSample DecodeGBuffers(float4 channels[4])
{
    MaterialSample surface = DefaultMaterialSample();
    
    surface.diffuseAlbedo = channels[0].xyz;
    surface.opacity = channels[0].w;
    surface.specularF0 = channels[1].xyz;
    surface.occlusion = channels[1].w;
    surface.normal = DecodeNormal(channels[2].xy);
    surface.roughness = channels[2].z;
    surface.metalness = channels[2].w;
    surface.emissiveColor = channels[3].xyz;
    
    return surface;
}

GBufferData EncodeGBuffers(in MaterialSample mat)
{
    GBufferData outData;
    
    outData.GBuffer0 = float4(mat.diffuseAlbedo, mat.opacity);
    outData.GBuffer1 = float4(mat.specularF0, mat.occlusion);
    outData.GBuffer2 = float4(EncodeNormal(mat.normal), mat.roughness, mat.metalness);
    outData.GBuffer3 = float4(mat.emissiveColor, 0.0);
    
    return outData;    
}    


#endif