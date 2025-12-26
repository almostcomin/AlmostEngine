#pragma once

#include "RHI/Resource.h"
#include "RHI/Shader.h"
#include "RHI/InputLayout.h"
#include "RHI/BlendState.h"
#include "RHI/RasterizerState.h"
#include "RHI/DepthStencilState.h"
#include "RHI/Common.h"

namespace st::rhi
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

} // namespace st::rhi