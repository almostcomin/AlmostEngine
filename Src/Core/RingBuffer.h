#pragma once

#include <array>

namespace st
{

	template<class T, size_t N>
	class RingBuffer
	{
	public:

		using value = T;

		bool Push(const T& value)
		{
			if (Full()) return false;
			m_Buffer[m_Tail] = value;
			m_Tail = (m_Tail + 1) % N;
			++m_Size;
			return true;
		}

		bool Push(T&& value)
		{
			if (Full()) return false;
			m_Buffer[m_Tail] = std::move(value);
			m_Tail = (m_Tail + 1) % N;
			++m_Size;
			return true;
		}

		bool Pop(T& obj)
		{
			if (Empty()) return false;
			obj = std::move(m_Buffer[m_Head]);
			m_Head = (m_Head + 1) % N;
			--m_Size;
			return true;
		}

		bool Pop()
		{
			if (Empty()) return false;
			m_Buffer[m_Head] = {};
			m_Head = (m_Head + 1) % N;
			--m_Size;
			return true;
		}

		bool Empty() const noexcept { return m_Size == 0; }
		bool Full() const noexcept { return m_Size == N; }
		size_t Size() const noexcept { return m_Size; }
		static constexpr size_t Capacity() noexcept { return N; }

		T& Front() { return m_Buffer[m_Head]; }
		const T& Front() const { return m_Buffer[m_Head]; }

		T& Back() { return m_Buffer[(m_Tail + N - 1) % N]; }
		const T& Back() const { return m_Buffer[(m_Tail + N - 1) % N]; }

	private:

		std::array<T, N> m_Buffer = {};
		size_t m_Head = 0;
		size_t m_Tail = 0;
		size_t m_Size = 0;
	};

} // namespace st