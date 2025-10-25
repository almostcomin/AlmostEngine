#ifndef __SHADERS_INTEROP_RENDERRESOURCES_H__
#define __SHADERS_INTEROP_RENDERRESOURCES_H__

#include "Interop.h"

namespace interop
{
    struct TriangleRenderResources
    {
        uint cameraBufferIndex;
        uint positionBufferIndex;
        uint textureCoordBufferIndex;
        uint textureIndex;
    };

    struct FullScreenTrianglePassRenderResources
    {
        uint renderTextureIndex;
    };
}

#endif // __SHADERS_INTEROP_RENDERRESOURCES_H__