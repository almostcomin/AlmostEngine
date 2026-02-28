#pragma once

#include "RHI/ShaderViews.h"
#include "RHI/RasterizerState.h"
#include "Gfx/RenderContext.h"

namespace st::gfx
{
	class MeshInstance;
}

namespace st::gfx
{

void RenderSet(const std::vector<const st::gfx::MeshInstance*>& instancesSet, st::rhi::BufferUniformView sceneBuffer,
	st::rhi::ICommandList* commandList);

// renderSet, list of instances to render
// sceneBuffer, view of buffer with scene data
// renderSetIndecesBuffer, view of a buffer with the index of the instances to render. Mush match instancesSet
// commandList, commanList to use
void RenderSetInstanced(const std::vector<std::pair<rhi::CullMode, std::vector<const st::gfx::MeshInstance*>>>& renderSet,
	st::rhi::BufferUniformView sceneBuffer, st::rhi::BufferReadOnlyView renderSetIndicesBuffer, const RenderContext& renderContext,
	st::rhi::ICommandList* commandList);

} //namespace st::gfx