#ifndef __HEIGHTMAP_MATERIAL_HLSLI__
#define __HEIGHTMAP_MATERIAL_HLSLI__

#include "Common.hlsli"
#include "Material.hlsli"

struct TerrainLayerSample
{
    float3 baseColor;
    float roughness;
    float3 normal;
    float metalness;
};

TerrainLayerSample MergeTerrainLayers(TerrainLayerSample sampleA, TerrainLayerSample sampleB, float w)
{
    TerrainLayerSample result;

    result.baseColor = lerp(sampleA.baseColor, sampleB.baseColor, w);
    result.roughness = lerp(sampleA.roughness, sampleB.roughness, w);
    result.metalness = lerp(sampleA.metalness, sampleB.metalness, w);
    result.normal = slerp(sampleA.normal, sampleB.normal, w);
    
    return result;
}

TerrainLayerSample SampleTerrainLayerColor(float2 uv, float3 normal, float4 tangent, interop::TerrainMaterialLayer matLayer)
{
    TerrainLayerSample result;
    uv = uv * matLayer.UVScale;
    
    result.baseColor = matLayer.BaseColorTint;
    if (matLayer.BaseColorTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D baseColorTexture = ResourceDescriptorHeap[matLayer.BaseColorTextureDI];
        result.baseColor *= baseColorTexture.Sample(linearWrapSampler, uv).xyz;
    }
    
    // Roughness & Metalness
    result.metalness = matLayer.Metalness;
    result.roughness = matLayer.Roughness;
    if (matLayer.MetalRoughTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        // Typically OCM texture (r=Occlusion, g=Roughness, b=Metalness)
        Texture2D metalRoughTexture = ResourceDescriptorHeap[matLayer.MetalRoughTextureDI];
        float3 metalRough = metalRoughTexture.Sample(linearWrapSampler, uv).xyz;
                
        result.roughness *= metalRough.g;
        result.metalness *= metalRough.b;
    }
        
    // Normal
    if (matLayer.NormalTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D normalTexture = ResourceDescriptorHeap[matLayer.NormalTextureDI];
        float3 normalTexV = normalTexture.Sample(linearWrapSampler, uv).xyz;
        result.normal = ApplyNormalMap(normal, tangent, normalTexV, float2(1.0, 1.0));
    }
    else
    {
        result.normal = normalize(normal);
    }
    
    return result;
}

MaterialSample EvaluateTerrainMaterial(float normHeight, float3 normalWorld, float3 normalView, float4 tangentView, float2 uv,
    interop::TerrainMaterialData mat)
{
    TerrainLayerSample layerSample;
    const float slopeS = 1.0 - normalWorld.y; // Sin of slope angle
    
    if (slopeS >= mat.SlopeAngleEndSin)
    {
        layerSample = SampleTerrainLayerColor(uv, normalView, tangentView, mat.SlopeLayer);
    }
    else
    {
        if (normHeight < mat.HeightTransitionStart)
        {
            layerSample = SampleTerrainLayerColor(uv, normalView, tangentView, mat.GroundLayer);
        }
        else if (normHeight > mat.HeightTransitionEnd)
        {
            layerSample = SampleTerrainLayerColor(uv, normalView, tangentView, mat.PeakLayer);
        }
        else
        {
            TerrainLayerSample groundSample = SampleTerrainLayerColor(uv, normalView, tangentView, mat.GroundLayer);
            TerrainLayerSample peakSample = SampleTerrainLayerColor(uv, normalView, tangentView, mat.PeakLayer);
            float w = smoothstep(mat.HeightTransitionStart, mat.HeightTransitionEnd, normHeight);
            layerSample = MergeTerrainLayers(groundSample, peakSample, w);
        }
        if (slopeS >= mat.SlopeAngleStartSin)
        {
            TerrainLayerSample slopeSample = SampleTerrainLayerColor(uv, normalView, tangentView, mat.SlopeLayer);
            float sw = smoothstep(mat.SlopeAngleStartSin, mat.SlopeAngleEndSin, slopeS);
            sw = pow(sw, mat.SlopeBlendSharpness);
            layerSample = MergeTerrainLayers(layerSample, slopeSample, sw);
        }
    }
        
    MaterialSample result = DefaultMaterialSample();
        
    // Compute the BRDF inputs for the metal-rough model
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#metal-brdf-and-dielectric-brdf
    result.diffuseAlbedo = lerp(layerSample.baseColor * (1.0 - c_DielectricSpecular), 0.0, result.metalness);
    result.specularF0 = lerp(c_DielectricSpecular, layerSample.baseColor, result.metalness);

    result.normal = layerSample.normal;
    result.roughness = layerSample.roughness;
    result.metalness = layerSample.metalness;
    result.opacity = 1.0;
    result.occlusion = 1.0;
    result.emissiveColor = float3(0.0, 0.0, 0.0);

    return result;
}

#endif // __HEIGHTMAP_MATERIAL_HLSLI__