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

void ShadeSurface_PointLight(interop::PointLightData pointLight, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{
    float dist = length(surfacePos - pointLight.viewSpacePosition);
    float rDist = 1.0 / dist;
    out_diffuseRadiance = 0;
    out_specularRadiance = 0;
    
    float attenuation = square(saturate(1.0 - square(square(dist / pointLight.range))));
    if(attenuation == 0.0)
        return;
    
    float halfAngularSize = 0;
    float irradiance = 0;

    if(pointLight.radius > 0)
    {
        halfAngularSize = atan(pointLight.radius / dist);
        // A good enough approximation for 2 * (1 - cos(halfAngularSize)), numerically more accurate for small angular sizes
        float solidAngleOverPi = square(halfAngularSize);        
        float radianceTimesPi = pointLight.intensity / square(pointLight.radius);        
        irradiance = radianceTimesPi * solidAngleOverPi;
    }
    else
    {
        irradiance = pointLight.intensity * square(rDist);
    }
    irradiance *= attenuation;
    
    float3 lightIncident = (surfacePos - pointLight.viewSpacePosition) * rDist;
        
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, lightIncident) * surfaceMaterial.diffuseAlbedo * irradiance * pointLight.color;
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, lightIncident, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, halfAngularSize) * irradiance * pointLight.color;
}

#endif //__SHADING_HLSLI__