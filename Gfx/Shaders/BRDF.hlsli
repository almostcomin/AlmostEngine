#ifndef __BRDF_H__
#define __BRDF_H__

#include "Common.hlsli"

float Lambert(float3 normal, float3 lightIncident)
{
    return max(0, -dot(normal, lightIncident)) * M_INV_PI;
}

// Normal distribution function, D factor
// Trowbridge-Reitz GGX
float DistributionGGX(float3 N, float3 H, float a, float halfAngularSize)
{
    float a2 = a * a;
    
    // Correct alpha
    float correctedA2 = saturate(a2 + 0.5 * tan(halfAngularSize));
    float sphereNorm = (a2 * a2) / (correctedA2 * correctedA2);
    
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;
        
    float nom = a2 * sphereNorm;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = M_PI * denom * denom;
    
    return nom / max(denom, 0.00001);
}

// Geometry term: Schlick-GGX (single direction)
float GeometrySchlickGGX(float NdotV, float a)
{    
    float k = square(a + 1) / 8;
    //float k = a * a / 2; // TODO IBL
        
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / max(denom, 0.00001);
};

// Geometry term: Smith's method
float GeometrySmith(float NdotV, float NdotL, float a)
{        
    float ggx1 = GeometrySchlickGGX(NdotV, a); // obstruction
    float ggx2 = GeometrySchlickGGX(NdotL, a); // shadowing
    
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float3 V, float3 H, float3 F0)
{
    float VdotH = saturate(dot(V, H));   
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

// BRDF: Cook-Torrance reflectance
float3 BRDF_Specular(float3 normal, float3 viewIncident, float3 lightIncident, float3 F0, float roughness, float halfAngularSize)
{
    float3 N = normal;
    float3 L = -lightIncident;
    float3 V = -viewIncident;
    float3 H = normalize(L + V);
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float a = roughness;// * roughness;
    
    float3 F = FresnelSchlick(V, H, F0);
    float D = DistributionGGX(N, H, a, halfAngularSize);
    float G = GeometrySmith(NdotV, NdotL, a);
    
    float3 num = D * F * G;
    float denom = 4.0 * NdotV * NdotL;
    
    return num / max(denom, 0.00001);
}

float3 BRDF_Diffuse(float3 normal, float3 lightIncident)
{
    return Lambert(normal, lightIncident);
}

#endif