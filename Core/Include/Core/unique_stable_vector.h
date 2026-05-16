#pragma once

#include <array>

// TODO:
// m_lookup keeps a copy of the key. Fine for small T (pointers, handles). If T becomes large,
// switch to a pointer-keyed map with transparent hashing over m_storage.
// https://trello.com/c/yFfs4ElR/4-usat-t-en-lugar-de-t-como-key-en-el-lookup-del-uniquestablevector

namespace alm
{
	template <typename T, size_t MaxElements, typename Hash = std::hash<T>>
	struct unique_stable_vector
	{
		static constexpr size_t max_elements = MaxElements;

		template<bool Const>
		struct Iterator
		{
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using value_type = std::conditional_t<Const, const T, T>;
			using pointer = value_type*;
			using reference = value_type&;
			using storage_pointer = std::conditional_t<Const, const std::byte*, std::byte*>;

			Iterator(storage_pointer storage, const uint8_t* occupied, size_t pos, size_t end)
				: m_storage(storage), m_occupied(occupied), m_pos(pos), m_end(end)
			{
				skipToNextValid();
			}

			reference operator*() const
			{
				assert(valid());
				return *std::launder(
					reinterpret_cast<pointer>(&m_storage[m_pos * sizeof(T)]));
			}

			pointer operator->() const
			{
				assert(valid());
				return std::launder(
					reinterpret_cast<pointer>(&m_storage[m_pos * sizeof(T)]));
			}

			Iterator& operator++()
			{
				++m_pos;
				skipToNextValid();
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator tmp = *this;
				++(*this);
				return tmp;
			}

			friend bool operator==(const Iterator& a, const Iterator& b)
			{
				return a.m_pos == b.m_pos && a.m_storage == b.m_storage;
			}

			friend bool operator!=(const Iterator& a, const Iterator& b)
			{
				return a.m_pos != b.m_pos || a.m_storage != b.m_storage;
			}

			bool valid() const noexcept
			{
				return m_occupied[m_pos];
			}

			size_t get_index() const noexcept
			{
				return m_pos;
			}

		private:
			void skipToNextValid() 
			{
				while (m_pos < m_end && !m_occupied[m_pos])
					++m_pos;
			}

			storage_pointer m_storage;
			const uint8_t* m_occupied;
			size_t m_pos;
			size_t m_end;
		};

		template<bool Const>
		struct AllIterator
		{
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using value_type = std::conditional_t<Const, const T, T>;
			using pointer = value_type*;
			using reference = value_type&;
			using storage_pointer = std::conditional_t<Const, const std::byte*, std::byte*>;

			AllIterator(storage_pointer storage, const uint8_t* occupied, size_t pos, size_t end)
				: m_storage(storage), m_occupied(occupied), m_pos(pos), m_end(end) {
			}

			reference operator*() const
			{
				assert(valid());
				return *std::launder(
					reinterpret_cast<pointer>(&m_storage[m_pos * sizeof(T)]));
			}

			pointer operator->() const
			{
				return std::launder(
					reinterpret_cast<pointer>(&m_storage[m_pos * sizeof(T)]));
			}

			AllIterator& operator++() { ++m_pos; return *this; }
			AllIterator operator++(int) { AllIterator tmp = *this; ++(*this); return tmp; }

			friend bool operator==(const AllIterator& a, const AllIterator& b)
			{
				return a.m_pos == b.m_pos && a.m_storage == b.m_storage;
			}
			friend bool operator!=(const AllIterator& a, const AllIterator& b)
			{
				return a.m_pos != b.m_pos || a.m_storage != b.m_storage;
			}

			bool valid() const noexcept
			{
				return m_occupied[m_pos];
			}

			size_t get_index() const noexcept
			{
				return m_pos;
			}

		private:
			storage_pointer m_storage;
			const uint8_t* m_occupied;
			size_t m_pos;
			size_t m_end;
		};

