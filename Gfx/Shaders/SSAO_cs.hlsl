#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::SSAOConstants> Constants : register(b0);

// Kernel samples (hemisphere)
// Set of samples with distance from center increasing linearly,
// and angle also increasing linearly with a step of 4.5678 radians.
// Plotted on x-y dimensions, it looks pretty much random, but is intended
// to make more samples closer to the center because they have greater weight.

static const float2 g_SamplePositions[] =
{
    float2(-0.016009523, -0.109951690), float2(-0.159746436,  0.047527402),
    float2( 0.093398190,  0.201641995), float2( 0.232600698,  0.151846663),
    float2(-0.220531935, -0.249953550), float2(-0.251498143,  0.296619710),
    float2( 0.376870668,  0.235583030), float2( 0.201175979,  0.457742532),
    float2(-0.535502966, -0.147913991), float2(-0.076133435,  0.606350138),
    float2( 0.666537538,  0.013120791), float2(-0.118107615, -0.712499494),
    float2(-0.740973793,  0.236423582), float2( 0.365057451,  0.749117816),
    float2( 0.734614792,  0.500464349), float2(-0.638657704, -0.695766948)
};

// Blue noise
static const float g_RandomValues[16] =
{
    0.059, 0.529, 0.176, 0.647,
    0.765, 0.294, 0.882, 0.412,
    0.235, 0.706, 0.118, 0.588,
    0.941, 0.471, 0.824, 0.353
};

float2 WindowToClip(float2 pixelPos) 
{
    return pixelPos * Constants.windowToClipScale + Constants.windowToClipBias;
}

// linearDepth is the linearized depth buffer, or the perpendicular distance (positive) from the camera XY plane to the point
// Result in local view space (right handed: +X right, +Y up, -Z forward)
float3 ClipToView(float2 clipPosXY, float linearDepth)
{
    return float3(clipPosXY * Constants.clipToView.xy * linearDepth, -linearDepth);
}

// V = unnormalized vector from center pixel to current sample
// N = normal at center pixel
float ComputeAO(float3 V, float3 N, float InvR2)
{
    // Squared distance    
    float VdotV = dot(V, V);
    // Cos(N, V). If > 0, sample (V) is over the horizon plane, if < 0, it is behind
    float NdotV = dot(N, V) * rsqrt(VdotV); 

    float lambertian = saturate(NdotV - Constants.surfaceBias);
    lambertian /= (1.0 - Constants.surfaceBias); // Remap to [0,1]
    float falloff = saturate(1 - VdotV * InvR2);
    return saturate(lambertian * falloff * Constants.power);
}

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    ConstantBuffer<interop::Scene> sceneData = ResourceDescriptorHeap[Constants.sceneDI];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[Constants.depthTextureDI]; // lienar depth
    Texture2D<float4> normalsTexture = ResourceDescriptorHeap[Constants.normalsTextureDI];
    RWTexture2D<float> outputAOTexture = ResourceDescriptorHeap[Constants.outputAOTextureDI];
        
    int2 outputSize = int2(Constants.textureWidth, Constants.textureHeight);    
    int2 pixelPos = DTid;
    if (any(pixelPos >= outputSize))
        return;
            
    // Get depth
    float linearDepth = depthTexture[pixelPos].r;
    if (linearDepth > 1000.0f)
    {
        outputAOTexture[pixelPos] = 1.0;
        return;
    }
    
    // Get normal in view space
    float3 normal = DecodeNormal(normalsTexture[pixelPos].xy);

    float2 clipPos = WindowToClip(float2(pixelPos) + 0.5);
    float3 viewPos = ClipToView(clipPos, linearDepth);
        
    float radiusWorld = Constants.radiusWorld * max(1.0, linearDepth * Constants.invBackgroundViewDepth);
    float radiusPixels = radiusWorld * Constants.radiusToScreen / linearDepth;
    
    float result = 0;
    if (radiusPixels > 1.0)
    {
        float invRadiusWorld2 = rcp(radiusWorld * radiusWorld);
        
        float angle = g_RandomValues[(pixelPos.x & 3) + ((pixelPos.y & 3) << 2)] * M_PI;
        float2 sincos = float2(sin(angle), cos(angle));
        
        uint numValidSamples = 0;
        [unroll]
        for (uint nSample = 0; nSample < 16; nSample++)
        {
            float2 sampleOffset = g_SamplePositions[nSample];
            sampleOffset = float2(
                sampleOffset.x * sincos.y - sampleOffset.y * sincos.x,
                sampleOffset.x * sincos.x + sampleOffset.y * sincos.y);
            
            int2 samplePixelPos = int2(floor(pixelPos + 0.5 + sampleOffset * radiusPixels));
            float2 sampleClipPos = WindowToClip(float2(samplePixelPos) + 0.5);
            if (any(samplePixelPos != pixelPos) && all(abs(sampleClipPos.xy) < 1.0))
            {
                float sampleLinearDepth = depthTexture[samplePixelPos].r;

                float3 sampleViewPos = ClipToView(sampleClipPos, sampleLinearDepth);
                float3 diff = sampleViewPos - viewPos;
                float AO = ComputeAO(diff, normal, invRadiusWorld2);
                result += AO;
                numValidSamples++;
            }
        }
        
        if (numValidSamples > 0)
        {
            result /= numValidSamples;
        }        
    }
    
    outputAOTexture[pixelPos] = 1.0 - result;
}