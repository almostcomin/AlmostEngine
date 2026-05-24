#include "RHI/RHI_PCH.h"
#include "RHI/Barriers.h"

alm::rhi::Barrier alm::rhi::Barrier::Memory(alm::rhi::IResource* resource)
{
	Barrier barrier;
	barrier.type = Type::MEMORY;
	barrier.memory.resource = resource;
	return barrier;
}

alm::rhi::Barrier alm::rhi::Barrier::Buffer(IBuffer* buffer, ResourceState before, ResourceState after)
{
	Barrier barrier;
	barrier.type = Type::BUFFER;
	barrier.buffer.buffer = buffer;
	barrier.buffer.state_before = before;
	barrier.buffer.state_after = after;
	return barrier;
}

alm::rhi::Barrier alm::rhi::Barrier::Texture(ITexture* texture, ResourceState before, ResourceState after, int mip, int slice, ImageAspect aspect)
{
	Barrier barrier;
	barrier.type = Type::TEXTURE;
	barrier.texture.texture = texture;
	barrier.texture.layout_before = before;
	barrier.texture.layout_after = after;
	barrier.texture.mip = mip;
	barrier.texture.slice = slice;
	barrier.texture.aspect = aspect;
	return barrier;
}

alm::rhi::Barrier alm::rhi::Barrier::Aliasing(IResource* before, IResource* after)
{
	Barrier barrier;
	barrier.type = Type::ALIASING;
	barrier.aliasing.resource_before = before;
	barrier.aliasing.resource_after = after;
	return barrier;
}

void alm::rhi::Barrier::Inverse()
{
	switch (type)
	{
	case Type::BUFFER:
		std::swap(buffer.state_before, buffer.state_after);
		break;
	case Type::TEXTURE:
		std::swap(texture.layout_before, texture.layout_after);
		break;
	case Type::MEMORY:
	case Type::ALIASING:
	default:
		// no-op
		break;
	};
}