		using iterator = Iterator<false>;
		using const_iterator = Iterator<true>;
		using all_iterator = AllIterator<false>;
		using const_all_iterator = AllIterator<true>;

		unique_stable_vector() = default;

		unique_stable_vector(const unique_stable_vector& other)
		{
			for (size_t i = 0; i < other.m_end; ++i)
			{
				if (other.m_occupied[i])
				{
					construct_at(i, *other.ptr(i));
					m_lookup.emplace(*ptr(i), i);
				}
			}
			m_firstFree = other.m_firstFree;
			m_end = other.m_end;
			m_size = other.m_size;
		}

		unique_stable_vector(unique_stable_vector&& other)
		{
			for (size_t i = 0; i < other.m_end; ++i)
			{
				if (other.m_occupied[i])
				{
					construct_at(i, std::move(*other.ptr(i)));
					m_lookup.emplace(*ptr(i), i);
				}
			}
			m_firstFree = other.m_firstFree;
			m_end = other.m_end;
			m_size = other.m_size;

			other.clear();
		}

		~unique_stable_vector()
		{
			clear();
		}

		unique_stable_vector& operator=(const unique_stable_vector& other)
		{
			if (this == &other)
				return *this;

			clear();

			for (size_t i = 0; i < other.m_end; ++i)
			{
				if (other.m_occupied[i])
				{
					construct_at(i, *other.ptr(i));
					m_lookup.emplace(*ptr(i), i);
				}
			}
			m_firstFree = other.m_firstFree;
			m_end = other.m_end;
			m_size = other.m_size;

			return *this;
		}

		unique_stable_vector& operator=(unique_stable_vector&& other)
		{
			if (this == &other)
				return *this;

			clear();

			for (size_t i = 0; i < other.m_end; ++i)
			{
				if (other.m_occupied[i])
				{
					construct_at(i, std::move(*other.ptr(i)));
					m_lookup.emplace(*ptr(i), i);
				}
			}
			m_firstFree = other.m_firstFree;
			m_end = other.m_end;
			m_size = other.m_size;

			other.clear();

			return *this;
		}

		iterator begin() 
		{
			return iterator(m_storage, m_occupied.data(), 0, m_end);
		}

		iterator end()
		{
			return iterator(m_storage, m_occupied.data(), m_end, m_end);
		}

		const_iterator begin() const
		{
			return const_iterator(m_storage, m_occupied.data(), 0, m_end);
		}

		const_iterator end() const
		{
			return const_iterator(m_storage, m_occupied.data(), m_end, m_end);
		}

		const_iterator cbegin() const { return begin(); }
		const_iterator cend() const { return end(); }

		all_iterator begin_all()
		{
			return all_iterator(m_storage, m_occupied.data(), 0, m_end);
		}

		all_iterator end_all()
		{
			return all_iterator(m_storage, m_occupied.data(), m_end, m_end);
		}

		const_all_iterator begin_all() const
		{
			return const_all_iterator(m_storage, m_occupied.data(), 0, m_end);
		}

		const_all_iterator end_all() const
		{
			return const_all_iterator(m_storage, m_occupied.data(), m_end, m_end);
		}

		const_all_iterator cbegin_all() const { return begin_all(); }
		const_all_iterator cend_all() const { return end_all(); }

		// Insert value, returns [index, blue=inserted, false=not_inserted]
		std::pair<size_t, bool> insert(const T& value)
		{
			assert(m_size < max_elements);

			auto it = m_lookup.find(value);
			if (it != m_lookup.end())
				return { it->second, false };

			const size_t slot = m_firstFree;
			construct_at(slot, value);
			m_end = std::max(m_end, slot + 1);
			++m_size;

			m_lookup.emplace(*ptr(slot), slot);
			findNextFree();

			return { slot, true };
		}

