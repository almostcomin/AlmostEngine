#pragma once

#include <string>
#include <string_view>
#include <cassert>

namespace st
{

struct noncopyable_nonmovable 
{
protected:
    noncopyable_nonmovable() = default;
    ~noncopyable_nonmovable() = default;

    noncopyable_nonmovable(const noncopyable_nonmovable&) = delete;
    noncopyable_nonmovable& operator=(const noncopyable_nonmovable&) = delete;
    noncopyable_nonmovable(noncopyable_nonmovable&&) = delete;
    noncopyable_nonmovable& operator=(noncopyable_nonmovable&&) = delete;
};

std::string_view GetFilenameFromPath(const std::string_view& path);
std::string_view GetExtensionFromPath(const std::string_view& path);

// Based on 128 bits number
std::string MakeUniqueStringId();

inline uint32_t NextPowerOf2(uint32_t v)
{
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

template<typename T>
constexpr T KiB(T val)
{
    return val * 1024;
}

template<typename T>
constexpr T MiB(T val)
{
    return KiB(val) * 1024;
}

// A type cast that is safer than static_cast in debug builds, and is a simple static_cast in release builds.
// Used for downcasting various ISomething* pointers to their implementation classes in the backends.
template <typename T, typename U>
T checked_cast(U u)
{
    static_assert(!std::is_same<T, U>::value, "Redundant checked_cast");
#ifdef _DEBUG
    if (!u) return nullptr;
    T t = dynamic_cast<T>(u);
    if (!t) assert(!"Invalid type cast");  // NOLINT(clang-diagnostic-string-conversion)
    return t;
#else
    return static_cast<T>(u);
#endif
}

#define ENUM_CLASS_FLAG_OPERATORS(T) \
    inline T operator | (T a, T b) { return T(uint32_t(a) | uint32_t(b)); } \
    inline T operator & (T a, T b) { return T(uint32_t(a) & uint32_t(b)); } /* NOLINT(bugprone-macro-parentheses) */ \
    inline T operator ~ (T a) { return T(~uint32_t(a)); } /* NOLINT(bugprone-macro-parentheses) */ \
    inline bool operator !(T a) { return uint32_t(a) == 0; } \
    inline bool operator ==(T a, uint32_t b) { return uint32_t(a) == b; } \
    inline bool operator !=(T a, uint32_t b) { return uint32_t(a) != b; }

template<typename T>
constexpr bool has_flag(T lhs, T rhs)
{
    return (lhs & rhs) == rhs;
}

std::wstring ToWide(const char* utf8);
std::string ToUtf8(const wchar_t* wide);

} // namespace st