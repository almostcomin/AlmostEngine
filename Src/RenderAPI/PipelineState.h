#pragma once

#include "RenderAPI/Resource.h"
#include "RenderAPI/Shader.h"
#include "RenderAPI/InputLayout.h"
#include "RenderAPI/BlendState.h"
#include "RenderAPI/RasterizerState.h"
#include "RenderAPI/DepthStencilState.h"
#include "RenderAPI/Common.h"

namespace st::rapi
{

struct GraphicsPipelineStateDesc
{
    ShaderHandle VS;
    ShaderHandle HS;
    ShaderHandle DS;
    ShaderHandle GS;
    ShaderHandle PS;

    BlendState blendState;
    DepthStencilState depthStencilState;
    RasterizerState rasterState;

    PrimitiveTopology primTopo = PrimitiveTopology::TriangleList;
    uint32_t patchControlPoints = 0;

    std::string debugName = "{null}";
};

class IGraphicsPipelineState : public IResource
{
public:

    virtual const GraphicsPipelineStateDesc& GetDesc() const = 0;
};

using GraphicsPipelineStateHandle = st::weak<IGraphicsPipelineState>;

} // namespace st::rapi