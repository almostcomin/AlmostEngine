#ifndef __SHADER_INTEROP_INTEROP_H__
#define __SHADER_INTEROP_INTEROP_H__

#ifdef __cplusplus // not invoking shader compiler, but included in engine source

#include "Core/Math.h"

using int2 = st::int2;
using int3 = st::int3;
using int4 = st::int4;
using uint = uint32_t;
using uint2 = st::uint2;
using uint3 = st::uint3;
using uint4 = st::uint4;
using float2 = st::float2;
using float3 = st::float3;
using float4 = st::float4;
using float3x3 = st::float3x3;
using float4x4 = st::float4x4;

#define column_major
#define row_major

#define ConstantBufferStruct struct alignas(256)

#else

#define ConstantBufferStruct struct

// Set the matrix packing to row major by default. Prevents needing to transpose matrices on the C++ side.
#pragma pack_matrix(row_major)

#endif // __cplusplus
#endif // __SHADER_SHADERINTEROP_H__