#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"

[RootSignature(BindlessRootSignature)]
float4 main() : SV_Target
{
    return float4(1.0, 1.0, 1.0, 1.0);
}
