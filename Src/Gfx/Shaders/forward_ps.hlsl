
#pragma pack_matrix(row_major)

#include "forward_cb.h"
#include "forward_vertex.hlsli"
#include "material_bindings.hlsli"

void main(
    in float4 i_position : SV_Position,
    in SceneVertex i_vtx,
    out float4 o_color : SV_Target0
)
{
    float4 diffuseTerm = SampleDiffuse(i_vtx.texCoord);

    o_color.rgb = diffuseTerm.rgb;
    o_color.a = 1;
}
