#pragma once

#include <memory>

namespace st
{

template<class T>
class weak_handle
{
    template<class T>
    friend class unique_with_weak_ptr;
    template<class T>
    friend class enable_weak_from_this;
public:
    weak_handle() : ptr(nullptr) {};
    bool expired() const { return flag.expired(); }
    T* get() const { return expired() ? nullptr : ptr; }
    void reset() { ptr = nullptr; flag.reset(); }

    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }
    operator bool() const { return !expired(); }

private:
    weak_handle(T* p, const std::weak_ptr<void>& f) : ptr(p), flag(f) {}
    T* ptr = nullptr;
    std::weak_ptr<void> flag;
};

template<class T>
class enable_weak_from_this
{
    template<class T>
    friend class unique_with_weak_ptr;
public:
    weak_handle<T> weak_from_this()
    {
        return weak_handle<T>(static_cast<T*>(this), control);
    }
private:
    std::weak_ptr<void> control;
};

template<class T>
class unique_with_weak_ptr
{
    template <typename U>
    friend class unique_with_weak_ptr;

public:
    unique_with_weak_ptr() = default;

    template<typename... Args>
    explicit unique_with_weak_ptr(Args... args) :
        obj(std::make_unique<T>(std::forward<Args>(args)...)),
        flag(std::make_shared<int>(0))
    {
        if constexpr (std::is_base_of_v<enable_weak_from_this<T>, T>)
            obj->control = flag;
    }

    unique_with_weak_ptr(const unique_with_weak_ptr&) = delete;
    unique_with_weak_ptr& operator=(const unique_with_weak_ptr&) = delete;

    unique_with_weak_ptr(unique_with_weak_ptr&& other) noexcept :
        obj(std::move(other.obj)),
        flag(std::move(other.flag))
    {}

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    unique_with_weak_ptr(unique_with_weak_ptr<U>&& other) noexcept :
        obj(std::move(other.obj)),
        flag(std::move(other.flag))
    {
    }

    unique_with_weak_ptr& operator=(unique_with_weak_ptr&& other) noexcept
    {
        if (this != &other)
        {
            obj = std::move(other.obj);
            flag = std::move(other.flag);
        }
        return *this;
    }

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    unique_with_weak_ptr& operator=(unique_with_weak_ptr<U>&& other) noexcept
    {
        if (this != &other)
        {
            obj = std::move(other.obj);
            flag = std::move(other.flag);
        }
        return *this;
    }

    T* get() const { return obj.get(); }
    T& operator*() const { return *obj; }
    T* operator->() const { return obj.get(); }
    operator bool() const { return obj ? true : false; }

    weak_handle<T> weak() const { return weak_handle<T>(obj.get(), flag); }

private:
    std::unique_ptr<T> obj;
    std::shared_ptr<void> flag;
};

template<class T, typename... Args>
unique_with_weak_ptr<T> make_unique_with_weak(Args&&... args)
{
    return unique_with_weak_ptr<T>(std::forward<Args>(args)...);
}

} // namespace st