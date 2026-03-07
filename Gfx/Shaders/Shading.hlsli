#ifndef __SHADING_HLSLI__
#define __SHADING_HLSLI__

#include "Material.hlsli"
#include "BRDF.hlsli"

static const int LightType_Directional = 0;
static const int LighType_Point = 1;
static const int LighType_Spot = 2;

struct LightConstants
{
    uint lighType;
    float3 direction;
    float angularSizeOrInvRange;
    float intensity;
};

void ShadeSurface(LightConstants light, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident, float3x3 worldToViewMatrix,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{
    float3 lightIncident = 0;
    float halfAngularSize = 0;
    float irradiance = 0;

    if(light.lighType == LightType_Directional)
    {
        lightIncident = normalize(mul(worldToViewMatrix, light.direction));
        halfAngularSize = light.angularSizeOrInvRange / 2;
        irradiance = light.intensity;
    }
    else //if(light.lighType == LighType_Point || light.lighType == LighType_Spot)
    {
        // TODO
        return;
    }
        
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, lightIncident) * surfaceMaterial.diffuseAlbedo * irradiance;
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, lightIncident, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, halfAngularSize) * irradiance;
}

void ShadeSurface_PointLight(interop::PointLightData pointLight, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident, float3x3 worldToViewMatrix,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{
    float dist = length(surfacePos - pointLight.position);
    float attenuation = 1.0 / (dist * dist);    
    float rangeAttenuation = square(saturate(1.0 - square(dist / pointLight.range)));
    attenuation *= rangeAttenuation;

    float3 lightIncident = (surfacePos - pointLight.position) / dist;
    float halfAngularSize = 0;
    float3 irradiance = pointLight.color * pointLight.intensity * attenuation;
    
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, lightIncident) * surfaceMaterial.diffuseAlbedo * irradiance;
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, lightIncident, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, halfAngularSize) * irradiance;
}

#endif //__SHADING_HLSLI__