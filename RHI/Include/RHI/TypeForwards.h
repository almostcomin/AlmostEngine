#pragma once

#include "Core/Memory.h"

namespace st::rhi
{

class IBuffer;
class ICommandList;
class IFence;
class ITexture;

using BufferHandle = st::weak<IBuffer>;
using CommandListHandle = st::weak<ICommandList>;
using FenceHandle = st::weak<IFence>;
using TextureHandle = st::weak<ITexture>;

};