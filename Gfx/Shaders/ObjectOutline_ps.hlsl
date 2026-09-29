#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

ConstantBuffer<interop::ObjectOutlineConstants> StageConstants : register(b0);

[RootSignature(BindlessRootSignature)]
float4 main() : SV_Target
{
    return StageConstants.Color;
}
