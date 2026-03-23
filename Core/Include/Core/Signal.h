#pragma once

#include <future>

namespace alm
{

class SignalEmitter
{
	friend class SignalListener;

public:

	SignalEmitter() : m_Future{ m_Promise.get_future().share() } {}
	~SignalEmitter() = default;

	// noncopyable
	SignalEmitter(const SignalEmitter&) = delete;
	SignalEmitter& operator =(SignalEmitter&) = delete;

	SignalEmitter(SignalEmitter&& other) noexcept :
		m_Promise{ std::move(other.m_Promise) },
		m_Future{ std::move(other.m_Future) },
		m_Signaled{ other.m_Signaled.exchange(false) }
	{
		other.ReInit();
	}

	SignalEmitter& operator =(SignalEmitter&& other) noexcept
	{
		if (this != &other)
		{
			m_Promise = std::move(other.m_Promise);
			m_Future = std::move(other.m_Future);
			m_Signaled = other.m_Signaled.exchange(false);

			other.ReInit();
		}
		return *this;
	}

	void Signal()
	{
		if (!m_Signaled.exchange(true))
		{
			m_Promise.set_value();
		}
	}

	// warning! will orphan all listeners
	void Reset()
	{
		if (!m_Signaled)
			Signal();

		ReInit();
	}

	SignalListener GetListener();

private:

	void ReInit()
	{
		m_Promise = {};
		m_Future = m_Promise.get_future().share();
		m_Signaled = false;
	}

	std::promise<void> m_Promise;
	std::shared_future<void> m_Future;
	std::atomic<bool> m_Signaled{ false };
};

class SignalListener
{
	friend class SignalEmitter;

public:

	SignalListener() = default;
	SignalListener(SignalEmitter& emitter) : m_Future(emitter.m_Future) {}

	void Wait() const
	{
		if (m_Future.valid())
			m_Future.wait();
	}

	template <class _Rep, class _Per>
	void Wait(const std::chrono::duration<_Rep, _Per>& timeOut) const
	{
		if (m_Future.valid())
			m_Future.wait_for(timeOut);
	}

	// Returns true if signaled, otherwise false
	bool Poll() const
	{
		if (m_Future.valid())
			return m_Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		return true;
	}

	bool Orphan() const
	{
		return !m_Future.valid();
	}

private:

	explicit SignalListener(std::shared_future<void> fut) : m_Future{ fut } {}

	std::shared_future<void> m_Future;
};

inline SignalListener SignalEmitter::GetListener()
{
	return SignalListener{ m_Future };
}

} // namespace st