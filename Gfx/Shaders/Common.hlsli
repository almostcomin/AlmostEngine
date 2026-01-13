
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

float3 DecodeSnorm8(uint n)
{
    int x = int(n << 24) >> 24;
    int y = int(n << 16) >> 24;
    int z = int(n << 8) >> 24;

    return normalize(float3(x, y, z) / 127.0);
}