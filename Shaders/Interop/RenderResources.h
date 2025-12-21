#ifndef __SHADERS_INTEROP_RENDERRESOURCES_H__
#define __SHADERS_INTEROP_RENDERRESOURCES_H__

#include "Interop.h"

namespace interop
{
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
        uint indexOffset;
        uint vertexBufferDI;
        uint vertexBufferOffsetBytes;
        uint vertexStride;
        uint vertexPositionStride;
        uint vertexNormalStride;
        uint vertexTangetStride;
        uint vertexTexCoord0Stride;
        uint vertexTexCoord1Stride;
        uint vertexColorStride;
        uint textureDI;
    };

    struct Scene
    {
        float4x4 viewProjectionMatrix;
        //float4x4 projectionMatrix;
        //float4x4 inverseProjectionMatrix;
        //float4x4 viewMatrix;
        //float4x4 inverseViewMatrix;
    };

    struct OpaqueStage
    {
        uint sceneDI;
        uint instanceBufferDI;
        uint meshesBufferDI;
        uint instanceIdx;
    };

    struct BlitConstants
    {
        uint textureDI;
    };
}

#endif // __SHADERS_INTEROP_RENDERRESOURCES_H__