#include "sh2/s2h.hlsl"
#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::HeightmapDebugConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    RWTexture2D<float4> inputTex = ResourceDescriptorHeap[Constants.inputTextureDI];
    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[Constants.outputTextureDI];
    
    float4 bgColor = inputTex[DTid];

    ContextGather ui;
    s2h_init(ui, float2(DTid.x, DTid.y));
    
    ui.textColor = float4(0, 0, 0, 1);
    s2h_setScale(ui, 1.0);
    
    s2h_setCursor(ui, Constants.screenPos);
    
    s2h_printTxt(ui, _C, _o, _o, _r, _d);
    s2h_printTxt(ui, _s, _COLON, _SPACE);
    s2h_printInt(ui, Constants.coords.x);
    s2h_printTxt(ui, _COMMA, _SPACE);
    s2h_printInt(ui, Constants.coords.y);
    
    s2h_printLF(ui);
    s2h_printTxt(ui, _L, _e, _v, _e, _l);
    s2h_printTxt(ui, _COLON, _SPACE);
    s2h_printInt(ui, Constants.level);
        
    float4 fragColor = bgColor * (1.0 - ui.dstColor.a) + ui.dstColor;    
    outputTex[DTid] = fragColor;
}
