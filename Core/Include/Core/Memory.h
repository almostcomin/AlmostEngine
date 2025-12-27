#pragma once

#include <memory>

namespace st
{

template<class T>
class weak
{
    template <typename U>
    friend class weak;

    template<class T>
    friend class unique;

    template<class T>
    friend class enable_weak_from_this;

    template<class T, class U>
    friend st::weak<T> static_pointer_cast(const st::weak<U>& r) noexcept;

    template<class T, class U>
    friend st::weak<T> dynamic_pointer_cast(const st::weak<U>& r) noexcept;

    template<class T, class U>
    friend st::weak<T> checked_pointer_cast(const st::weak<U>& r) noexcept;

public:
    weak() : ptr(nullptr) {};
    weak(std::nullptr_t) : ptr(nullptr) {}

    weak(const weak<T>& other) :
        ptr(other.ptr),
        flag(other.flag)
    {}

    weak(weak<T>&& other) :
        ptr(std::move(other.ptr)),
        flag(std::move(other.flag))
    {}

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    weak(const weak<U>& other) :
        ptr(other.ptr),
        flag(other.flag)
    {}

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    weak(weak<U>&& other) :
        ptr(std::move(other.ptr)),
        flag(std::move(other.flag))
    {}

    weak<T>& operator=(const weak<T>& other)
    {
        if (this != &other)
        {
            ptr = other.ptr;
            flag = other.flag;
        }
        return *this;
    }

    weak<T>& operator=(weak<T>&& other)
    {
        if (this != &other)
        {
            ptr = std::move(other.ptr);
            flag = std::move(other.flag);
        }
        return *this;
    }

    T& operator*() const { assert(!expired()); return *ptr; }
    T* operator->() const { assert(!expired()); return ptr; }
    operator bool() const { return ptr != nullptr; }

    template<class U>
    bool operator ==(const weak<U>& other) const { return ptr == other.ptr; }
    template<class U>
    bool operator !=(const weak<U>& other) const { return ptr != other.ptr; }

    bool operator==(std::nullptr_t) const { return ptr == nullptr; }
    bool operator!=(std::nullptr_t) const { return ptr != nullptr; }

    friend bool operator==(std::nullptr_t, const weak& w) { return w.ptr == nullptr; }
    friend bool operator!=(std::nullptr_t, const weak& w) { return w.ptr != nullptr; }

    bool operator<(const weak& other) const {
        return ptr < other.ptr;
    }

    bool expired() const { return flag.expired(); }
    T* get() const 
    { 
        assert(!expired()); 
        return ptr; 
    }
    void reset() { ptr = nullptr; flag.reset(); }

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

    using Deleter = std::function<void(void*)>;

    unique() :
        obj{ nullptr, [](void* ptr) { delete static_cast<T*>(ptr); } },
        flag{}
    {}

    explicit unique(T* ptr) :
        obj{ ptr, [](void* ptr) { delete static_cast<T*>(ptr); } },
        flag{ std::make_shared<int>(0) }
    {
        setup_weak_from_this();
    }

    explicit unique(T* ptr, Deleter&& del) :
        obj(ptr, std::forward<Deleter>(del)),
        flag(std::make_shared<int>(0))
    {
        setup_weak_from_this();
    }

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    explicit unique(U* ptr) :
        obj(ptr, [](void* ptr) { delete static_cast<T*>(ptr); }),
        flag(std::make_shared<int>(0))
    {
        setup_weak_from_this();
    }

    template<class U, typename Deleter, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    explicit unique(U* ptr, Deleter&& del) :
        obj(ptr, std::forward<Deleter>(del)),
        flag(std::make_shared<int>(0))
    {
        setup_weak_from_this();
    }

    unique(const unique&) = delete;
    unique& operator=(const unique&) = delete;

    unique(unique&& other) :
        obj(std::move(other.obj)),
        flag(std::move(other.flag))
    {}

    template<class U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    unique(unique<U>&& other) :
        obj{ std::move(other.obj.release()), std::move(other.obj.get_deleter()) },
        flag{ std::move(other.flag) }
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
    bool valid() const { return obj ? true : false; }
    operator bool() const { return valid(); }

    bool operator<(const unique& other) const {
        return obj.get() < other.obj.get();
    }

    template<class U>
    bool operator ==(const unique<U>& other) const { return obj.get() == other.obj.get(); }
    template<class U>
    bool operator !=(const unique<U>& other) const { return obj.get() != other.obj.get(); }

    T* release()
    {
        flag.reset();
        return obj.release();
    }

    void reset() 
    { 
        obj.reset(); 
        flag.reset(); 
    }

    void reset(T* ptr)
    {
        obj.reset(ptr);
        flag = std::make_shared<int>(0);
    }

    weak<T> get_weak() const { return weak<T>{obj.get(), flag}; }

private:

    void setup_weak_from_this() 
    {
        if constexpr (std::is_base_of_v<enable_weak_from_this<T>, T>) 
        {
            if (obj) 
            {
                flag = std::make_shared<int>(0);
                obj->control = flag;
            }
        }
    }

    std::unique_ptr<T, std::function<void(void*)>> obj;
    std::shared_ptr<void> flag;
};

template<class T, typename... Args>
unique<T> make_unique_with_weak(Args&&... args)
{
    T* p = new T(std::forward<Args>(args)...);
    return unique<T>(p);
}

template<class T, class U>
st::weak<T> static_pointer_cast(const st::weak<U>& r) noexcept
{
    static_assert(!std::is_same<T, U>::value, "Redundant checked_pointer_cast");
    auto* p = static_cast<T*>(r.get());
    return st::weak<T>{ p, r.flag };
}

template<class T, class U>
st::weak<T> dynamic_pointer_cast(const st::weak<U>& r) noexcept
{
    static_assert(!std::is_same<T, U>::value, "Redundant checked_pointer_cast");
    if (!r) return nullptr;
    auto* p = dynamic_cast<T*>(r.get());
    if (!p) return { nullptr };
    return st::weak<T>{ p, r.flag };
}

template<class T, class U>
st::weak<T> checked_pointer_cast(const st::weak<U>& r) noexcept
{
#ifdef _DEBUG
    if (!r) return nullptr;
    auto* p = dynamic_cast<T*>(r.get());
    if (!p)
    {
        assert(!"Invalid type cast");
        return { nullptr };
    }
    return st::weak<T>{ p, r.flag };
#else
    return static_pointer_cast<T>(u);
#endif
}

} // namespace st