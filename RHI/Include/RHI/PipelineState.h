#pragma once

#include "RHI/Resource.h"
#include "RHI/Shader.h"
#include "RHI/InputLayout.h"
#include "RHI/BlendState.h"
#include "RHI/RasterizerState.h"
#include "RHI/DepthStencilState.h"
#include "RHI/TypeForwards.h"

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
};

struct ComputePipelineStateDesc
{
    ShaderHandle CS;
};

class IGraphicsPipelineState : public IResource
{
public:
    virtual const GraphicsPipelineStateDesc& GetDesc() const = 0;
protected:
    IGraphicsPipelineState(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

class IComputePipelineState : public IResource
{
protected:
    IComputePipelineState(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

} // namespace st::rhi