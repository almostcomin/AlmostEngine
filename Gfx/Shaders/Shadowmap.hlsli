#ifndef __SHADOWMAP_HLSLI__
#define __SHADOWMAP_HLSLI__

static const float2 PoissonDisk4[4] =
{
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760)
};

static const float2 PoissonDisk16[16] =
{
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

float SampleShadowMapPoissonDisk4(float4 viewPos, float4x4 viewToClipMatrix, Texture2D shadowMap, float2 texelSize, float filterRadius)
{
    float4 clipPos = mul(viewToClipMatrix, viewPos);
    float3 ndcPos = clipPos.xyz / clipPos.w;
    float2 uv = ndcPos.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    // PCF
    float shadow = 0.0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float2 offset = PoissonDisk4[i];
        float2 sampleUV = uv + offset * texelSize * filterRadius;
        float depth = shadowMap.Sample(pointClampSampler, sampleUV).r;
        shadow += ndcPos.z < depth ? 0.0 : 1.f; // Reverse depth
    }
    
    return shadow / 4.0;
}

float SampleShadowMapPoissonDisk16(float4 viewPos, float4x4 viewToClipMatrix, Texture2D shadowMap, float2 texelSize, float filterRadius)
{
    float4 clipPos = mul(viewToClipMatrix, viewPos);
    float3 ndcPos = clipPos.xyz / clipPos.w;
    float2 uv = ndcPos.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    // Rotate Poisson disk per pixel to break up pattern
    float angle = frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453) * 2.0 * 3.14159;
    float2 rotation = float2(cos(angle), sin(angle));
    
    // PCF
    float shadow = 0.0;
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 offset = PoissonDisk16[i];
        // Rotate offset
        offset = float2(
            offset.x * rotation.y - offset.y * rotation.x,
            offset.x * rotation.x + offset.y * rotation.y);
                
        float2 sampleUV = uv + offset * texelSize * filterRadius;
        float depth = shadowMap.Sample(pointClampSampler, sampleUV).r;
        shadow += ndcPos.z < depth ? 0.0 : 1.f; // Reverse depth
    }
    
    return shadow / 4.0;
}

float SampleShadowMap(float4 viewPos, float4x4 viewToClipMatrix, Texture2D shadowMap)
{
    float4 clipPos = mul(viewToClipMatrix, viewPos);
    float3 ndcPos = clipPos.xyz / clipPos.w;
    // UV
    float2 shadowUV = ndcPos.xy * 0.5 + 0.5;
    shadowUV.y = 1.0 - shadowUV.y;
        
    // Sample shadowmap
    float shadowDepth = shadowMap.Sample(pointClampSampler, shadowUV).r;
    
    float shadow = ndcPos.z < shadowDepth ? 0.0 : 1.f; // Reverse-Z compare    
    return shadow;
}

#endif