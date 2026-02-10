#ifndef __SHADERS_INTEROP_RENDERRESOURCES_H__
#define __SHADERS_INTEROP_RENDERRESOURCES_H__

#include "Interop.h"

namespace interop
{
    struct ClearBufferConstants
    {
        uint bufferDI;
        uint bufferElementCount; // uints
        uint clearValue;
    };

    struct BlitGraphicsConstants
    {
        uint textureDI;
    };

    struct BlitComputeConstants
    {
        TextureSampledViewIndex srcTextureDI;
        TextureStorageViewIndex dstTextureDI;
        uint2 viewBegin;
        uint2 viewEnd;
    };

    struct GenMipsConstants
    {
        uint srcMipDI;
        uint dstMipDI;
    };

    // There is a max limit to the number of lights in the engine.
    // Index 0 of both the light buffer will be
    // reserved for directional light.
    static const uint MAX_LIGHTS = 10;

    // Lights resources / properties for all lights in the scene.
    ConstantBufferStruct LightBuffer
    {
        // Note : lightPosition essentially stores the light direction if the type is directional light.
        // The shader can differentiate between directional and point lights based on the 'w' value. If 1 (i.e it is a position), light is a point light, while
        // if it is zero, then it is a light direction.
        // Light intensity is not automatically multiplied to the light color on the C++ side, shading shader need to manually multiply them.
        float4 lightPosition[MAX_LIGHTS];
        float4 viewSpaceLightPosition[MAX_LIGHTS];

        float4 lightColor[MAX_LIGHTS];
        // float4 because of struct packing (16byte alignment).
        // radiusIntensity[0] stores the radius, while index 1 stores the intensity.
        float4 radiusIntensity[MAX_LIGHTS];

        uint numberOfLights;
        uint _padding[3];
    };

    struct InstanceData
    {
        float4x4 modelMatrix;
        float4x4 inverseModelMatrix;
        uint meshIndex;
        uint _padding[3];
    };

    struct MeshData
    {
        uint indexBufferDI;
        uint indexSize;
        uint indexOffsetBytes;
        uint vertexBufferDI;
        uint vertexBufferOffsetBytes;
        uint vertexStride;
        uint vertexPositionOffset;
        uint vertexNormalOffset;
        uint vertexTangetOffset;
        uint vertexTexCoord0Offset;
        uint vertexTexCoord1Offset;
        uint vertexColorOffset;
        uint materialIdx;
        uint _padding[3];
    };

    struct MaterialData
    {
        TextureSampledViewIndex baseColorTextureDI;
        TextureSampledViewIndex emissiveTextureDI;
        TextureSampledViewIndex metalRoughTextureDI;
        TextureSampledViewIndex occlusionTextureDI;
        TextureSampledViewIndex normalTextureDI;
        float2 normalScale;
        float4 baseColor; // rgb + opacity
        float3 emissiveColor;
        float occlusion;
        float metalness;
        float roughness;
    };

    struct Scene
    {
        // Camera
        float4x4 camViewProjMatrix;
        float4x4 invCamViewProjMatrix;
        float4 camWorldPos; // xyz

        // Sun light
        float3 sunDirection; // Normalized, from light
        float sunIrradiance;
        float3 sunColor;
        float sunAngularSizeRad;
        float4x4 sunWorldToClipMatrix;

        // Sky/ambient light
        float4 ambientTop;      // rgb
        float4 ambientBottom;   // rgb

        // Global descriptors indices
        uint instanceBufferDI;
        uint meshesBufferDI;
        uint materialsBufferDI;
        uint _padding3;
    };

    struct SingleInstanceDrawData
    {
        uint sceneDI;           // DI for the scene constants
        uint instanceIdx;       // Index in the instance data buffer to render
    };

    struct DeferredLightingConstants
    {
        uint sceneDI;
        TextureSampledViewIndex sceneDepthDI;
        TextureSampledViewIndex shadowMapDI;
        TextureSampledViewIndex GBuffer0DI;
        TextureSampledViewIndex GBuffer1DI;
        TextureSampledViewIndex GBuffer2DI;
        TextureSampledViewIndex GBuffer3DI;
    };

    struct DebugStage
    {
        uint sceneDI;
        uint aaboxDI;
    };

    struct AABB
    {
        float3 min;
        float3 max;
    };

    struct CompositeConstants
    {
        TextureSampledViewIndex sceneTextureDI;
        TextureSampledViewIndex uiTextureDI;
        uint colorSpace; // st::rhi::ColorSpace
        float paperWhiteNits;
    };

    struct TonemapConstants
    {
        TextureSampledViewIndex inputTextureDI;
        TextureSampledViewIndex inputAvgLuminanceTextureDI; // 1x1 R32
        TextureStorageViewIndex outputTextureDI;
        float contrast;
        float shoulder;
        float2 bc;
        float middleGray;
        float sdrExposureBias;
    };

    struct BuildLuminanceHistogramConstants
    {
        TextureSampledViewIndex inputTextureDI;
        uint outputHistogramBufferDI; // 256 4-byte (uint32) elements
        uint outputStatsBufferDI;
        uint _padding;
        uint2 viewBegin;
        uint2 viewEnd;
        float minLogLuminance;
        float oneOverLogLuminanceRange;
    };

    struct AvgLuminanceHistogramConstants
    {
        uint inputHistogramBufferDI;
        TextureStorageViewIndex outputAvgLuminanceTextureDI;
        uint outputStatsBufferDI;
        uint pixelCount;
        float minLogLuminance;
        float logLuminanceRange;
        float timeDelta;
        float tau;
    };

    struct TonemappingStatsBuffer
    {
        float minLuminance;
        float maxLuminance;
        float avgLuminance;
        float avgBin;
    };
}

#endif // __SHADERS_INTEROP_RENDERRESOURCES_H__