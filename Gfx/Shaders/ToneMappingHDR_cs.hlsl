#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

float3 HDRHighlightRolloff(float3 x, float knee, float maxNits)
{
    float3 result;
    result.r = x.r < knee ? x.r : knee + (x.r - knee) / (1.0 + (x.r - knee) / (maxNits - knee));
    result.g = x.g < knee ? x.g : knee + (x.g - knee) / (1.0 + (x.g - knee) / (maxNits - knee));
    result.b = x.b < knee ? x.b : knee + (x.b - knee) / (1.0 + (x.b - knee) / (maxNits - knee));
    return result;
}

ConstantBuffer<interop::TonemapConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4> exposedTexture = ResourceDescriptorHeap[Constants.InputExposedTextureDI];
    Texture2D<float4> bloomTexture = ResourceDescriptorHeap[Constants.InputBloomTextureDI];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[Constants.OutputTextureDI];
    
    if (any(DTid.xy > Constants.TextureDims))
        return;
    
    float4 color = exposedTexture[DTid.xy];
    color.rgb += bloomTexture[DTid.xy].rgb;
    color.rgb = HDRHighlightRolloff(color.rgb, 5000.0, 10000.0);
        
    outputTexture[DTid.xy] = color;
}