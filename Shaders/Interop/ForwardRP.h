#ifndef __SHADERS_INTEROP_FORWARD_RP_H__
#define __SHADERS_INTEROP_FORWARD_RP_H__

#include "Interop.h"

namespace interop
{
    struct MeshCB
    {
        uint indexBuffer;
        uint indexOffset;
        uint vertexBuffer;
        uint vertexBufferOffsetBytes;
        uint vertexPosStride;
        uint vertexNormalStride;
        uint vertexTangetStride;
        uint vertexTexCoord0Stride;
        uint vertexTexCoord1Stride;
        uint vertexColorStride;
        uint vertexStride;
    };

    struct MaterialCB
    {
        uint textureIndex;
    };

    struct ForwardRP
    {
        uint CameraCBIndex;
        uint MeshCBIndex;
        uint MaterialCBIndex;
        uint TransformCBIndex;
    };

    struct FullScreenTrianglePassRenderResources
    {
        uint renderTextureIndex;
    };
}

#endif // __SHADERS_INTEROP_RENDERRESOURCES_H__