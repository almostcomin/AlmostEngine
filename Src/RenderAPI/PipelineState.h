#pragma once

#include "RenderAPI/Resource.h"
#include "RenderAPI/Shader.h"
#include "RenderAPI/InputLayout.h"
#include "RenderAPI/BlendState.h"
#include "RenderAPI/RasterizerState.h"
#include "RenderAPI/DepthStencilState.h"

namespace st::rapi
{

enum class PrimitiveTopology
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
    TriangleListWithAdjacency,
    TriangleStripWithAdjacency,
    PatchList
};

struct GraphicsPipelineStateDesc
{
    ShaderHandle VS;
    ShaderHandle HS;
    ShaderHandle DS;
    ShaderHandle GS;
    ShaderHandle PS;

    InputLayout inputLayout;
    BlendState blendState;
    DepthStencilState depthStencilState;
    RasterizerState rasterState;

    PrimitiveTopology primTopo = PrimitiveTopology::TriangleList;
    uint32_t patchControlPoints = 0;
};

class IGraphicsPipelineState : public IResource
{
public:

    virtual const GraphicsPipelineStateDesc& GetDesc() const = 0;
};

using GraphicsPipelineStateHandle = std::shared_ptr<IGraphicsPipelineState>;

} // namespace st::rapi