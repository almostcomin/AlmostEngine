#include "Interop/ImGUI_CB.h"
#include "BindlessRS.hlsli"

using ImGuiTexFlags = uint;
static const ImGuiTexFlags ImGuiTexFlags_None               = 0;
static const ImGuiTexFlags ImGuiTexFlags_IgnoreAlpha        = 1 << 0;
static const ImGuiTexFlags ImGuiTexFlags_HideRedChannel     = 1 << 1;
static const ImGuiTexFlags ImGuiTexFlags_HideGreenChannel   = 1 << 2;
static const ImGuiTexFlags ImGuiTexFlags_HideBlueChannel    = 1 << 3;
static const ImGuiTexFlags ImGuiTexFlags_ShowAlphaChannel   = 1 << 4;

ConstantBuffer<interop::ImGUI_CB> CB : register(b0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    if (CB.textureIndex == INVALID_DESCRIPTOR_INDEX)
        return float4(0.0, 0.0, 0.0, 1.0);
        
    Texture2D texture = ResourceDescriptorHeap[CB.textureIndex];
    
    float4 out_col = input.col * texture.Sample(pointWrapSampler, input.uv);
    
    if (CB.flags & ImGuiTexFlags_ShowAlphaChannel)
        out_col.rgb = out_col.aaa;
    
    if (CB.flags & ImGuiTexFlags_IgnoreAlpha)
        out_col.a = 1.0;
    
    if (CB.flags & ImGuiTexFlags_HideRedChannel)
        out_col.r = 0.0;
    
    if (CB.flags & ImGuiTexFlags_HideGreenChannel)
        out_col.g = 0.0;
    
    if (CB.flags & ImGuiTexFlags_HideBlueChannel)
        out_col.b = 0.0;

    //out_col = 1.0;
    return out_col;
}
