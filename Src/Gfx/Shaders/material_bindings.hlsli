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

#ifndef MATERIAL_BINDINGS_HLSLI
#define MATERIAL_BINDINGS_HLSLI

#include "binding_helpers.hlsli"

// Bindings - can be overriden before including this file if necessary

#ifndef MATERIAL_DIFFUSE_SLOT 
#define MATERIAL_DIFFUSE_SLOT 0
#endif

#ifndef MATERIAL_REGISTER_SPACE
#define MATERIAL_REGISTER_SPACE 0
#endif

#ifndef MATERIAL_SAMPLER_SLOT 
#define MATERIAL_SAMPLER_SLOT 0
#endif

#ifndef MATERIAL_SAMPLER_REGISTER_SPACE
#define MATERIAL_SAMPLER_REGISTER_SPACE 0
#endif

Texture2D t_BaseOrDiffuse : REGISTER_SRV(MATERIAL_DIFFUSE_SLOT, MATERIAL_REGISTER_SPACE);
SamplerState s_MaterialSampler : REGISTER_SAMPLER(MATERIAL_SAMPLER_SLOT, MATERIAL_SAMPLER_REGISTER_SPACE);

float4 SampleDiffuse(float2 texCoord)
{
    return t_BaseOrDiffuse.Sample(s_MaterialSampler, texCoord);
}

#endif