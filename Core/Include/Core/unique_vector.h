#pragma once

#include <vector>
#include <unordered_map>
#include <utility>

// TODO:
// m_lookup keeps a copy of the key. Fine for small T (pointers, handles). If T becomes large,
// switch to a pointer-keyed map with transparent hashing over m_storage.
// https://trello.com/c/yFfs4ElR/4-usat-t-en-lugar-de-t-como-key-en-el-lookup-del-uniquestablevector

namespace alm
{

template<typename T, typename Hash = std::hash<T>>
struct unique_vector
{
	using index_type = size_t;

	std::pair<index_type, bool> insert(const T& value)
	{
		auto it = m_lookup.find(value);
		if (it != m_lookup.end())
			return { it->second, false };

		index_type idx = m_data.size();
		m_data.push_back(value);
		m_lookup.emplace(value, idx);
		return { idx, true };
	}

	const T& operator[](index_type i) const { return m_data[i]; }
	T& operator[](index_type i) { return m_data[i]; }

	size_t size() const { return m_data.size(); }
	bool empty() const { return m_data.empty();	}
	const std::vector<T>& as_vector() const { return m_data; }

	std::vector<T>::iterator begin() { return m_data.begin(); }
	std::vector<T>::iterator end() { return m_data.end(); }

	std::vector<T>::const_iterator begin() const { return m_data.begin(); }
	std::vector<T>::const_iterator end() const { return m_data.end(); }

	// swap-and-pop
	void fast_erase(const T& value)
	{
		auto it = m_lookup.find(value);
		if (it == m_lookup.end())
			return;

		const index_type idx = it->second;
		const index_type lastIdx = m_data.size() - 1;

		if (idx != lastIdx)
		{
			// Move the last element into the freed slot and update its lookup entry.
			m_data[idx] = std::move(m_data[lastIdx]);
			m_lookup[m_data[idx]] = idx;
		}

		m_data.pop_back();
		m_lookup.erase(it);
	}

	void reserve(size_t size)
	{
		m_data.reserve(size);
	}

private:

	std::vector<T> m_data;
	std::unordered_map<T, index_type, Hash> m_lookup;
};

}