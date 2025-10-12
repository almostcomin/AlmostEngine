/*
* Copyright (c) 2014-2024, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#ifndef FORWARD_CB_H
#define FORWARD_CB_H

#include "view_cb.h"

#define FORWARD_SPACE_VIEW 2
#define FORWARD_BINDING_VIEW_CONSTANTS 2

struct ForwardShadingViewConstants
{
    PlanarViewConstants view;
};

struct ForwardShadingLightConstants
{
    float2      shadowMapTextureSize;
    float2      shadowMapTextureSizeInv;
    float4      ambientColorTop;
    float4      ambientColorBottom;

    uint2       padding;
    uint        numLights;
    uint        numLightProbes;
};

struct ForwardPushConstants
{
    uint        startInstanceLocation;
    uint        startVertexLocation;
    uint        positionOffset;
    uint        texCoordOffset;
    uint        normalOffset;
    uint        tangentOffset;
};

#endif // FORWARD_CB_H