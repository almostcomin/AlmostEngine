#pragma once

#include <array>
#include <new>
#include <memory>
#include <cassert>
#include <optional>

namespace st
{

	template<class T, size_t N>
	class RingBuffer
	{
		static_assert(N > 0, "N must be > 0");

	public:

		using value_type = T;
		using size_type = std::size_t;
		static constexpr size_type kBufferSize = N + 1;

		// iterator (forward)
		template<bool IsConst>
		struct basic_iterator
		{
			using buffer_type = std::conditional_t<IsConst, const RingBuffer, RingBuffer>;
			using value_type = std::conditional_t<IsConst, const T, T>;
			using reference = value_type&;
			using pointer = value_type*;
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;

			basic_iterator(buffer_type* rb, size_type idx) : m_rb(rb), m_idx(idx) {}

			reference operator*() const { return *std::launder(reinterpret_cast<pointer>(std::addressof(m_rb->m_Storage[m_idx]))); }
			pointer   operator->() const { return std::launder(reinterpret_cast<pointer>(std::addressof(m_rb->m_Storage[m_idx]))); }

			basic_iterator& operator++()
			{
				m_idx = (m_idx + 1) % kBufferSize;
				return *this;
			}

			basic_iterator operator++(int)
			{
				basic_iterator tmp = *this;
				++(*this);
				return tmp;
			}

			bool operator==(basic_iterator const& o) const { return m_rb == o.m_rb && m_idx == o.m_idx; }
			bool operator!=(basic_iterator const& o) const { return !(*this == o); }

		private:
			buffer_type* m_rb;
			size_type    m_idx;
		};

		using iterator = basic_iterator<false>;
		using const_iterator = basic_iterator<true>;

		~RingBuffer() { Clear(); }

		bool Push(const T& value)
		{
			if (Full()) return false;
			std::construct_at(GetRawPtr(m_Tail), value);
			AdvanceTail();
			return true;
		}

		bool Push(T&& value)
		{
			if (Full()) return false;
			std::construct_at(GetRawPtr(m_Tail), std::move(value));
			AdvanceTail();
			return true;
		}

		template<typename U = T, typename = std::enable_if_t<std::is_default_constructible_v<U>>>
		bool Push()
		{
			if (Full()) return false;
			std::construct_at(GetRawPtr(m_Tail));
			AdvanceTail();
			return true;
		}

		template<class... Args>
		bool Emplace(Args&&... args)
		{
			if (Full()) return false;
			std::construct_at(GetRawPtr(m_Tail), std::forward<Args>(args)...);
			AdvanceTail();
			return true;
		}

		std::optional<T> Pop()
		{
			if (Empty()) return std::nullopt;
			std::optional<T> opt(std::in_place, std::move(*GetPtr(m_Head)));
			std::destroy_at(GetPtr(m_Head));
			AdvanceHead();
			return opt;
		}

		bool TryPop(T& out)
		{
			if (Empty()) return false;
			out = std::move(*GetPtr(m_Head));
			std::destroy_at(GetPtr(m_Head));
			AdvanceHead();
			return true;
		}

		void Clear() 
		{
			while (!Empty()) 
			{
				std::destroy_at(GetPtr(m_Head));
				AdvanceHead();
			}
		}

		bool Empty() const noexcept { return m_Head == m_Tail; }
		bool Full() const noexcept { return (m_Tail + 1) % kBufferSize == m_Head; }
		size_t Size() const noexcept { return m_Tail >= m_Head  ? m_Tail - m_Head : kBufferSize - m_Head + m_Tail; }
		static constexpr size_t Capacity() noexcept { return N; }

		T& Front() 
		{ 
			assert(!Empty());
			return *GetPtr(m_Head);
		}
		const T& Front() const 
		{
			assert(!Empty());
			return *GetPtr(m_Head);
		}

		T& Back() 
		{ 
			assert(!Empty());
			return *GetPtr((m_Tail + kBufferSize - 1) % kBufferSize);
		}
		const T& Back() const 
		{ 
			assert(!Empty());
			return *GetPtr((m_Tail + kBufferSize - 1) % kBufferSize);
		}

		iterator begin() { return iterator(this, m_Head); }
		iterator end() { return iterator(this, m_Tail); }
		const_iterator begin() const { return const_iterator(this, m_Head); }
		const_iterator end()   const { return const_iterator(this, m_Tail); }
		const_iterator cbegin() const { return const_iterator(this, m_Head); }
		const_iterator cend()   const { return const_iterator(this, m_Tail); }

	private:

		void AdvanceHead() { m_Head = (m_Head + 1) % kBufferSize; }
		void AdvanceTail() { m_Tail = (m_Tail + 1) % kBufferSize; }

		T* GetRawPtr(size_t idx) noexcept
		{
			return reinterpret_cast<T*>(std::addressof(m_Storage[idx]));
		}

		const T* GetRawPtr(size_t idx) const noexcept
		{
			return reinterpret_cast<const T*>(std::addressof(m_Storage[idx]));
		}
		T* GetPtr(size_t idx)
		{
			return std::launder(reinterpret_cast<T*>(std::addressof(m_Storage[idx])));
		}
		const T* GetPtr(size_t idx) const
		{
			return std::launder(reinterpret_cast<const T*>(std::addressof(m_Storage[idx])));
		}

		using StorageType = std::aligned_storage_t<sizeof(T), alignof(T)>;
		std::array<StorageType, kBufferSize> m_Storage;

		size_t m_Head = 0;
		size_t m_Tail = 0;
	};

} // namespace st