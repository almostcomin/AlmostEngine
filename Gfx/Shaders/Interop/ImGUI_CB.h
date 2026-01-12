#ifndef __SHADERS_INTEROP_IMGUI_CB_H__
#define __SHADERS_INTEROP_IMGUI_CB_H__

#include "Interop.h"

namespace interop
{
    ConstantBufferStruct ImGUI_CB
    {
        float2 invDisplaySize;
        uint indexBuffer;
        uint indexOffset;
        uint vertexBuffer;
        uint vertexBufferOffset;
        uint textureIndex;
        uint flags;
    };

} // namespace interlop

#endif // __SHADERS_INTEROP_IMGUI_CB_H__