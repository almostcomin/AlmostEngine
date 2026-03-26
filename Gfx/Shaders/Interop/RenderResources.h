#ifndef __SHADERS_INTEROP_RENDERRESOURCES_H__
#define __SHADERS_INTEROP_RENDERRESOURCES_H__

#include "Interop.h"

// Structs wich name end in 'Constants' are tipically constant buffer (being SceneConstants the only one at the moment of writing this comment)
// Any other can be structured buffer data or root constants
// Special care needed for the alignment rules: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules

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
        TextureSampledViewIndex baseColorTextureDI;     // offset 0
        TextureSampledViewIndex emissiveTextureDI;      // offset 4
        TextureSampledViewIndex metalRoughTextureDI;    // offset 8
        TextureSampledViewIndex occlusionTextureDI;     // offset 12
        TextureSampledViewIndex normalTextureDI;        // offset 16
        uint _padding0;                                 // offset 20
        float2 normalScale;                             // offset 24
        float4 baseColor; // rgb + opacity              // offset 32
        float3 emissiveColor;                           // offset 48
        float occlusion;                                // offset 60
        float metalness;                                // offset 64
        float roughness;                                // offset 68
        float alphaCutoff;                              // offset 72
        uint _padding1;                                 // offset 76
    };                                                  // size 80

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
        float2 invScreenResolution;
        // Warning! can't use _padding[2] since in a constant buffer each array element consumes 4 bytes:
        // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules#more-aggressive-packing
        uint _padding0;        
        uint _padding1;

        // Camera
        float4x4 camViewProjMatrix;             // offset 16
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
        BufferReadOnlyIndex dirLightsDataDI;    // DirLightData
        uint pointLightCount;
        BufferReadOnlyIndex pointLightsDataDI;  // PointLightData
        uint spotLightCount;
        BufferReadOnlyIndex spotLightsDataDI;   // SpotLightData

        // Global descriptors indices
        BufferReadOnlyIndex instanceBufferDI;   // InstanceData
        BufferReadOnlyIndex meshesBufferDI;     // MeshData
        BufferReadOnlyIndex materialsBufferDI;  // MaterialData
    };

    struct DepthPrepassStageConstants
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
    };

    struct GBufferStageConstats
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
    };

    struct ShadowmapStageConstats
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
    };

    struct WBOITAccumStageConstants
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        TextureSampledViewIndex shadowMapDI;
        float2 oneOverShadowmapResolution;
        TextureSampledViewIndex SSAO_DI;
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
    };

    struct WBOITResolveStageConstants
    {
        TextureSampledViewIndex accumDI;
        TextureSampledViewIndex revealageDI;
    };

    struct WireframeStageConstats
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
    };

    struct MultiInstanceDrawConstants
    {
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
        TextureSampledViewIndex depthTextureDI; // linearized
        TextureStorageViewIndex outputTextureDI;
        float textureWidth;
        float textureHeight;
    };

    struct BloomDownsampleConstants
    {
        TextureSampledViewIndex inputTextureDI;
        float2 invInputTexResolution;
    };

    struct BloomUpsampleConstants
    {
        TextureSampledViewIndex inputTextureDI;
        float filterRadius;
    };

    struct BloomMixConstants
    {
        TextureSampledViewIndex sceneTextureDI;
        TextureSampledViewIndex bloomTextureDI;
        float bloomStrength;
    };
}

#endif // __SHADERS_INTEROP_RENDERRESOURCES_H__