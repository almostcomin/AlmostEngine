#ifndef __BRDF_H__
#define __BRDF_H__

#include "Common.hlsli"

float Lambert(float3 normal, float3 lightIncident)
{
    return max(0, -dot(normal, lightIncident)) * M_INV_PI;
}

// Normal distribution function, D factor
// Trowbridge-Reitz GGX, widened to approximate a spherical light source of the given
// angular size. Inspired by "Real Shading in Unreal Engine 4" (B. Karis), as in Donut.
float DistributionGGX(float3 N, float3 H, float a, float tanHalfAngularSize)
{
    float a2 = a * a;
    
    // Correct alpha
    float correctedA = a + 0.5 * tanHalfAngularSize;
    float sphereNorm = square(a / correctedA);
    
    float NdotH = saturate(dot(N, H));
    float NdotH2 = square(NdotH);

    float denom = (1.0 - NdotH2) + a2 * NdotH2;
    return M_INV_PI * a2 / square(denom) * sphereNorm;
}

// Geometry term: Schlick-GGX (single direction)
float GeometrySchlickGGX(float NdotV, float roughness)
{    
    float k = square(roughness + 1) / 8;
    //float k = a * a / 2; // TODO IBL
        
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / denom;
};

// Geometry term: Smith's method
float GeometrySmith(float NdotV, float NdotL, float roughness)
{            
    float k = square(roughness + 1) / 8;    
    return 1 / ((NdotL * (1 - k) + k) * (NdotV * (1 - k) + k));
}

// Schlick approximation
float3 FresnelSchlick(float3 V, float3 H, float3 F0)
{
    float VdotH = saturate(dot(V, H));   
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

// BRDF: Cook-Torrance reflectance
float3 BRDF_Specular_NDotL(float3 normal, float3 viewIncident, float3 lightIncident, float3 F0, float roughness, float tanHalfAngularSize)
{
    float3 N = normal;
    float3 L = -lightIncident;    
    float3 V = -viewIncident;
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    
    if (NdotV <= 0 || NdotL <= 0)
        return float3(0, 0, 0);
    
    float3 H = normalize(L + V);
    float alpha = max(0.01, roughness * roughness); // Clamp alpha: lower values make the specular lobe narrower than a pixel,
                                                    // producing aliasing and fireflies    
    float3 F = FresnelSchlick(V, H, F0);
    float D = DistributionGGX(N, H, alpha, tanHalfAngularSize);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    
    return F * (D * G * NdotL / 4);
}

float3 BRDF_Diffuse(float3 normal, float3 lightIncident)
{
    return Lambert(normal, lightIncident);
}

#endif