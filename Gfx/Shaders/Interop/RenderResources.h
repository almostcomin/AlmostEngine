#ifndef __SHADERS_INTEROP_RENDERRESOURCES_H__
#define __SHADERS_INTEROP_RENDERRESOURCES_H__

#include "Interop.h"

namespace interop
{
    struct ClearBufferConstants
    {
        BufferReadWriteIndex bufferDI;
        uint bufferElementCount; // uints
        uint clearValue;
    };

    struct ClearTextureConstants
    {
        TextureStorageViewIndex textureDI;
        uint2 textureDim;
        uint2 _padding;
        float4 clearValue;
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
        TextureSampledViewIndex srcMipDI;
        TextureStorageViewIndex dstMipDI;
    };

    struct LinearizeDepthConstants
    {
        TextureSampledViewIndex srcDepthTexDI;
        TextureStorageViewIndex outLinearDepthTexDI;
        uint width; // texture width in pixels
        uint height;// texture height in pixels
        float nearPlaneDist;
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
        BufferReadOnlyIndex indexBufferDI;
        uint indexSize;
        uint indexOffsetBytes;
        BufferReadOnlyIndex vertexBufferDI;
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

    struct DirLightData
    {
        float3 viewSpaceDirection;
        float irradiance;
        float3 color;
        float halfAngularSize;
    };

    struct PointLightData
    {
        float3 viewSpacePosition;
        float range;
        float3 color; // color * intensity
        float intensity;
        float radius; // radius of the light source
        uint _padding[3];
    };

    struct SpotLightData
    {
        float3 viewSpacePosition;
        float range;
        float3 viewSpaceDirection;
        float intensity;
        float3 color;        
        float radius;
        float innerAngle;
        float outerAngle;
        uint _padding[2];
    };

    struct SceneConstants
    {
        // Camera
        float4x4 camViewProjMatrix;
        float4x4 invCamViewProjMatrix;
        float4x4 camViewMatrix;
        float4x4 invCamViewMatrix;
        float4x4 camProjMatrix;
        float4x4 invCamProjMatrix;
        float4 camWorldPos; // xyz

        // Shadomap matrices
        float4x4 shadowMapWorldToClipMatrix;
        float4x4 shadowMapViewToClipMatrix;

        // Sky/ambient light
        float4 ambientTop;      // rgb
        float4 ambientBottom;   // rgb

        DirLightData mainDirLight;

        // Lights
        uint dirLightCount;
        BufferReadOnlyIndex dirLightsDataDI; // DirLightData
        uint pointLightCount;
        BufferReadOnlyIndex pointLightsDataDI; // PointLightData
        uint spotLightCount;
        BufferReadOnlyIndex spotLightsDataDI;

        // Global descriptors indices
        BufferReadOnlyIndex instanceBufferDI;
        BufferReadOnlyIndex meshesBufferDI;
        BufferReadOnlyIndex materialsBufferDI;
    };

    struct SingleInstanceDrawData
    {
        BufferUniformIndex sceneDI; // DI for the scene constants
        uint instanceIdx;           // Index in the instance data buffer to render
    };

    struct MultiInstanceDrawConstants
    {
        BufferUniformIndex sceneDI;
        BufferReadOnlyIndex instancesDI;
        uint baseInstanceIdx;
        uint meshIndex;
        uint materialIndex;
    };

    struct DeferredLightingConstants
    {
        BufferUniformIndex sceneDI;
        TextureSampledViewIndex sceneDepthDI;
        TextureSampledViewIndex shadowMapDI;
        TextureSampledViewIndex GBuffer0DI;
        TextureSampledViewIndex GBuffer1DI; // offset 16
        TextureSampledViewIndex GBuffer2DI;
        TextureSampledViewIndex GBuffer3DI;
        TextureSampledViewIndex SSAO_DI;
        float2 oneOverShadowmapResolution; // offset 32
        uint MaterialChannel; 
        uint ShowSSAO;
        uint ShowShadowmap;
    };

    struct DebugStage
    {
        BufferUniformIndex sceneDI;
        BufferReadOnlyIndex aaboxDI;
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
        BufferReadWriteIndex outputHistogramBufferDI; // 256 4-byte (uint32) elements
        BufferReadWriteIndex outputStatsBufferDI;
        uint _padding;
        uint2 viewBegin;
        uint2 viewEnd;
        float minLogLuminance;
        float oneOverLogLuminanceRange;
    };

    struct AvgLuminanceHistogramConstants
    {
        BufferReadOnlyIndex inputHistogramBufferDI;
        TextureStorageViewIndex outputAvgLuminanceTextureDI;
        BufferReadWriteIndex outputStatsBufferDI;
        uint pixelCount;
        float minLogLuminance;
        float logLuminanceRange;
        float timeDelta;
        float adaptionSpeedUp;
        float adaptionSpeedDown;
    };

    struct TonemappingStatsBuffer
    {
        float minLuminance;
        float maxLuminance;
        float avgLuminance;
        float avgBin;
    };

    struct SSAOConstants
    {
        BufferUniformIndex sceneDI;
        TextureSampledViewIndex depthTextureDI;
        TextureSampledViewIndex normalsTextureDI; // GBuffer2.xz
        TextureStorageViewIndex outputAOTextureDI;
        uint textureWidth;          // offset 16
        uint textureHeight;
        float radiusWorld;
        float invBackgroundViewDepth;
        float2 clipToWindowScale;   // offset 32
        float2 clipToWindowBias;
        float2 windowToClipScale;   // offset 48
        float2 windowToClipBias;
        float2 clipToView;          // offset 64
        float radiusToScreen;
        float power;
        float surfaceBias;          // offset 80
    };

    struct BilaterialBlurConstants
    {
        TextureSampledViewIndex inputTextureDI;
        TextureSampledViewIndex depthTextureDI; // linear
        TextureStorageViewIndex outputTextureDI;
        float textureWidth;
        float textureHeight;
    };

    struct DrawChunk
    {
        float4x4 modelMatrix;
        uint meshIndex;
        uint materialIndex;
    };
}

#endif // __SHADERS_INTEROP_RENDERRESOURCES_H__