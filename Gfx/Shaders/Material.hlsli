#ifndef __MATERIAL_HLSLI__
#define __MATERIAL_HLSLI__

#include "Common.hlsli"

static const float c_DielectricSpecular = 0.04;

static const int MaterialTextureSample_ValidBaseColor   = 0x00000001;
static const int MaterialTextureSample_ValidMetalRough  = 0x00000002;
static const int MaterialTextureSample_ValidNormal      = 0x00000004;
static const int MaterialTextureSample_ValidEmissive    = 0x00000008;
static const int MaterialTextureSample_ValidOcclusion   = 0x00000010;

struct MaterialSample
{
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
    uint MaterialTextureSample_Flags;
};

MaterialSample DefaultMaterialSample()
{
    MaterialSample result;

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
    MaterialTextureSample ret;
    
    ret.baseColor = 0;
    ret.metalRough = 0;
    ret.normal = 0;
    ret.emissive = 0;
    ret.MaterialTextureSample_Flags = 0;
    
    return ret;
}

MaterialTextureSample SampleMaterialTextures(float2 uv, interop::MaterialData mat)
{
    MaterialTextureSample ret = DefaultMaterialTextureSample();
    
    if (mat.baseColorTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D baseColorTexture = ResourceDescriptorHeap[mat.baseColorTextureDI];
        ret.baseColor = baseColorTexture.Sample(linearWrapSampler, uv);
        ret.MaterialTextureSample_Flags |= MaterialTextureSample_ValidBaseColor;
    }
    if (mat.metalRoughTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D metalRoughTexture = ResourceDescriptorHeap[mat.metalRoughTextureDI];
        ret.metalRough = metalRoughTexture.Sample(linearWrapSampler, uv);
        ret.MaterialTextureSample_Flags |= MaterialTextureSample_ValidMetalRough;
        // Special case: usually we are going to have OCR texture (r=Occlusion, g=Roughness, b=Metalness)
        // We can check here if that is the case and avoid an extra sample
        if (mat.metalRoughTextureDI == mat.occlusionTextureDI)
        {
            ret.occlusion = ret.metalRough;
            ret.MaterialTextureSample_Flags |= MaterialTextureSample_ValidOcclusion;
        }
    }
    if (mat.occlusionTextureDI != INVALID_DESCRIPTOR_INDEX && (ret.MaterialTextureSample_Flags |= MaterialTextureSample_ValidOcclusion) == 0)
    {
        Texture2D occlusionTexture = ResourceDescriptorHeap[mat.occlusionTextureDI];
        ret.occlusion = occlusionTexture.Sample(linearWrapSampler, uv);
        ret.MaterialTextureSample_Flags |= MaterialTextureSample_ValidOcclusion;
    }
    if (mat.normalTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D normalTexture = ResourceDescriptorHeap[mat.normalTextureDI];
        ret.normal = normalTexture.Sample(linearWrapSampler, uv);
        ret.MaterialTextureSample_Flags |= MaterialTextureSample_ValidNormal;
    }
    if (mat.emissiveTextureDI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D emissiveTexture = ResourceDescriptorHeap[mat.emissiveTextureDI];
        ret.emissive = emissiveTexture.Sample(linearWrapSampler, uv);
        ret.MaterialTextureSample_Flags |= MaterialTextureSample_ValidEmissive;
    }
    
    return ret;
}

MaterialSample EvaluateSceneMaterial(float3 normal, float4 tangent, interop::MaterialData mat, MaterialTextureSample textures)
{
    MaterialSample result = DefaultMaterialSample();
    
    // Base Color
    float3 baseColor;
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidBaseColor) != 0)
    {
        baseColor = textures.baseColor.rgb;
        result.opacity = textures.baseColor.a;
    }
    else
    {
        baseColor = mat.baseColor.rgb;
        result.opacity = mat.baseColor.a;
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
    result.occlusion = mat.occlusion;
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidOcclusion) != 0)
    {
        result.occlusion *= textures.occlusion.r;
    }
                
    // Compute the BRDF inputs for the metal-rough model
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#metal-brdf-and-dielectric-brdf
    result.diffuseAlbedo = lerp(baseColor * (1.0 - c_DielectricSpecular), 0.0, result.metalness);
    result.specularF0 = lerp(c_DielectricSpecular, baseColor.rgb, result.metalness);
    
    // Normal
    if ((textures.MaterialTextureSample_Flags & MaterialTextureSample_ValidNormal) != 0)
    {
        result.normal = ApplyNormalMap(normal, tangent, textures.normal.xyz, mat.normalScale);
    }
    else
    {
        result.normal = normal;
    }
    
    return result;
}

#endif // __MATERIAL_HLSLI__