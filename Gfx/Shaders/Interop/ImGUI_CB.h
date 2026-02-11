#ifndef __SHADERS_INTEROP_IMGUI_CB_H__
#define __SHADERS_INTEROP_IMGUI_CB_H__

#include "Interop.h"

namespace interop
{
    struct ImGUI_CB
    {
        float2 invDisplaySize;
        BufferReadOnlyIndex indexBuffer;
        uint indexOffset;
        BufferReadOnlyIndex vertexBuffer;
        uint vertexBufferOffset;
        TextureSampledViewIndex textureIndex;
        uint flags;
    };

} // namespace interlop

#endif // __SHADERS_INTEROP_IMGUI_CB_H__