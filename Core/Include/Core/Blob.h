#pragma once

#include <cstdlib>
#include <functional>
#include <memory>

namespace alm
{
// Specific blob implementation that owns the data and frees it when deleted.
// Custom deleter can be specified, if not, free will be used
class Blob
{
public:

	// non-copiable
	Blob(const Blob&) = delete;
	Blob& operator=(const Blob&) = delete;

	Blob() = default;

	Blob(char* data, size_t size) :
		m_data{ data }, m_size{ size }, m_real_ptr{ data }, m_deleter { nullptr }
	{}

	Blob(char* data, size_t size, char* real_ptr, std::function<void(void*)> deleter) :
		m_data{ data }, m_size{ size }, m_real_ptr{ real_ptr }, m_deleter{ std::move(deleter) }
	{}

	Blob(Blob&& other) noexcept :
		m_data{ std::exchange(other.m_data, nullptr) },
		m_size{ std::exchange(other.m_size, 0) },
		m_real_ptr{ std::exchange(other.m_real_ptr, nullptr) },
		m_deleter{ std::move(other.m_deleter) }
	{}

	~Blob()
	{
		reset();
	}

	Blob& operator=(Blob&& other) noexcept
	{
		if (this == &other) return *this;
		reset();
		m_data = std::exchange(other.m_data, nullptr);
		m_size = std::exchange(other.m_size, 0);
		m_real_ptr = std::exchange(other.m_real_ptr, nullptr);
		m_deleter = std::move(other.m_deleter);
		return *this;
	}

	char* data() const { return m_data; }
	size_t size() const { return m_size; }
	bool empty() const { return m_data == nullptr || m_size == 0; }

	operator bool() const { return !empty(); }

	void reset()
	{
		if (m_real_ptr)
		{
			if (m_deleter) m_deleter(m_real_ptr);
			else free(m_real_ptr);
		}
		m_data = nullptr;
		m_size = 0;
		m_real_ptr = nullptr;
		m_deleter = nullptr;
	}

protected:

	char* m_data = nullptr;
	size_t m_size = 0;
	char* m_real_ptr = nullptr;
	std::function<void(void*)> m_deleter = nullptr;
};

class SharedBlob
{
public:

	SharedBlob() = default;

	SharedBlob(char* data, size_t size) :
		m_data{ data }, m_size{ size }, m_proxy{ std::shared_ptr<proxy_t>(new proxy_t, customDeleter) }
	{
		m_proxy->data = data;
	}

	SharedBlob(char* data, size_t size, char* real_ptr, std::function<void(void*)> deleter) :
		m_data{ data }, m_size{ size }, m_proxy{ std::shared_ptr<proxy_t>(new proxy_t, customDeleter) }
	{
		m_proxy->data = real_ptr;
		m_proxy->deleter = std::move(deleter);
	}

	SharedBlob(const SharedBlob& other) : m_data{ other.m_data }, m_size{ other.m_size }, m_proxy{ other.m_proxy }
	{}

	SharedBlob(char* data, size_t size, const SharedBlob& other) : m_data(data), m_size(size), m_proxy(other.m_proxy)
	{}

	SharedBlob(SharedBlob&& other) noexcept :
		m_data{ std::exchange(other.m_data, nullptr) },
		m_size{ std::exchange(other.m_size, 0) },
		m_proxy{ std::move(other.m_proxy) }
	{}

	~SharedBlob()
	{
		reset();
	}

	SharedBlob& operator=(const SharedBlob& other)
	{
		if (this == &other) return *this;
		m_data = other.m_data;
		m_size = other.m_size;
		m_proxy = other.m_proxy;
		return *this;
	}

	SharedBlob& operator=(SharedBlob&& other) noexcept
	{
		if (this == &other) return *this;
		m_data = std::exchange(other.m_data, nullptr);
		m_size = std::exchange(other.m_size, 0);
		m_proxy = std::move(other.m_proxy);
		return *this;
	}

	char* data() const { return m_data; }
	size_t size() const { return m_size; }
	bool empty() const { return m_data == nullptr || m_size == 0; }

	void reset()
	{
		m_data = nullptr;
		m_size = 0;
		m_proxy.reset();
	}

	void setsub(char* data, size_t size)
	{
		m_data = data;
		m_size = size;
	}

protected:

	struct proxy_t
	{
		char* data = 0;
		std::function<void(void*)> deleter = nullptr;
	};

	static void customDeleter(proxy_t* ptr)
	{
		if (!ptr) return;
		if (ptr->deleter) ptr->deleter(ptr->data);
		else free(ptr->data);
	}

	std::shared_ptr<proxy_t> m_proxy;
	char* m_data = nullptr;
	size_t m_size = 0;
};

// Specific blob implementation that does not owns the data
class WeakBlob
{
public:

	WeakBlob() = default;
	WeakBlob(char* data, size_t size) : m_data(data), m_size(size) {}
	explicit WeakBlob(const Blob& b) : m_data{ b.data() }, m_size{ b.size() } {}
	explicit WeakBlob(const SharedBlob& b) : m_data{ b.data() }, m_size{ b.size() } {}
	~WeakBlob() = default;

	char* data() const { return m_data; }
	size_t size() const { return m_size; }
	bool empty() const { return m_data == nullptr || m_size == 0; }

	operator bool() const { return !empty(); }

private:

	char* m_data = nullptr;
	size_t m_size = 0;
};

} // namespace st