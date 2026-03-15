#pragma once

#include "RHI/ShaderViews.h"
#include "RHI/RasterizerState.h"
#include "Gfx/RenderContext.h"

namespace st::gfx
{
	class MeshInstance;
	struct RenderSet;
}

namespace st::gfx
{

void DrawElements(const std::vector<const st::gfx::MeshInstance*>& instancesSet, st::rhi::BufferUniformView sceneBuffer,
	st::rhi::ICommandList* commandList);

// renderSet, list of instances to render
// renderSetIndicesBuffer, view of the buffer with the index of the instances to render. Must match renderSet.
// sceneBuffer, view of buffer with scene data
// renderContext, contains the PSOs needed to render each set of elements in renderSet
// commandList, commanList to use
void DrawRenderSetInstanced(const RenderSet& renderSet, st::rhi::BufferReadOnlyView renderSetIndicesBuffer,
	st::rhi::BufferUniformView sceneBuffer, const RenderContext& renderContext, st::rhi::ICommandList* commandList);

} //namespace st::gfx