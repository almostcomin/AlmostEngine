#ifndef __MATERIAL_HLSLI__
#define __MATERIAL_HLSLI__

#include "Common.hlsli"

static const float c_DielectricSpecular = 0.04;

static const int MaterialTextureSample_ValidBaseColor   = 0x00000001;
static const int MaterialTextureSample_ValidMetalRough  = 0x00000002;
static const int MaterialTextureSample_ValidNormal      = 0x00000004;
static const int MaterialTextureSample_ValidEmissive    = 0x00000008;
static const int MaterialTextureSample_ValidOcclusion   = 0x00000010;
static const int MaterialTextureSample_ValidTransmission = 0x00000020;

struct MaterialSample
{
    float3 baseColor;
    float transmission;
    float3 diffuseAlbedo; // BRDF input Cdiff    
    float opacity;
    float3 specularF0; // BRDF input F0    
    float occlusion;
    float3 normal;
    float roughness;
    float3 emissiveColor;
    float metalness;
};

struct MaterialTextureSample
{
    float4 baseColor;
    float4 metalRough;
    float4 occlusion;
    float4 normal;
    float4 emissive;
    float transmission;
    uint MaterialTextureSample_Flags;
};

MaterialSample DefaultMaterialSample()
{
    MaterialSample result;

    result.baseColor = 0;
    result.transmission = 0;
    result.diffuseAlbedo = 0;
    result.opacity = 0;
    result.specularF0 = 0;
    result.occlusion = 0;
    result.normal = 0;
    result.roughness = 0;
    result.emissiveColor = 0;
    result.metalness = 0;

    return result;
}

MaterialTextureSample DefaultMaterialTextureSample()
{
    MaterialTextureSample result;
    
    result.baseColor = 0;
    result.metalRough = 0;
    result.occlusion = 0;
    result.normal = 0;
    result.emissive = 0;
    result.transmission = 0;
    result.MaterialTextureSample_Flags = 0;
    
    return result;
}

MaterialTextureSample SampleMaterialTextures(float2 uv, interop::MaterialData mat)
{
    MaterialTextureSample result = DefaultMaterialTextureSample();
    
    if (mat.baseColorTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D baseColorTexture = ResourceDescriptorHeap[mat.baseColorTextureDI];
        result.baseColor = baseColorTexture.Sample(linearWrapSampler, uv);
        result.MaterialTextureSample_Flags |= MaterialTextureSample_ValidBaseColor;
    }
    if (mat.metalRoughTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        // Typically OCR texture (r=Occlusion, g=Roughness, b=Metalness)
        Texture2D metalRoughTexture = ResourceDescriptorHeap[mat.metalRoughTextureDI];
        result.metalRough = metalRoughTexture.Sample(linearWrapSampler, uv);
        result.MaterialTextureSample_Flags |= MaterialTextureSample_ValidMetalRough;
    }
    if (mat.occlusionTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        // Typically OCR texture (r=Occlusion, g=Roughness, b=Metalness)
        Texture2D occlusionTexture = ResourceDescriptorHeap[mat.occlusionTextureDI];
        result.occlusion = occlusionTexture.Sample(linearWrapSampler, uv);
        result.MaterialTextureSample_Flags |= MaterialTextureSample_ValidOcclusion;
    }
    if (mat.normalTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D normalTexture = ResourceDescriptorHeap[mat.normalTextureDI];
        result.normal = normalTexture.Sample(linearWrapSampler, uv);
        result.MaterialTextureSample_Flags |= MaterialTextureSample_ValidNormal;
    }
    if (mat.emissiveTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D emissiveTexture = ResourceDescriptorHeap[mat.emissiveTextureDI];
        result.emissive = emissiveTexture.Sample(linearWrapSampler, uv);
        result.MaterialTextureSample_Flags |= MaterialTextureSample_ValidEmissive;
    }
    if (mat.transmissionTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D transmissionTexture = ResourceDescriptorHeap[mat.transmissionTextureDI];
        result.transmission = transmissionTexture.Sample(linearWrapSampler, uv).r;
        result.MaterialTextureSample_Flags |= MaterialTextureSample_ValidTransmission;
    }
    
    return result;
}

float SampleBaseColorAlpha(float2 uv, interop::MaterialData mat)
{
    if (mat.baseColorTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D baseColorTexture = ResourceDescriptorHeap[mat.baseColorTextureDI];
        return baseColorTexture.Sample(linearWrapSampler, uv).a;
    }
    return mat.baseColor.a;
}

MaterialSample EvaluateSceneMaterial(float3 normal, float4 tangent, interop::MaterialData mat, MaterialTextureSample textures, bool isFrontFace)
{
    MaterialSample result = DefaultMaterialSample();
    
    // Base Color & Opacity
    result.baseColor = mat.baseColor.rgb;
    result.opacity = mat.baseColor.a;
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidBaseColor) != 0)
    {
        result.baseColor *= textures.baseColor.rgb;
        result.opacity *= textures.baseColor.a;
    }
    
    // Emissive
    result.emissiveColor = mat.emissiveColor;
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidEmissive) != 0)
    {
        result.emissiveColor *= textures.emissive.rgb;
    }
    
    // Roughness & Metalness
    result.metalness = mat.metalness;
    result.roughness = mat.roughness;
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidMetalRough) != 0)
    {
        // Lets assume default ORM texture (occlusion-roughness-metalness)
        result.roughness *= textures.metalRough.g;
        result.metalness *= textures.metalRough.b;
    }
    
    // Occlusion
    result.occlusion = 1.0;
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidOcclusion) != 0)
    {
        result.occlusion *= textures.occlusion.r;
        result.occlusion *= mat.occlusion; // mat.occlusion is actually occlusion strength
    }

    // Transmission
    result.transmission = mat.transmission;
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidTransmission) != 0)
    {
        result.transmission *= textures.transmission;
    }
    
    // Compute the BRDF inputs for the metal-rough model
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#metal-brdf-and-dielectric-brdf
    result.diffuseAlbedo = lerp(result.baseColor * (1.0 - c_DielectricSpecular), 0.0, result.metalness);
    result.specularF0 = lerp(c_DielectricSpecular, result.baseColor.rgb, result.metalness);
    
    // Normal
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidNormal) != 0)
    {
        result.normal = ApplyNormalMap(normal, tangent, textures.normal.xyz, mat.normalScale);
    }
    else
    {
        result.normal = normalize(normal);
    }
    if (!isFrontFace)
    {
        result.normal = -result.normal;
    }
        
    return result;
}

#endif // __MATERIAL_HLSLI__