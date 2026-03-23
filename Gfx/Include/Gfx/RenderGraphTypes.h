#pragma once

namespace alm::gfx
{
	template<typename Tag>
	struct Handle 
	{
		void* ptr = nullptr;
		bool IsValid() const { return ptr != nullptr; }

		bool operator ==(const Handle<Tag>& other) const { return ptr == other.ptr; }
		bool operator <(const Handle<Tag>& other) const { return ptr < other.ptr; }
	};

	struct TextureTag {};
	struct BufferTag {};
	struct TextureViewTicketTag {};
	struct BufferViewTicketTag {};

	using RGTextureHandle = Handle<TextureTag>;
	using RGBufferHandle = Handle<BufferTag>;

	using RGTextureViewTicket = Handle<TextureViewTicketTag>;
	using RGBufferViewTicket = Handle<BufferViewTicketTag>;
} // namespace st::gfx

namespace std 
{
	// To be able to use Handles in std maps & sets
	template<typename Tag>
	struct hash<alm::gfx::Handle<Tag>> 
	{
		size_t operator()(const alm::gfx::Handle<Tag>& h) const 
		{
			return std::hash<void*>{}(h.ptr);
		}
	};
} // namespace std