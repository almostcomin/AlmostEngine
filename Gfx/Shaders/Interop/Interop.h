#ifndef __SHADER_INTEROP_INTEROP_H__
#define __SHADER_INTEROP_INTEROP_H__

#ifdef __cplusplus // not invoking shader compiler, but included in engine source

#include "Core/Math.h"

using uint = uint32_t;

static constexpr uint INVALID_DESCRIPTOR_INDEX = 0xFFFFFFFFu;

static_assert(sizeof(float4x4) == 64);

#define column_major
#define row_major

#define ConstantBufferStruct struct alignas(256)

#else

static const uint INVALID_DESCRIPTOR_INDEX = 0xFFFFFFFFu;

#define ConstantBufferStruct struct

#endif // __cplusplus
#endif // __SHADER_SHADERINTEROP_H__