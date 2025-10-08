#pragma once

#include <cstdlib>
#include <functional>

namespace st
{

// A blob is a package for untyped data
class IBlob
{
public:

	virtual ~IBlob() = default;

	[[nodiscard]] virtual char* data() const = 0;
	[[nodiscard]] virtual size_t size() const = 0;
	[[nodiscard]] virtual bool empty() const = 0;
};

// Specific blob implementation that owns the data and frees it when deleted.
// It uses free, so original data must have been allocated using malloc.
class Blob : public IBlob
{
public:

	Blob() = default;
	Blob(char* data, size_t size, std::function<void(void*)> deleter = nullptr) : 
		m_data{ data }, m_size{ size }, m_deleter{ std::move(deleter) } 
	{}

	// non-copiable
	Blob(const Blob&) = delete;
	Blob& operator=(const Blob&) = delete;

	Blob(Blob&& other) : 
		m_data{ other.m_data }, m_size{ other.m_size }, m_deleter{ std::move(other.m_deleter) }
	{
		other.m_data = nullptr;
		other.m_size = 0;
	}
	Blob& operator=(Blob&& other)
	{
		m_data = other.m_data;
		m_size = other.m_size;
		m_deleter = std::move(other.m_deleter);
		other.m_data = nullptr;
		other.m_size = 0;
		return *this;
	}

	virtual ~Blob() override
	{
		reset();
	}

	[[nodiscard]] virtual char* data() const { return m_data; }
	[[nodiscard]] virtual size_t size() const { return m_size; }
	[[nodiscard]] virtual bool empty() const { return m_data == nullptr || m_size == 0; }

	void reset()
	{
		if (m_data)
		{
			if (m_deleter) m_deleter(m_data);
			else free(m_data);
		}
		m_data = nullptr;
		m_size = 0;
		m_deleter = nullptr;
	}

private:

	char* m_data = nullptr;
	size_t m_size = 0;
	std::function<void(void*)> m_deleter = nullptr;
};

// Specific blob implementation that does not owns the data
class WeakBlob : public IBlob
{
public:

	WeakBlob() = default;
	WeakBlob(char* data, size_t size) : m_data(data), m_size(size) {}
	explicit WeakBlob(const Blob& b) : m_data{ b.data() }, m_size{ b.size() } {}
	virtual ~WeakBlob() override = default;

	[[nodiscard]] virtual char* data() const { return m_data; }
	[[nodiscard]] virtual size_t size() const { return m_size; }
	[[nodiscard]] virtual bool empty() const { return m_data == nullptr || m_size == 0; }

private:

	char* m_data;
	size_t m_size;
};

} // namespace st