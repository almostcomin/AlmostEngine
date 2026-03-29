#pragma once

namespace alm::gfx
{
	template<typename Tag>
	struct Handle 
	{
		void* ptr = nullptr;

		Handle() : ptr{ nullptr } {}
		Handle(void* p) : ptr(p) {}
		bool IsValid() const { return ptr != nullptr; }

		bool operator ==(const Handle<Tag>& other) const { return ptr == other.ptr; }
		bool operator <(const Handle<Tag>& other) const { return ptr < other.ptr; }
		operator bool() const { return ptr != nullptr; }
	};

	struct TextureTag {};
	struct BufferTag {};
	struct FramebufferTag {};
	struct TextureViewTicketTag {};
	struct BufferViewTicketTag {};

	using RGTextureHandle = Handle<TextureTag>;
	using RGBufferHandle = Handle<BufferTag>;
	using RGFramebufferHandle = Handle<FramebufferTag>;

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