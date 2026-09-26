#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

struct PS_INPUT
{
    float4 pos : SV_Position;
    float3 worldPos : WORLDPOS;
    nointerpolation uint colorIdx : COLORIDX;
};

ConstantBuffer<interop::GridStageConstants> Constants : register(b0);

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::SceneConstants> sceneData = ResourceDescriptorHeap[Constants.sceneDI];

    float3 color;
    float baseAlpha;
    switch (input.colorIdx)
    {
    case 2: // X axis
        color = float3(0.0f, 0.0f, 0.0f);
        baseAlpha = 1.0f;
        break;
    case 3: // Z axis
        color = float3(0.0f, 0.0f, 0.0f);
        baseAlpha = 1.0f;
        break;
    case 1: // major
        color = float3(0.60f, 0.60f, 0.60f);
        baseAlpha = 0.85f;
        break;
    default: // minor
        color = float3(0.55f, 0.55f, 0.55f);
        baseAlpha = 0.40f;
        break;
    }

    float dist = distance(input.worldPos, sceneData.camWorldPos);
    float fade = 1.0f - smoothstep(Constants.fadeStartDist, Constants.fadeEndDist, dist);

    return float4(color, baseAlpha * fade);
}
