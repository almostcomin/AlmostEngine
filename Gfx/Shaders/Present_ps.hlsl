#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Color.hlsli"

// These values must match the st::rhi::ColorSpace enum
#define COLOR_SPACE_SRGB            0
#define COLOR_SPACE_HDR10_ST2084    1
#define COLOR_SPACE_HDR_LINEAR      2

ConstantBuffer<interop::PresentConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    Texture2D sceneTexture = ResourceDescriptorHeap[Constants.sceneTextureDI];
    
    float4 color = sceneTexture.Sample(pointClampSampler, input.uv);
    
    if (Constants.colorSpace == COLOR_SPACE_SRGB)
    {
        // assuming back-buffer is SRGB, so no thing to do here
    }
    else if (Constants.colorSpace == COLOR_SPACE_HDR10_ST2084)
    {
        // https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HDR/src/presentPS.hlsl
        const half referenceWhiteNits = 80.0;
        const half st2084max = 10000.0;
        const half hdrScalar = referenceWhiteNits / st2084max;
	    // The input is in Rec.709, but the display is Rec.2020
        color.rgb = Rec709ToRec2020(color.rgb);
        // Apply the ST.2084 curve to the result.
        color.rgb = LinearToST2084(color.rgb * hdrScalar);
    }
    else // colorSpace == COLOR_SPACE_HDR_LINEAR
    {
        // Just pass through
    }
        
    return color;
}