#pragma once

#include "Core/Memory.h"

namespace alm::rhi
{

class IBuffer;
class ICommandList;
class IFence;
class ITexture;
class IShader;
class IFramebuffer;
class IGraphicsPipelineState;
class IComputePipelineState;
class ITimerQuery;

using BufferOwner = alm::unique<IBuffer>;
using BufferHandle = alm::weak<IBuffer>;

using CommandListOwner = alm::unique<ICommandList>;
using CommandListHandle = alm::weak<ICommandList>;

using FenceOwner = alm::unique<IFence>;
using FenceHandle = alm::weak<IFence>;

using TextureOwner = alm::unique<ITexture>;
using TextureHandle = alm::weak<ITexture>;

using ShaderOwner = alm::unique<IShader>;
using ShaderHandle = alm::weak<IShader>;

using FramebufferOwner = alm::unique<IFramebuffer>;
using FramebufferHandle = alm::weak<IFramebuffer>;

using GraphicsPipelineStateOwner = alm::unique<IGraphicsPipelineState>;
using GraphicsPipelineStateHandle = alm::weak<IGraphicsPipelineState>;

using ComputePipelineStateOwner = alm::unique<IComputePipelineState>;
using ComputePipelineStateHandle = alm::weak<IComputePipelineState>;

using TimerQueryOwner = alm::unique<ITimerQuery>;
using TimerQueryHandle = alm::weak<ITimerQuery>;

} // namespace st::rhi