		std::pair<size_t, bool> insert(T&& value)
		{
			assert(m_size < max_elements);

			auto it = m_lookup.find(value);
			if (it != m_lookup.end())
				return { it->second, false };

			const size_t slot = m_firstFree;
			construct_at(slot, std::move(value));
			m_end = std::max(m_end, slot + 1);
			++m_size;

			m_lookup.emplace(*ptr(slot), slot);
			findNextFree();

			return { slot, true };
		}

		void erase(size_t index)
		{
			assert(index < max_elements);
			assert(m_occupied[index]);

			m_lookup.erase(*ptr(index));
			destroy_at(index);
			--m_size;
			m_firstFree = std::min(m_firstFree, index);
			findNewEnd();
		}

		bool erase(const T& value)
		{
			auto it = m_lookup.find(value);
			if (it == m_lookup.end())
				return false;

			destroy_at(it->second);
			--m_size;
			m_firstFree = std::min(m_firstFree, it->second);

			m_lookup.erase(it);
			findNewEnd();

			return true;
		}

		void clear() 
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				for (size_t i = 0; i < m_end; ++i)
				{
					if (m_occupied[i])
						std::destroy_at(ptr(i));
				}
			}
			m_lookup.clear();
			m_occupied = {};
			m_firstFree = 0;
			m_end = 0;
			m_size = 0;
		}

		T* get(size_t index) noexcept
		{
			return (index < max_elements && m_occupied[index]) ?
				ptr(index) : nullptr;
		}

		const T* get(size_t index) const noexcept
		{
			return (index < max_elements && m_occupied[index]) ?
				ptr(index) : nullptr;
		}

		T& operator[](size_t index) noexcept
		{
			assert(m_occupied[index]);
			return *ptr(index);
		}

		const T& operator[](size_t index) const noexcept
		{
			assert(m_occupied[index]);
			return *ptr(index);
		}

		size_t find(const T& value) const noexcept
		{
			auto it = m_lookup.find(value);
			return (it != m_lookup.end()) ? it->second : max_elements;
		}

		bool contains(const T& value) const noexcept 
		{
			return m_lookup.find(value) != m_lookup.end();
		}

		bool valid_index(size_t index) const noexcept
		{
			return (index < max_elements && m_occupied[index]);
		}

		size_t size() const noexcept { return m_size; }
		size_t storage_size() const noexcept { return m_end; }
		size_t capacity() const noexcept { return max_elements; }
		bool empty() const noexcept { return m_size == 0; }
		bool full() const noexcept { return m_size >= max_elements; }

	private:

		T* ptr(size_t i)
		{
			return std::launder(
				reinterpret_cast<T*>(&m_storage[i * sizeof(T)])
			);
		}

		const T* ptr(size_t i) const
		{
			return std::launder(
				reinterpret_cast<const T*>(&m_storage[i * sizeof(T)])
			);
		}

		template<typename U>
		void construct_at(size_t i, U&& value)
		{
			assert(!m_occupied[i]);
			new (&m_storage[i * sizeof(T)]) T(std::forward<U>(value));
			m_occupied[i] = true;
		}

		void destroy_at(size_t i)
		{
			assert(m_occupied[i]);
			std::destroy_at(ptr(i));
			m_occupied[i] = false;
		}

		void findNextFree()
		{
			if (m_size >= max_elements) {
				m_firstFree = max_elements;
				return;
			}

			auto nextFree = m_firstFree + 1;
			while (nextFree < max_elements && m_occupied[nextFree])
				++nextFree;

			m_firstFree = nextFree;
		}

		void findNewEnd()
		{
			if (m_size == 0)
			{
				m_end = 0;
				return;
			}

			while (m_end > 0 && !m_occupied[m_end - 1])
				--m_end;
		}

		alignas(T) std::byte m_storage[max_elements * sizeof(T)];
		std::array<uint8_t, max_elements> m_occupied = {};
		std::unordered_map<T, size_t, Hash> m_lookup;
		size_t m_firstFree = 0;
		size_t m_end = 0;
		size_t m_size = 0;
	};
}