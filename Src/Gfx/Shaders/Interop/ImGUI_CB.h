#ifndef __SHADERS_INTEROP_IMGUI_CB_H__
#define __SHADERS_INTEROP_IMGUI_CB_H__

#include "Interop.h"

namespace interop
{
    ConstantBufferStruct ImGUI_CB
    {
        float2 invDisplaySize;
        uint positionBufferIndex;
        uint textureCoordBufferIndex;
        uint colorBufferIndex;
        uint textureIndex;
    };

} // namespace interlop

#endif // __SHADERS_INTEROP_IMGUI_CB_H__