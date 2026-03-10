#ifndef __SHADING_HLSLI__
#define __SHADING_HLSLI__

#include "Material.hlsli"
#include "BRDF.hlsli"

void ShadeSurface_DirLight(interop::DirLightData dirLight, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{       
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, dirLight.viewSpaceDirection) * surfaceMaterial.diffuseAlbedo * dirLight.irradiance *
        dirLight.color;
    
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, dirLight.viewSpaceDirection, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, dirLight.halfAngularSize) * dirLight.irradiance * dirLight.color;
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

void ShadeSurface_SpotLight(interop::SpotLightData spotLight, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{
    float dist = length(surfacePos - spotLight.viewSpacePosition);
    float rDist = 1.0 / dist;
    out_diffuseRadiance = 0;
    out_specularRadiance = 0;
    
    float attenuation = square(saturate(1.0 - square(square(dist / spotLight.range))));
    if (attenuation == 0.0)
        return;
    
    float3 lightIncident = (surfacePos - spotLight.viewSpacePosition) * rDist;
    // Cone
    float LdotD = dot(lightIncident, spotLight.viewSpaceDirection);
    float directionAngle = acos(LdotD);
    float spotlight = 1 - smoothstep(spotLight.innerAngle, spotLight.outerAngle, directionAngle);
    if (spotlight == 0)
        return;
    
    float halfAngularSize = 0;
    float irradiance = 0;

    if (spotLight.radius > 0)
    {
        halfAngularSize = atan(spotLight.radius / dist);
        // A good enough approximation for 2 * (1 - cos(halfAngularSize)), numerically more accurate for small angular sizes
        float solidAngleOverPi = square(halfAngularSize);
        float radianceTimesPi = spotLight.intensity / square(spotLight.radius);
        irradiance = radianceTimesPi * solidAngleOverPi;
    }
    else
    {
        irradiance = spotLight.intensity * square(rDist);
    }
    irradiance *= spotlight * attenuation;
            
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, lightIncident) * surfaceMaterial.diffuseAlbedo * irradiance * spotLight.color;
    
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, lightIncident, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, halfAngularSize) * irradiance * spotLight.color;
}

float GetSpecularAO(float3 surfaceNormal, float roughness, float3 viewIncident, float ambientOcclusion)
{
    float NdotV = saturate(dot(surfaceNormal, -viewIncident));
    float specAO = saturate(pow(NdotV + ambientOcclusion, exp2(-16.0 * roughness - 1.0)) - 1.0 + ambientOcclusion);
    return specAO;
}

#endif //__SHADING_HLSLI__