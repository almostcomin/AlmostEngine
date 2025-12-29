#pragma once

#include "Core/Memory.h"

namespace st::rhi
{

class IBuffer;
class ICommandList;
class IFence;
class ITexture;
class IShader;
class IFramebuffer;
class IGraphicsPipelineState;


using BufferOwner = st::unique<IBuffer>;
using BufferHandle = st::weak<IBuffer>;

using CommandListOwner = st::unique<ICommandList>;
using CommandListHandle = st::weak<ICommandList>;

using FenceOwner = st::unique<IFence>;
using FenceHandle = st::weak<IFence>;

using TextureOwner = st::unique<ITexture>;
using TextureHandle = st::weak<ITexture>;

using ShaderOwner = st::unique<IShader>;
using ShaderHandle = st::weak<IShader>;

using FramebufferOwner = st::unique<IFramebuffer>;
using FramebufferHandle = st::weak<IFramebuffer>;

using GraphicsPipelineStateOwner = st::unique<IGraphicsPipelineState>;
using GraphicsPipelineStateHandle = st::weak<IGraphicsPipelineState>;

} // namespace st::rhi