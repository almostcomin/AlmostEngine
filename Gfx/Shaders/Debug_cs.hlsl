#include "sh2/s2h.hlsl"
#include "sh2/s2h_3d.hlsl"
#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

#define S2S_FRAMEBUFFERSIZE() sceneConstants.screenResolution.xy
#define S2S_MOUSEPOS() sceneConstants.mouseState.xy
#define S2S_CAMERA_POS() sceneConstants.camWorldPos.xyz
#define S2S_NEAR() sceneConstants.camZNear
#define S2S_INV_VIEW_PROJECTION() sceneConstants.invCamViewProjMatrix

ConstantBuffer<interop::DebugStageS2H> Constants : register(b0);

void printFloat3(inout ContextGather ui, float3 v)
{
    s2h_printFloat(ui, v.x);
    s2h_printTxt(ui, _COMMA, _SPACE);
    s2h_printFloat(ui, v.y);
    s2h_printTxt(ui, _COMMA, _SPACE);
    s2h_printFloat(ui, v.z);
}

void mainImage(out float4 fragColor, in float2 fragCoord, in float4 background, float3 normal, interop::SceneConstants sceneConstants)
{
    Context3D context3d;
    ContextGather ui;
    
    float3 ro = S2S_CAMERA_POS();
    float2 pxPos = float2(fragCoord.x, S2S_FRAMEBUFFERSIZE().y - fragCoord.y);
    float2 uv = pxPos / S2S_FRAMEBUFFERSIZE();
    float3 worldPos;
    {
        float2 screenPos = uv * 2.0 - 1.0;
        screenPos.y = -screenPos.y;
        float4 worldPosHom = mul(S2S_INV_VIEW_PROJECTION(), float4(screenPos, S2S_NEAR(), 1.0));
        worldPos = worldPosHom.xyz / worldPosHom.w;
    }
    
    s2h_init(context3d, ro, normalize(worldPos - ro));    
    
    s2h_init(ui, float2(fragCoord.x, S2S_FRAMEBUFFERSIZE().y - fragCoord.y));
    
    s2h_setCursor(ui, float2(10, 40));
    s2h_setScale(ui, 2.0);        
    ui.textColor = float4(0, 0, 0, 1);
    
    s2h_printTxt(ui, _P, _i, _x, _e, _l);
    s2h_printTxt(ui, _COLON, _SPACE);
    s2h_printFloat(ui, S2S_MOUSEPOS().x);
    s2h_printTxt(ui, _COMMA, _SPACE);
    s2h_printFloat(ui, S2S_MOUSEPOS().y);
    
    s2h_printLF(ui);
    
    s2h_printTxt(ui, _U, _V, _COLON, _SPACE);
    s2h_printFloat(ui, S2S_MOUSEPOS().x / S2S_FRAMEBUFFERSIZE().x);
    s2h_printTxt(ui, _COMMA, _SPACE);
    s2h_printFloat(ui, S2S_MOUSEPOS().y / S2S_FRAMEBUFFERSIZE().y);
        
    s2h_printLF(ui);    
    s2h_printLF(ui);
    
    s2h_printTxt(ui, _N, _o, _r, _m, _a);
    s2h_printTxt(ui, _l, _COLON, _SPACE);
    s2h_printFloat(ui, normal.x);
    s2h_printTxt(ui, _COMMA, _SPACE);
    s2h_printFloat(ui, normal.y);
    s2h_printTxt(ui, _COMMA, _SPACE);
    s2h_printFloat(ui, normal.z);
    
    s2h_drawArrowWS(context3d, float3(0, 0, 0), float3(0, 1, 0), float4(1, 0, 0, 1), 0.1);

    fragColor = background;
    fragColor = lerp(fragColor, float4(context3d.dstColor.rgb, 1), context3d.dstColor.a);
    fragColor = fragColor * (1.0 - ui.dstColor.a) + ui.dstColor;
    
    //fragColor = background * (1.0 - ui.dstColor.a) + ui.dstColor;
/*    
    ContextGather ui;
    s2h_init(ui, float2(fragCoord.x, S2S_FRAMEBUFFERSIZE().y - fragCoord.y));
    s2h_setCursor(ui, float2(10, 10));
    s2h_printLF(ui);
    s2h_setScale(ui, 8.0);
    s2h_printSpace(ui, 2.4);
    ui.textColor.rgb = float3(1, 0, 0);
    s2h_printTxt(ui, _S);
    ui.textColor.rgb = float3(0, 1, 0);
    s2h_printTxt(ui, _2);
    ui.textColor.rgb = float3(0, 0, 1);
    s2h_printTxt(ui, _H);
    s2h_printLF(ui);
    s2h_setScale(ui, 2.0);
    s2h_printSpace(ui, 8.0);
    ui.textColor.rgb = float3(0.5, 0.5, 0.5);
    s2h_printTxt(ui, _S, _2, _H, _UNDERSCORE);
    s2h_printTxt(ui, _V, _E, _R, _S);
    s2h_printTxt(ui, _I, _O, _N, _COLON);
    // We use "_S2H_VERSION" which is "static const uint",
    // not "S2H_VERSION" as that is only supported on platforms that have
    // a preprocessor (define support).
    s2h_printInt(ui, _S2H_VERSION);
    float3 linearc = backgroundLinear.rgb * (1.0f - ui.dstColor.a) + ui.dstColor.rgb;
    //fragColor = float4(s2h_accurateLinearToSRGB(linearc), 1.0f);
    fragColor = float4(linearc, 1.0f);
*/
}

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    RWTexture2D<float4> inputTex = ResourceDescriptorHeap[Constants.inputTextureDI];
    RWTexture2D<float4> outputTex = ResourceDescriptorHeap[Constants.outputTextureDI];
    ConstantBuffer<interop::SceneConstants> sceneConstants = ResourceDescriptorHeap[Constants.sceneDI];
    
    float2 dimensions = S2S_FRAMEBUFFERSIZE();
    float4 bg = inputTex[DTid];
    
    float3 normal = float3(0.0, 0.0, 0.0);
    if (Constants.GBuffer2DI != INVALID_DESCRIPTOR_INDEX)
    {
        Texture2D GBuffer2 = ResourceDescriptorHeap[Constants.GBuffer2DI];
        float2 uv = S2S_MOUSEPOS() / S2S_FRAMEBUFFERSIZE();
        float2 encodedNormal = GBuffer2.SampleLevel(pointClampSampler, uv, 0).xy;
        normal = DecodeNormal(encodedNormal);
        //normal = mul((float3x3)sceneConstants.invCamViewMatrix, normal);
    }
    
    float4 fragColor = float4(0, 0, 0, 0);
    mainImage(fragColor, float2(DTid.x + 0.5, dimensions.y - float(DTid.y) - 0.5), bg, normal, sceneConstants);
    
    outputTex[DTid] = fragColor;
}
