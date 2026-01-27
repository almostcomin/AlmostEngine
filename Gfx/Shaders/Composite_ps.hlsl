#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Color.hlsli"
#include "ACES.hlsli"

// These values must match the st::rhi::ColorSpace enum
#define COLOR_SPACE_SRGB            0
#define COLOR_SPACE_HDR10_ST2084    1
#define COLOR_SPACE_HDR_LINEAR      2

ConstantBuffer<interop::CompositeConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    Texture2D sceneTexture = ResourceDescriptorHeap[Constants.sceneTextureDI];
    float4 sceneColor = sceneTexture.Sample(pointClampSampler, input.uv);
    
    Texture2D uiTexture = ResourceDescriptorHeap[Constants.uiTextureDI];
    float4 uiColor = uiTexture.Sample(pointClampSampler, input.uv);
    
    if (Constants.colorSpace == COLOR_SPACE_SRGB)
    {
/*        
        if (Constants.tonemapping != 0)
        {
            sceneColor.rgb = ACESFilm_HDR(sceneColor.rgb, 1000.0);
        }
*/        
        // assuming back-buffer is SRGB, do not LinearToSRGB
    }
    else if (Constants.colorSpace == COLOR_SPACE_HDR10_ST2084)
    {
/*        
        if (Constants.tonemapping != 0)
        {
            sceneColor.rgb = ACESFilm_HDR(sceneColor.rgb, 1000.0);
        }
*/      
        // https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HDR/src/presentPS.hlsl
        const float referenceWhiteNits = 203.0;
        const float st2084max = 10000.0;
        const float hdrScalar = referenceWhiteNits / st2084max;
	    // The input is in Rec.709, but the display is Rec.2020
        sceneColor.rgb = Rec709ToRec2020(sceneColor.rgb);
        // Apply the ST.2084 curve to the result.
        sceneColor.rgb = LinearToST2084(sceneColor.rgb * hdrScalar);
    }
    else // colorSpace == COLOR_SPACE_HDR_LINEAR
    {
        // Just pass through
    }
    
    float4 color = float4(lerp(sceneColor, uiColor, uiColor.a));
    return color;
}