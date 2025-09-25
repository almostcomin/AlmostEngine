#pragma once

#include <memory>

namespace st
{

template<class T>
class weak
{
    template<class T>
    friend class unique;
    template<class T>
    friend class enable_weak_from_this;
public:
    weak() : ptr(nullptr) {};
    bool expired() const { return flag.expired(); }
    T* get() const { return expired() ? nullptr : ptr; }
    void reset() { ptr = nullptr; flag.reset(); }

    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }
    operator bool() const { return !expired(); }

private:
    weak(T* p, const std::weak_ptr<void>& f) : ptr(p), flag(f) {}
    T* ptr = nullptr;
    std::weak_ptr<void> flag;
};

template<class T>
class enable_weak_from_this
{
    template<class T>
    friend class unique;
public:
    weak<T> weak_from_this()
    {
        return weak<T>(static_cast<T*>(this), control);
    }
private:
    std::weak_ptr<void> control;
};

template<class T>
class unique
{
    template <typename U>
    friend class unique;

public:
    unique() = default;

    explicit unique(T* ptr) :
        obj(ptr),
        flag(std::make_shared<int>(0))
    {
        if constexpr (std::is_base_of_v<enable_weak_from_this<T>, T>)
            obj->control = flag;
    }

    template<typename... Args>
    explicit unique(Args&&... args) :
        obj(std::make_unique<T>(std::forward<Args>(args)...)),
        flag(std::make_shared<int>(0))
    {
        if constexpr (std::is_base_of_v<enable_weak_from_this<T>, T>)
            obj->control = flag;
    }

    unique(const unique&) = delete;
    unique& operator=(const unique&) = delete;

    unique(unique&& other) :
        obj(std::move(other.obj)),
        flag(std::move(other.flag))
    {}

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    unique(unique<U>&& other) :
        obj(std::move(other.obj)),
        flag(std::move(other.flag))
    {}

    unique& operator=(unique&& other)
    {
        if (this != &other)
        {
            obj = std::move(other.obj);
            flag = std::move(other.flag);
        }
        return *this;
    }

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    unique& operator=(unique<U>&& other)
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

    weak<T> get_weak() const { return weak<T>{obj.get(), flag}; }

private:
    std::unique_ptr<T> obj;
    std::shared_ptr<void> flag;
};

template<class T, typename... Args>
unique<T> make_unique_with_weak(Args&&... args)
{
    T* p = new T(std::forward<Args>(args)...);
    return unique<T>(p);
}

} // namespace st