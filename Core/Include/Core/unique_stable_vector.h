#pragma once

#include <array>

namespace alm
{
	template <typename T, size_t MaxElements, typename Hash = std::hash<T>>
	struct unique_stable_vector
	{
		static constexpr size_t max_elements = MaxElements;
		using index_type = size_t;

		template<bool Const>
		struct Iterator
		{
			using iterator_category = std::forward_iterator_tag;
			using difference_type = ptrdiff_t;
			using value_type = std::conditional_t<Const, const T, T>;
			using pointer = value_type*;
			using reference = value_type&;

			Iterator(pointer slots, const uint8_t* occupied, size_t pos, size_t end)
				: m_slots(slots), m_occupied(occupied), m_pos(pos), m_end(end)
			{
				skipToNextValid();
			}

			reference operator*() const
			{
				return m_slots[m_pos];
			}

			pointer operator->() const
			{
				return &m_slots[m_pos];
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
				return a.m_pos == b.m_pos;
			}

			friend bool operator!=(const Iterator& a, const Iterator& b)
			{
				return a.m_pos != b.m_pos;
			}

		private:
			void skipToNextValid() 
			{
				while (m_pos < m_end && !m_occupied[m_pos])
					++m_pos;
			}

			pointer m_slots;
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

			AllIterator(pointer slots, const uint8_t* occupied, size_t pos, size_t end)
				: m_slots(slots), m_occupied(occupied), m_pos(pos), m_end(end) {
			}

			reference operator*() const { return m_slots[m_pos]; }
			pointer operator->() const { return &m_slots[m_pos]; }

			AllIterator& operator++() { ++m_pos; return *this; }
			AllIterator operator++(int) { AllIterator tmp = *this; ++(*this); return tmp; }

			friend bool operator==(const AllIterator& a, const AllIterator& b) { return a.m_pos == b.m_pos; }
			friend bool operator!=(const AllIterator& a, const AllIterator& b) { return a.m_pos != b.m_pos; }

			bool valid_index() const noexcept
			{
				return m_occupied[m_pos];
			}

			size_t get_index() const noexcept
			{
				return m_pos;
			}

		private:
			pointer m_slots;
			const uint8_t* m_occupied;
			size_t m_pos;
			size_t m_end;
		};

		using iterator = Iterator<false>;
		using const_iterator = Iterator<true>;
		using all_iterator = AllIterator<false>;
		using const_all_iterator = AllIterator<true>;

		iterator begin() 
		{
			return iterator(m_slots.data(), m_occupied.data(), 0, m_end);
		}

		iterator end()
		{
			return iterator(m_slots.data(), m_occupied.data(), m_end, m_end);
		}

		const_iterator begin() const
		{
			return const_iterator(m_slots.data(), m_occupied.data(), 0, m_end);
		}

		const_iterator end() const
		{
			return const_iterator(m_slots.data(), m_occupied.data(), m_end, m_end);
		}

		const_iterator cbegin() const { return begin(); }
		const_iterator cend() const { return end(); }

		all_iterator begin_all()
		{
			return all_iterator(m_slots.data(), m_occupied.data(), 0, m_end);
		}

		all_iterator end_all()
		{
			return all_iterator(m_slots.data(), m_occupied.data(), m_end, m_end);
		}

		const_all_iterator begin_all() const
		{
			return const_all_iterator(m_slots.data(), m_occupied.data(), 0, m_end);
		}

		const_all_iterator end_all() const
		{
			return const_all_iterator(m_slots.data(), m_occupied.data(), m_end, m_end);
		}

		const_all_iterator cbegin_all() const { return begin_all(); }
		const_all_iterator cend_all() const { return end_all(); }

		unique_stable_vector() = default;

		~unique_stable_vector()
		{
			clear();
		}

		// Insert value, returns [index, blue=inserted, false=not_inserted]
		std::pair<index_type, bool> insert(const T& value) noexcept
		{
			assert(m_size < max_elements);

			auto it = m_lookup.find(value);
			if (it != m_lookup.end())
				return { it->second, false };

			const index_type slot = m_firstFree;

			new (&m_slots[slot]) T(value);
			m_occupied[slot] = true;
			m_end = std::max(m_end, slot + 1);
			++m_size;

			m_lookup.emplace(value, slot);
			findNextFree();

			return { slot, true };
		}

		std::pair<index_type, bool> insert(T&& value) noexcept
		{
			assert(m_size < max_elements);

			auto it = m_lookup.find(value);
			if (it != m_lookup.end())
				return { it->second, false };

			const index_type slot = m_firstFree;
			new (&m_slots[slot]) T(std::move(value));
			m_end = std::max(m_end, slot + 1);
			m_occupied[slot] = true;
			++m_size;

			m_lookup.emplace(m_slots[slot], slot);
			findNextFree();

			return { slot, true };
		}

		// Borrar por índice
		void erase(index_type index) noexcept
		{
			assert(index < max_elements);
			assert(m_occupied[index]);

			if constexpr (!std::is_trivially_destructible_v<T>)
				m_slots[index].~T();

			m_lookup.erase(m_slots[index]);
			m_occupied[index] = false;
			m_firstFree = std::min(m_firstFree, index);
			--m_size;
			findNewEnd();
		}

		bool erase(const T& value) noexcept
		{
			auto it = m_lookup.find(value);
			if (it == m_lookup.end())
				return false;

			erase(it->second);
			return true;
		}

		void clear() noexcept 
		{
			for (size_t i = 0; i < max_elements; ++i) 
			{
				if (m_occupied[i]) 
				{
					if constexpr (!std::is_trivially_destructible_v<T>)
						m_slots[i].~T();
				}
			}
			m_lookup.clear();
			m_occupied = {};
			m_firstFree = 0;
			m_end = 0;
			m_size = 0;
		}

		T* get(index_type index) noexcept
		{
			return (index < max_elements && m_occupied[index]) ? &m_slots[index] : nullptr;
		}

		const T* get(index_type index) const noexcept 
		{
			return (index < max_elements && m_occupied[index]) ? &m_slots[index] : nullptr;
		}

		T& operator[](index_type index) 
		{
			assert(m_occupied[index]);
			return m_slots[index];
		}

		const T& operator[](index_type index) const 
		{
			assert(m_occupied[index]);
			return m_slots[index];
		}

		index_type find(const T& value) const noexcept 
		{
			auto it = m_lookup.find(value);
			return (it != m_lookup.end()) ? it->second : max_elements;
		}

		bool contains(const T& value) const noexcept 
		{
			return m_lookup.find(value) != m_lookup.end();
		}

		bool valid_index(index_type index) const noexcept
		{
			return (index < max_elements && m_occupied[index]);
		}

		size_t size() const noexcept { return m_size; }
		size_t storage_size() const noexcept { return m_end; }
		size_t capacity() const noexcept { return max_elements; }
		bool empty() const noexcept { return m_size == 0; }
		bool full() const noexcept { return m_size >= max_elements; }

	private:

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

		std::array<T, max_elements> m_slots;
		std::array<uint8_t, max_elements> m_occupied = {};
		std::unordered_map<T, index_type, Hash> m_lookup;
		size_t m_firstFree = 0;
		size_t m_end = 0;
		size_t m_size = 0;
	};
}