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
        uint _padding0;
        float4 clearValue;
    };

    struct BlitGraphicsConstants
    {
        TextureSampledViewIndex textureDI;
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
    };

    struct InstanceCullData
    {
        float4 BoundsSphere;    // xyz = center, w = radius
        uint BatchIndex;
        uint Flags;             // CastShadows, etc.
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

    struct TerrainMaterialLayer
    {
        TextureSampledViewIndex BaseColorTextureDI;
        TextureSampledViewIndex NormalTextureDI;
        TextureSampledViewIndex MetalRoughTextureDI;
        float UVScale;
        float3 BaseColorTint;
        float Roughness;
        float Metalness;
        uint3 _padding0;
    };

    struct TerrainMaterialData
    {
        TerrainMaterialLayer GroundLayer;
        TerrainMaterialLayer SlopeLayer;
        TerrainMaterialLayer PeakLayer;
        float SlopeAngleStartCos;
        float SlopeAngleEndCos;
        float HeightTransitionStart;
        float HeightTransitionEnd;
        float SlopeBlendSharpness;
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

    struct HeightmapPatchData
    {
        float2 MinUV;           
        float2 UVScale;
        float SizeUV;                       // offset 4
        uint MipLevel;
        uint2 TextureResolution;
        // 3x3 normal matrix, stored as 3 float4 columns (w unused)
        float4 NormalMatrixCol0;            // offset 8
        float4 NormalMatrixCol1;
        float4 NormalMatrixCol2;
        float4x4 InverseHeightmapMatrix;    // offset 20
        uint EdgeMask;                      // bits: bit0=N Low?, bit1=S Low?, bit2=E Low?, bit3=W Low?
        float CellSize;
        float CellUVScale;
        TextureSampledViewIndex HeightmapTextureDI; 
    }; // size 40 * 4 = 160 bytes

    // Warning! can't use _padding[2] since in a constant buffer each array element consumes 4 bytes:
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules#more-aggressive-packing
    // TODO: Split this in two buffers: Per scene & per render-view
    struct SceneConstants
    {
        float2 screenResolution;
        float2 invScreenResolution;
        float aspect;
        float time;
        float deltaTime;
        uint _padding0;
        float4 mouseState;                      // .xy pos, .zw: leftBtn, rightBtn

        // Camera
        float4x4 camViewProjMatrix;             // offset 12
        float4x4 invCamViewProjMatrix;          // offset 28
        float4x4 camViewMatrix;                 // offset 44
        float4x4 invCamViewMatrix;              // offset 60
        float4x4 camProjMatrix;                 // offset 76
        float4x4 invCamProjMatrix;              // offset 92
        float3 camWorldPos;                     // offset 108
        float camZNear;
        float4 frustumPlanes[6];                // offset 112

        // Shadomap matrices
        float4x4 shadowMapWorldToClipMatrix;    // offset 136
        float4x4 shadowMapViewToClipMatrix;     // offset 152
        float4 shadowCasterPlanes[6];           // offset 112
        // Clouds shadowmap
        float4x4 CloudsShadowmapWorldToClipMatrix;
        float3 CloudsShadowmapSunPosition;
        uint _padding1;

        // Sky/ambient light
        float4 ambientTop;      // rgb
        float4 ambientBottom;   // rgb

        // Lights
        DirLightData mainDirLight;
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
        BufferReadOnlyIndex terrainMaterialsBufferDI; // TerrainMaterialData
        BufferReadOnlyIndex patchDataBufferDI;  // HeightmapPatchData
    };

    struct DepthPrepassStageConstants
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
        BufferReadOnlyIndex payloadDI;      // VisibleInstancePayload
    };

    struct GBufferStageConstats
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
        BufferReadOnlyIndex payloadDI;      // VisibleInstancePayload
        uint DebugChannel;                  // GBuffersRenderStage::DebugChannel
    };

    struct ShadowmapStageConstats
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
        BufferReadOnlyIndex payloadDI;      // VisibleInstancePayload
    };

    struct WBOITAccumStageConstants
    {
        BufferUniformIndex sceneDI;         // SceneConstants
        TextureSampledViewIndex shadowMapDI;
        float2 oneOverShadowmapResolution;
        TextureSampledViewIndex SSAO_DI;
        BufferReadOnlyIndex instancesDI;    // array of uint32 (indices to SceneConstants::instanceBufferDI)
        BufferReadOnlyIndex payloadDI;      // VisibleInstancePayload
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
        BufferReadOnlyIndex payloadDI;      // VisibleInstancePayload
    };

    struct MultiInstanceDrawConstants
    {
        uint baseInstanceIdx;               // Base index into Visibily Buffer
        uint rawInstanceBaseIdx;            // Base index into InstanceData
        uint meshIndex;                     // Index into MeshData Buffer
        uint materialIndex;                 // Index into MaterialData Buffer
        uint extraDataBaseIdx;              // Base offset for shader-specific extra data buffers. Heightmap PatchData base for instance.
    };

    struct BatchTableEntry
    {
        uint MeshIndex;
        uint MaterialIndex;
        uint ExtraDataBaseIdx;
        uint RegionOffset;
        uint MaxInstances;
        uint IndexCount;
    };

    struct IndirectDrawCommand
    {
        uint VertexCountPerInstance;        // = IndexCount (prefill desde BatchTable)
        uint InstanceCount;
        uint StartVertexLocation;
        uint StartInstanceLocation;
    };

    struct VisibleInstancePayload
    {
        uint InstanceIndex;
        uint MeshIndex;
        uint MaterialIndex;
        uint ExtraDataBaseIdx;
    };

    struct CullingConstants
    {
        BufferUniformIndex SceneDI;             // SceneConstants
        BufferReadWriteIndex CameraArgsDI;      // IndirectDrawCommand
        BufferReadWriteIndex CameraPayloadDI;   // VisibleInstancePayload
        BufferReadWriteIndex ShadowArgsDI;      // IndirectDrawCommand
        BufferReadWriteIndex ShadowPayloadDI;   // VisibleInstancePayload
        BufferReadOnlyIndex  BatchTableDI;
        BufferReadOnlyIndex  CullDataDI;        // InstanceCullData
        uint BatchCount;                        // = BatchTable.size()
        uint InstanceCount;                     // Alive static instances
        uint ShadowEnabled;
    };

    struct DeferredLightingConstants
    {
        BufferUniformIndex sceneDI;
        TextureSampledViewIndex sceneDepthDI;
        TextureSampledViewIndex shadowMapDI;
        TextureSampledViewIndex GBuffer0DI;

        TextureSampledViewIndex GBuffer1DI;
        TextureSampledViewIndex GBuffer2DI;
        TextureSampledViewIndex GBuffer3DI;
        TextureSampledViewIndex SSAO_DI;

        TextureSampledViewIndex CloudsShadowmapDI;
        float2 oneOverShadowmapResolution; 
        uint MaterialChannel; 

        uint ShowSSAO;
        uint ShowShadowmap;
        uint2 _padding0;
    };

    struct DebugStageBBoxes
    {
        BufferUniformIndex sceneDI;
        BufferReadOnlyIndex aaboxDI;
    };

    struct DebugStageS2H
    {
        BufferUniformIndex sceneDI;
        TextureStorageViewIndex inputTextureDI;
        TextureStorageViewIndex outputTextureDI;
        TextureSampledViewIndex GBuffer2DI;
    };

    struct AABB
    {
        float3 min;
        uint _padding0;
        float3 max;
        uint _padding1;
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
        TextureStorageViewIndex outputTextureDI;
        uint2 outputTexResolution;
        float2 inputTexInvResolution;
        uint2 _padding0;
    };

    struct BloomUpsampleConstants
    {
        TextureSampledViewIndex inputTextureDI;
        TextureStorageViewIndex outputTextureDI;
        float2 outputTexInvResolution;
        uint2 outputTexResolution;
        float filterRadius;
        uint _padding0;
    };

    struct BloomMixConstants
    {
        TextureSampledViewIndex sceneTextureDI;
        TextureSampledViewIndex bloomTextureDI;
        float bloomStrength;
    };

    struct SimpleSkyData
    {
        float3 directionToLight;
        float angularSizeOfLight;
        float3 lightColor;
        float glowSize;
        float3 skyColor;
        float glowIntensity;
        float3 horizonColor;
        float horizonSize;
        float3 groundColor;
        float glowSharpness;
        float3 directionUp;
        float aspect;
        float2 resolution;
    };

    struct SimpleSkyConstants
    {
        float4x4 matClipToTranslatedWorld;
        BufferUniformIndex skyDataDI; // SimpleSkyData
    };

    struct SkyData
    {
        float3 ToSunDirection;
        float AtmosRadius;
        float3 SunColor;
        float SunIntensity;         // Atmospheric scattering arbitrary 
        float3 EarthCenter;
        float EarthRadius;
        float3 bR;                  // Rayleigh scattering coefficients at sea level (per meter)
        float Hr;                   // Rayleigh scale height
        float3 bM;                  // Mie scattering coefficients at sea level (per meter)
        float Hm;                   // Mie scale height
        float3 SunRadiance;         // Sun bright for the sun disk
        float G;                    // Mie anisotropy
        float SunAngularRadius;     // Sun radius angular size
        float SunAngularRadiusCos;  // Cosine of sun radius
        float SunEdgeAAFalloff;
        TextureSampledViewIndex LinearDepthTexDI;
        float3 CameraForward;
        uint NumSteps;
        uint NumLightSteps;
        TextureStorageViewIndex SceneColorDI;
        float2 SceneColorTexSize;
    };

    struct SkyConstants
    {
        float4x4 matClipToTranslatedWorld;
        float3 CameraPosition;
        BufferUniformIndex SkyDataDI;  // SkyData
    };

    struct CloudsShapeData
    {
        TextureSampledViewIndex BaseShapeTexture;
        TextureSampledViewIndex DetailTexture;
        float2 WindOffset;

        float2 WindDir;
        float ShapeScale;
        float DetailScale;

        float DetailErosionStrength;
        float Coverage;
        float StratusWeight;
        float CumulusWeight;

        float CumulonimbusWeight;
        float CloudLayerMinH;
        float CloudLayerMaxH;
        uint _padding0;

        float3 EarthCenter;
        float EarthRadius;

        float InvCloudLayerThickness;
        float muT;
        float muS;
        float Albedo;

        float AnimTime;
        float ShearTiltMeters;
        float SwayAmpMeters;
        float SwaySpeed;

        float SwirlSpeed;
        float SwirlRadius;
        float MorphSpeed;
        uint _padding1;
    };

    struct CloudsData
    {
        TextureStorageViewIndex DstTextureDI;
        TextureSampledViewIndex CloudsShadowMapDI;
        uint2 DstTextureSize;

        TextureSampledViewIndex linearDepthTexDI;
        TextureSampledViewIndex prevCloudsTexDI;
        float cloudFadeDistance;
        uint _padding1;

        float3 toSunDirection;
        uint _padding2;

        float3 cameraForward;
        uint _padding3;

        uint maxSteps;
        uint lightSteps;
        uint2 _padding4;

        float4x4 matPrevFrameViewProj;

        float3 sunT;
        float invCloudFadeDistance;

        float3 sunB;
        uint multiScatterOctaves;

        float ambientStrength;
        float multiScatterEccentricity;
        float multiScatterContribution;
        uint _padding5;

        float3 sunRadiance;
        float multiScatterOcclusion;

        float3 sunIrradiance;
        float phaseGForward;

        float phaseGBackward;
        float multiScatterBaseG;
        float powderStrength;
        float powderEdgeWidth;

        uint volumetricShadows;
        float depthThreshold;
        float blendFactor;
        uint _padding6;

        float4x4 ShadowWorldToClip;
        float3 SunPos;
        uint _padding7;
    };

    struct CloudsConstants
    {
        float4x4 matClipToTranslatedWorld;
        float3 cameraPosition;
        BufferUniformIndex cloudsShapeDataDI;   // CloudsShapeData
        BufferUniformIndex cloudsDataDI;        // CloudsData
        uint frameCounter;
        uint debugChannel;
    };

    struct CloudsShadowmapData
    {
        TextureStorageViewIndex DstTextureDI;
        TextureSampledViewIndex LinearDepthTexDI;
        TextureSampledViewIndex CloudsBaseShapeTexture;
        TextureSampledViewIndex CloudsDetailTexture;
        uint2 DstTextureSize;
        uint2 _padding0;
        float4x4 MatClipToTranslatedWorld;
        float3 SunPos;
        uint _padding1;
        float3 SunDir;
        float zNear;
        uint RayMarchStepCount;
        float muT;
        uint2 _padding2;
    };

    struct CloudsShadowmapConstants
    {
        BufferUniformIndex CloudsShapeDataDI;     // CloudsShapeData
        BufferUniformIndex CloudsShadowmapDataDI; // CloudsShadowmapData
    };

    struct HeightmapDebugConstants
    {
        TextureStorageViewIndex inputTextureDI;
        TextureStorageViewIndex outputTextureDI;
        float2 screenPos;
        uint2 coords;
        uint level;
        float errWorld;
    };
}

#endif // __SHADERS_INTEROP_RENDERRESOURCES_H__