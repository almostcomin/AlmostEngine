#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

#define M_PI 3.14159265358979323846
#define M_INV_PI 0.31830988618379067154
#define POS_INFINITY asfloat(0x7F800000)
#define NEG_INFINITY asfloat(0xFF800000)

// Typically:
//   invMatrix is invViewProjMatrix for world space position reconstruction
//   invMatrix is invProjMatrix for view space position reconstruction
float4 PosReconstruction(float2 uv, float depth, float4x4 invViewProjMatrix)
{
    float4 ndcPos;
    ndcPos.x = uv.x * 2.0 - 1.0;
    ndcPos.y = 1.0 - uv.y * 2.0; // Y flip D3D
    ndcPos.z = depth;
    ndcPos.w = 1.0;
    
    float4 pos = mul(invViewProjMatrix, ndcPos);
    pos /= pos.w;
    return pos;
}

float3 ApplyNormalMap(float3 vNormal, float4 vTangent, float3 tNormal, float2 normalScale)
{
    // Remap to [-1,1]
    float3 tangentNormal = tNormal * 2.0 - 1.0;
    // Apply scale
    tangentNormal.xy *= normalScale;
    // Build TBN matrix
    float3 N = vNormal;
    float3 T = vTangent.xyz;
    float3 B = cross(N, T) * vTangent.w; // w is headedness
    float3x3 TBN = float3x3(T, B, N);
        
    return normalize(mul(tangentNormal, TBN));
}

uint GetIndex(ByteAddressBuffer indexBuffer, uint indexOffsetBytes, uint indexSize, uint vertexID)
{
    if (indexSize == 2)
    {
        uint word = indexBuffer.Load(indexOffsetBytes + (vertexID & ~1) * 2);
        return (vertexID & 1) ? (word >> 16) : (word & 0xFFFF);
    }
    else
    {
        return indexBuffer.Load(indexOffsetBytes + vertexID * 4);
    }
}

float2 LoadVertexAttributeFloat2(ByteAddressBuffer buffer, uint vertexOffsetBytes, uint attributeOffsetBytes)
{
    if (attributeOffsetBytes == 0xFFFFFFFF)
        return float2(0, 0);
    return asfloat(buffer.Load2(vertexOffsetBytes + attributeOffsetBytes));
};

float3 LoadVertexAttributeFloat3(ByteAddressBuffer buffer, uint vertexOffsetBytes, uint attributeOffsetBytes)
{
    if (attributeOffsetBytes == 0xFFFFFFFF)
        return float3(0, 0, 0);
    return asfloat(buffer.Load3(vertexOffsetBytes + attributeOffsetBytes));
};

uint LoadVertexAttributeUInt(ByteAddressBuffer buffer, uint vertexOffsetBytes, uint attributeOffsetBytes)
{
    if (attributeOffsetBytes == 0xFFFFFFFF)
        return 0;
    return buffer.Load(vertexOffsetBytes + attributeOffsetBytes);
}

// Octahedron encoding
float2 EncodeNormal(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 enc = n.xy;
    if (n.z < 0.0)
    {
        enc = (1.0 - abs(enc.yx)) * select(enc.xy >= 0.0, 1.0, -1.0);
    }
    return enc * 0.5 + 0.5; // [1,-1] -> [0,1]
}

// Octahedron decoding
float3 DecodeNormal(float2 enc)
{
    enc = enc * 2.0 - 1.0;
    float3 n = float3(enc.xy, 1.0 - abs(enc.x) - abs(enc.y));
    float t = saturate(-n.z);
    n.xy += select(n.xy >= 0.0, -t, t);
    return normalize(n);
}

float3 Unpack_RGB8_SNORM(uint n)
{
    int x = int(n << 24) >> 24;
    int y = int(n << 16) >> 24;
    int z = int(n << 8) >> 24;

    return float3(x, y, z) / 127.0;
}

float4 Unpack_RGBA8_SNORM(uint n)
{
    int x = int(n << 24) >> 24;
    int y = int(n << 16) >> 24;
    int z = int(n << 8) >> 24;
    int w = int(n) >> 24;

    return float4(x, y, z, w) / 127.0;
}

float square(float x)
{
    return x * x;
}

float3 slerp(float3 a, float3 b, float angle, float t)
{
    t = saturate(t);
    float sin1 = sin(angle * t);
    float sin2 = sin(angle * (1 - t));
    float ta = sin1 / (sin1 + sin2);
    float3 result = lerp(a, b, ta);
    return normalize(result);
}

// Given v in the range [srcMin,  srcMax] returns it remapes in the range [dstMin, dstMax]
float remap(float v, float srcMin, float srcMax, float dstMin, float dstMax)
{
    return (((v - srcMin) / (srcMax - srcMin)) * (dstMax - dstMin)) + dstMin;
}

// Ray-plane intersection wuth horizontal plane
// Return distance from origin to intersection point
// Returns -1 if no intersection (parallel ray or wrong direction)
float RayPlaneIntersection(float3 origin, float3 dir, float planeY)
{
    if (abs(dir.y) < 0.0001f)
        return -1.0f;
    return (planeY - origin.y) / dir.y;
}

// Generic ray-sphere intersection
// Returns the distance t from origin to the first intersection point
// Returns -1.0 if no intersection or intersection is behind the ray
float RaySphereIntersection(float3 rayOrigin, float3 rayDirection, float3 sphereCenter, float sphereRadius)
{
    float3 oc = rayOrigin - sphereCenter;
    float b = dot(oc, rayDirection);
    float c = dot(oc, oc) - square(sphereRadius);
    float d = b * b - c;
    
    if (d < 0.0f)
        return -1.0f; // no intersection
    
    float sqrtD = sqrt(d);
    float t0 = -b - sqrtD; // near intersection
    float t1 = -b + sqrtD; // far intersection
    
    // Return nearest positive t
    if (t0 >= 0.0f)
        return t0; // ray enters the sphere
    if (t1 >= 0.0f)
        return t1; // rayOrigin is inside the sphere
    return -1.0f; // sphere is entirely behind the ray
}

#endif // __COMMON_HLSLI__