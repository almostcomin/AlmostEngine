#pragma once

#include "RHI/ShaderViews.h"

namespace st::gfx
{
class MeshInstance;

void RenderSet(const std::vector<const st::gfx::MeshInstance*>& instancesSet, st::rhi::BufferUniformView sceneBuffer,
	st::rhi::ICommandList* commandList);

void RenderSetInstanced(const std::vector<const st::gfx::MeshInstance*>& instancesSet, st::rhi::BufferUniformView sceneBuffer,
	st::rhi::BufferReadOnlyView instancesIndexBuffer, st::rhi::ICommandList* commandList);

} //namespace st::gfx