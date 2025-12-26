#pragma once

#include <vector>
#include <unordered_map>

namespace st
{

template<typename T, typename Hash = std::hash<T>>
struct unique_vector
{
	using index_type = size_t;

	index_type insert(const T& value)
	{
		auto it = m_lookup.find(value);
		if (it != m_lookup.end())
			return it->second;

		index_type idx = m_data.size();
		m_data.push_back(value);
		m_lookup.emplace(value, idx);
		return idx;
	}

	const T& operator[](index_type i) const { return m_data[i]; }
	T& operator[](index_type i) { return m_data[i]; }

	size_t size() const { return m_data.size(); }
	bool empty() const { return m_data.empty();	}

	std::vector<T>::iterator begin() { return m_data.begin(); }
	std::vector<T>::iterator end() { return m_data.end(); }

private:

	std::vector<T> m_data;
	std::unordered_map<T, index_type, Hash> m_lookup;
};

}