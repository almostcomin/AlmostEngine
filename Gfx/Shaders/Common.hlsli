float2 LoadVertexAttribute2(ByteAddressBuffer buffer, uint offset, uint stride)
{
    if (stride == 0xFFFFFFFF)
        return float2(0, 0);
    return asfloat(buffer.Load2(offset + stride));
};

float3 LoadVertexAttribute3(ByteAddressBuffer buffer, uint offset, uint stride)
{
    if (stride == 0xFFFFFFFF)
        return float3(0, 0, 0);
    return asfloat(buffer.Load3(offset + stride));
};