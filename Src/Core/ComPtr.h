#pragma once

#include <type_traits>
#include <Unknwn.h>

namespace st
{

template <typename T>
class ComPtr 
{
    static_assert(std::is_base_of<IUnknown, T>::value,
        "ComPtr only works with COM objects");

public:

    ComPtr() noexcept : ptr_{ nullptr } {}
    ComPtr(T* ptr) noexcept : ptr_{ ptr } { AddRef(); }
    
    ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) 
    {
        AddRef();
    }
    
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) 
    {
        other.ptr_ = nullptr;
    }

    ~ComPtr() 
    { 
        Release(); 
    }

    ComPtr& operator=(const ComPtr& other) noexcept 
    {
        if (this != &other)
            Reset(other.ptr_);
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept 
    {
        if (this != std::addressof(other))
        {
            Release();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* Get() const noexcept { return ptr_; }
    T** operator&() noexcept { return &ptr_; } // For IID_PPV_ARGS-like calls
    T* operator->() const noexcept { return ptr_; }

    operator bool() const noexcept { return ptr_ != nullptr; }
    bool operator==(const ComPtr& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator!=(const ComPtr& other) const noexcept { return ptr_ != other.ptr_; }

    T** GetAddressOf() noexcept { return &ptr_; }
    void** GetVoidAddressOf() noexcept { return reinterpret_cast<void**>(&ptr_); }

    void Reset(T* ptr = nullptr) noexcept 
    {
        Release();
        ptr_ = ptr;
        AddRef();
    }

    uint32_t AddRef() noexcept 
    { 
        if (ptr_)
            return ptr_->AddRef();
        else
            return 0;
    }
    uint32_t Release() noexcept 
    { 
        if (ptr_)
        {
            uint32_t refcount = ptr_->Release();
            ptr_ = nullptr;
            return refcount;
        }
        else
            return 0;
    }

private:

    T* ptr_ = nullptr;
};

} // namespace st