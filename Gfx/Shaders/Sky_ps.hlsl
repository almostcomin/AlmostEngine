#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Noise.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float CLOUD_RADIUS = 1000.0; // Radius of the skydome    
static const float CLOUD_DENSITY_POW = 2.0f;
static const float SUN_SCATTER_POW = 4.0f;
static const float SUN_CLOUD_POW = 2.0f;
static const float CLOUD_FADE_START = 0.05f;
static const float CLOUD_FADE_RANGE = 0.15f;
static const float CLOUD_FADE_POW = 1.5f;

float SampleCloudDensity(float3 worldPos, Texture3D cloudsTexture)
{
    float3 uvw;
    uvw.xy = worldPos.xz * Constants.cloudScale;
    uvw.xy += Constants.windVelocity * Constants.time;
    uvw.z = (worldPos.y - Constants.cloudLayerMin) / (Constants.cloudLayerMax - Constants.cloudLayerMin);
          
    float perlinWorley = cloudsTexture.SampleLevel(linearWrapSampler, float3(uvw.x * 0.5, uvw.y * 0.5, uvw.z), 0.0).r;
    float3 worley = cloudsTexture.SampleLevel(linearWrapSampler, uvw, 0.0).gba;

    // cloud shape modeled after the GPU Pro 7 chapter
    float wfbm = worley.r * 0.625 + worley.g * 0.25 + worley.b * 0.125;
    float coverage = remap(perlinWorley, wfbm - 1.0, 1.0, 0.0, 1.0);
    coverage = remap(coverage, 1.0 - Constants.coverage, 1.0, 0.0, 1.0); // fake cloud coverage    
    coverage = saturate(coverage);
    //coverage = pow(coverage, 2.0f);

    // Gradiente de densidad vertical — nubes más densas en el centro de la capa
    //float heightGradient = saturate(remap(uvw.z, 0.0f, 0.1f, 0.0f, 1.0f))   // fade inferior
    //                     * saturate(remap(uvw.z, 0.6f, 1.0f, 1.0f, 0.0f));  // fade superior
    //coverage *= heightGradient;
    
    return coverage;
}
/*
float SampleLightRay(float3 pos, float3 sunDir)
{
    float density = 0.0f;
    float stepSize = (CLOUD_LAYER_MAX - CLOUD_LAYER_MIN) / Constants.lighSteps;

    for (int i = 0; i < Constants.lighSteps; i++)
    {
        float3 lightPos = pos - sunDir * stepSize * i;
        density += SampleCloudDensity(lightPos);
    }

    // Beer-Lambert hacia el sol
    return exp(-density * stepSize * ABSORPTION_COEFF);
}
*/
[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{    
    Texture3D baseTexture = ResourceDescriptorHeap[Constants.cloudBaseShapeTexture];
    
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0;
    clipPos.z = 1.0;
    clipPos.w = 1.0;
    
    float4 worldPos = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 worldDir = normalize(worldPos.xyz);

    float3 rayOrigin = float3(0.0, 0.0, 0.0);
    float3 rayDir = worldDir;
    
    float stepSize = 50.0f; // metros por paso, ajustable
    float t = 0.0f; // distancia recorrida
    
    float cloudColor = 0.0;
    float transmittance = 1.0;
    for(uint i = 0; i < Constants.maxSteps; ++i)
    {
        float3 pos = rayOrigin + rayDir * t;
        if (pos.y < Constants.cloudLayerMin)
        {
            t += stepSize;
            continue;
        }
        
        if (pos.y > Constants.cloudLayerMax)
            break;
        
        float density = SampleCloudDensity(pos, baseTexture);
        if (density > 0.0)
        {
            cloudColor = saturate(cloudColor + density);
/*            
            // Beer-Lambert attenuation
            float absorption = exp(-density * stepSize * Constants.absorptionCoeff);
            transmittance *= absorption;
            // TODO: Sample to sun
            //float lightEnergy = SampleLightRay(pos, sunDir);
            float lightEnergy = 0.5;
            cloudColor += transmittance * lightEnergy * density * stepSize;
*/            
        }
        
        if (transmittance < 0.01)
            break;
        t += stepSize;
    }
    
    // Horizon mask
    float horizonMask = saturate((worldDir.y - CLOUD_FADE_START) / CLOUD_FADE_RANGE);
    cloudColor *= pow(horizonMask, CLOUD_FADE_POW);
    
    // Sun direction
    float3 sunDir = normalize(Constants.sunDirection);
    float sunDot = max(0.0f, dot(worldDir, sunDir));
    float sunScatter = pow(sunDot, SUN_SCATTER_POW);

    // Sky gradient: zenith-to-horizon + directional sun scatter
    float3 skyZenith = float3(0.00f, 0.10f, 0.20f);
    float3 skyHorizon = float3(0.10f, 0.20f, 0.40f);
    float3 sunColor = float3(1.00f, 0.75f, 0.45f); // warm scatter near sun

    float3 skyGradient = lerp(skyHorizon, skyZenith, saturate(worldDir.y));
    skyGradient = lerp(skyGradient, sunColor, sunScatter * 0.35f);

    float3 color = lerp(skyGradient, float3(1.00f, 1.00f, 0.90f), cloudColor);
    
    
/*    
    float3 uvw = float3(input.uv, 0.0);
    uvw.x *= (1920.0 / 1080.0);
    uvw.xy += Constants.time * 0.02;
    uvw.z += Constants.time * 0.01;
 
    float cloud = GetCloudCoverage(uvw, baseTexture);
            
    color = float3(cloud, cloud, cloud);
*/        
        
    return float4(color, 1.0f);
}