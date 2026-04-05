#pragma once

namespace alm
{

// Inherit from this to avoid copy/move
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

// a 128 bits number stringyfied
std::string MakeUniqueStringId();

// FNV-1a hash
constexpr uint32_t Hash(std::string_view str)
{
    uint32_t hash = 2166136261u;
    for (char c : str)
        hash = (hash ^ c) * 16777619u;
    return hash;
}

// String conversion
std::wstring ToWide(const char* utf8);
std::string ToUtf8(const wchar_t* wide);

// Convert float (32 bits) to half-float (16 bits) (IEEE 754)
uint16_t FloatToHalf(float f);

inline bool AlmostEqual(float v1, float v2, float epsilon = 1.0e-05f)
{
    return abs(v2 - v1) <= epsilon;
}

template<glm::length_t L, typename T, glm::qualifier Q>
inline bool AlmostEqual(const glm::vec<L, T, Q>& v1, const glm::vec<L, T, Q>& v2, T epsilon = T(1.0e-05))
{
    return glm::all(glm::lessThanEqual(glm::abs(v2 - v1), glm::vec<L, T, Q>(epsilon)));
}

template<typename T>
constexpr T AlignUp(T value, T alignment)
{
    return ((value + alignment - T(1)) / alignment) * alignment;
}
template<typename T>
constexpr bool IsAligned(T value, T alignment)
{
    return (value % alignment) == T(0);
}

template<typename T>
constexpr T DivRoundUp(T value, T top)
{
    return (value + top - T(1)) / top;
}

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

// A type cast that is dynamic_cast in debug builds and static_cast in release builds.
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

// Add some operators to enums flags
#define ENUM_CLASS_FLAG_OPERATORS(T) \
    inline T operator | (const T& a, const T& b) { return T(uint32_t(a) | uint32_t(b)); } \
    inline T operator & (const T& a, const T& b) { return T(uint32_t(a) & uint32_t(b)); } \
    inline T operator ~ (const T& a) { return T(~uint32_t(a)); } \
    inline T operator |= (T& a, const T& b) { a = T(uint32_t(a) | uint32_t(b)); return a; } \
    inline T operator &= (T& a, const T& b) { a = T(uint32_t(a) & uint32_t(b)); return a; } \
    inline bool operator !(const T& a) { return uint32_t(a) == 0; } \
    inline bool operator ==(const T& a, const uint32_t& b) { return uint32_t(a) == b; } \
    inline bool operator !=(const T& a, const uint32_t& b) { return uint32_t(a) != b; } \

template<typename T>
constexpr bool has_all_flags(T lhs, T rhs)
{
    return (lhs & rhs) == rhs;
}

template<typename T>
constexpr bool has_any_flag(T lhs, T rhs)
{
    return (lhs & rhs) != 0;
}

template<class T>
constexpr bool any(const T& c)
{
    return std::any_of(c.begin(), c.end(), std::identity{});
}

constexpr uint32_t MakeRGBA(byte r, byte g, byte b, byte a)
{
    return uint32_t(r) << 24 | uint32_t(g) << 16 | uint32_t(b) << 8 | a;
}

} // namespace st