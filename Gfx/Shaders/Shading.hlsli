#ifndef __SHADING_HLSLI__
#define __SHADING_HLSLI__

#include "Material.hlsli"
#include "BRDF.hlsli"

void ShaderSurface_IBL(MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{
    out_diffuseRadiance = float3(0.0, 0.0, 0.0);
    out_specularRadiance = float3(0.0, 0.0, 0.0);
}

void ShadeSurface_DirLight(interop::DirLightData dirLight, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{       
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, dirLight.viewSpaceDirection) * surfaceMaterial.diffuseAlbedo * dirLight.irradiance *
        dirLight.color;
    
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, dirLight.viewSpaceDirection, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, dirLight.tanHalfAngularSize);
    out_specularRadiance *= dirLight.irradiance * dirLight.color;
}

void ShadeSurface_PointLight(interop::PointLightData pointLight, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{
    float dist = length(surfacePos - pointLight.viewSpacePosition);
    dist = max(dist, SMALL_FLOAT);
    float rDist = 1.0 / dist;
    float tanHalfAngularSize = min(pointLight.radius * rDist, 1.0);
    out_diffuseRadiance = 0;
    out_specularRadiance = 0;
    
    float attenuation = square(saturate(1.0 - square(square(dist / pointLight.range))));
    if(attenuation == 0.0)
        return;

    // Sphere emitter: E = I/d^2 outside the source radius, saturates at I/r^2 inside.
    // radius == 0 degenerates to a punctual light.
    float invDist = 1.0 / max(dist, pointLight.radius);
    float irradiance = pointLight.intensity * square(invDist) * attenuation;
    
    float3 lightIncident = (surfacePos - pointLight.viewSpacePosition) * rDist;
        
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, lightIncident) * surfaceMaterial.diffuseAlbedo * irradiance * pointLight.color;
    
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, lightIncident, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, tanHalfAngularSize);
    out_specularRadiance *= irradiance * pointLight.color;
}

void ShadeSurface_SpotLight(interop::SpotLightData spotLight, MaterialSample surfaceMaterial, float3 surfacePos, float3 viewIncident,
    out float3 out_diffuseRadiance, out float3 out_specularRadiance)
{
    float dist = length(surfacePos - spotLight.viewSpacePosition);
    dist = max(dist, SMALL_FLOAT);
    float rDist = 1.0 / dist;
    float tanHalfAngularSize = min(spotLight.radius * rDist, 1.0);
    out_diffuseRadiance = 0;
    out_specularRadiance = 0;
    
    float attenuation = square(saturate(1.0 - square(square(dist / spotLight.range))));
    if (attenuation == 0.0)
        return;
    
    float3 lightIncident = (surfacePos - spotLight.viewSpacePosition) * rDist;
    
    // Cone
    float LdotD = dot(lightIncident, spotLight.viewSpaceDirection);    
    float cone = smoothstep(spotLight.cosOuterAngle, spotLight.cosInnerAngle, LdotD);
    if (cone == 0.0)
        return;
    
    // Sphere emitter: E = I/d^2 outside the source radius, saturates at I/r^2 inside.
    // radius == 0 degenerates to a punctual light.    
    float invDist = 1.0 / max(dist, spotLight.radius);
    float irradiance = spotLight.intensity * square(invDist) * attenuation * cone;
            
    out_diffuseRadiance = BRDF_Diffuse(surfaceMaterial.normal, lightIncident) * surfaceMaterial.diffuseAlbedo * irradiance * spotLight.color;
    
    out_specularRadiance = BRDF_Specular_NDotL(surfaceMaterial.normal, viewIncident, lightIncident, surfaceMaterial.specularF0,
        surfaceMaterial.roughness, tanHalfAngularSize);
    out_specularRadiance *=  irradiance * spotLight.color;
}

float GetSpecularAO(float3 surfaceNormal, float roughness, float3 viewIncident, float ambientOcclusion)
{
    float NdotV = saturate(dot(surfaceNormal, -viewIncident));
    float specAO = saturate(pow(NdotV + ambientOcclusion, exp2(-16.0 * roughness - 1.0)) - 1.0 + ambientOcclusion);
    return specAO;
}

#endif //__SHADING_HLSLI__