#pragma once

#include "RenderAPI/ResourceState.h"

namespace st::rapi
{
	class IResource;
	class ITexture;
	class IBuffer;

	enum class ImageAspect : uint8_t
	{
		Undef,
		Color,
		Depth,
		Stencil
	};

	struct Barrier
	{
		enum class Type
		{
			MEMORY,		// UAV accesses
			BUFFER,		// buffer state transition
			TEXTURE,	// texture layout transition
			ALIASING,	// memory aliasing transition
		};
		struct Memory
		{
			IResource* resource;
		};
		struct Texture
		{
			ITexture* texture;
			ResourceState layout_before;
			ResourceState layout_after;
			int mip;
			int slice;
			ImageAspect aspect;
		};
		struct Buffer
		{
			IBuffer* buffer;
			ResourceState state_before;
			ResourceState state_after;
		};
		struct Aliasing
		{
			IResource* resource_before;
			IResource* resource_after;
		};

		Type type;
		union
		{
			Memory memory;
			Texture texture;
			Buffer buffer;
			Aliasing aliasing;
		};

		static Barrier Memory(IResource* resource = nullptr);
		static Barrier Buffer(IBuffer* buffer, ResourceState before, ResourceState after);
		static Barrier Texture(ITexture* texture, ResourceState before, ResourceState after, int mip = -1, int slice = -1, ImageAspect aspect = ImageAspect::Undef);
		static Barrier Aliasing(IResource* before, IResource* after);

	};

} // namespace st::rapi