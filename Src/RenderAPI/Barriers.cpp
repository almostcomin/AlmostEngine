#include "RenderAPI/Barriers.h"

st::rapi::Barrier st::rapi::Barrier::Memory(st::rapi::IResource* resource)
{
	Barrier barrier;
	barrier.type = Type::MEMORY;
	barrier.memory.resource = resource;
	return barrier;
}

st::rapi::Barrier st::rapi::Barrier::Buffer(IBuffer* buffer, ResourceState before, ResourceState after)
{
	Barrier barrier;
	barrier.type = Type::BUFFER;
	barrier.buffer.buffer = buffer;
	barrier.buffer.state_before = before;
	barrier.buffer.state_after = after;
	return barrier;
}

st::rapi::Barrier st::rapi::Barrier::Texture(ITexture* texture, ResourceState before, ResourceState after, int mip, int slice, ImageAspect aspect)
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

st::rapi::Barrier st::rapi::Barrier::Aliasing(IResource* before, IResource* after)
{
	Barrier barrier;
	barrier.aliasing.resource_before = before;
	barrier.aliasing.resource_after = after;
	return barrier;
}