#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

#define M_PI 3.14159265358979323846
#define M_INV_PI 0.31830988618379067154

// Typically invMatrix is invViewProjMatrix for world pos reconstruction
// or invProjMatrix for view space pos recosntruction
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

float SampleShadowMap(float4 worldPos, float4x4 worldToClipMatrix, Texture2D shadowMap)
{
    float4 clipPos = mul(worldToClipMatrix, worldPos);
    float3 ndcPos = clipPos.xyz / clipPos.w;
    // UV
    float2 shadowUV = ndcPos.xy * 0.5 + 0.5;
    shadowUV.y = 1.0 - shadowUV.y;
        
    // Sample shadowmap
    float shadowDepth = shadowMap.Sample(pointClampSampler, shadowUV).r;
    
    float shadow = ndcPos.z < shadowDepth ? 0.0 : 1.f; // Reverse-Z compare    
    return shadow;
}

float3 ApplyNormalMap(float3 vNormal, float4 vTangent, float3 tNormal, float2 normalScale)
{
    // Remap to [-1,1]
    float3 tangentNormal = tNormal * 2.0 - 1.0;
    // Apply scale
    tangentNormal.xy *= normalScale;
    // Build TBN matrix
    float3 N = vNormal;
    float3 T = normalize(vTangent.xyz);
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

#endif // __COMMON_HLSLI__