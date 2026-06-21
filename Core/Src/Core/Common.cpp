#include "Core/CorePCH.h"
#include "Core/Common.h"

std::string_view alm::GetFilenameFromPath(const std::string_view& path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        return path;
    }
    else
    {
        return path.substr(pos + 1);
    }
}

std::string_view alm::GetExtensionFromPath(const std::string_view& path)
{
    size_t pos = path.find_last_of('.');
    if (pos == std::string::npos)
    {
        return {};
    }
    else
    {
        return path.substr(pos + 1);
    }
}

std::string alm::MakeUniqueStringId()
{
    static std::atomic<uint32_t> counter = 0;

    uint64_t timePart = std::chrono::steady_clock::now().time_since_epoch().count();
    uint64_t randPart1 = ((uint64_t)std::random_device{}() << 32) | std::random_device{}();
    uint64_t randPart2 = ((uint64_t)std::random_device{}() << 32) | std::random_device{}();
    uint64_t countPart = (uint64_t)(counter++);

    uint64_t high = timePart ^ randPart1;
    uint64_t low = randPart2 ^ (countPart + (timePart << 13));

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << high
        << std::setw(16) << std::setfill('0') << low;

    return ss.str();
}

std::wstring alm::ToWide(const char* utf8)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring wide(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), size);
    return wide;
}

std::string alm::ToUtf8(const wchar_t* wide)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
        return {};
    std::string utf8(size_needed - 1, 0); // -1 to remove last '\0'
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), size_needed, nullptr, nullptr);
    return utf8;
}

/**
 * Convert a 32-bit single-precision float to a 16-bit half-precision float
 * following IEEE 754 with round‑to‑nearest‑even.
 *
 * \param f  The input float.
 * \return   The converted half-precision value as a 16‑bit unsigned integer.
 */
uint16_t FloatToHalf(float f)
{
    uint32_t i;
    memcpy(&i, &f, sizeof(f));

    uint32_t sign = (i >> 31) & 0x1;
    int32_t  exp = (i >> 23) & 0xFF;    // biased exponent (0..255)
    uint32_t mantissa = i & 0x7FFFFF;   // 23 fractional bits

    // --- Handle special cases: zero, inf, NaN ---------------------------------
    if (exp == 0) {
        // Zero or denormal (float denormals are extremely small → flush to zero in half)
        return sign << 15;   // signed zero
    }
    if (exp == 255) {
        // Infinity or NaN
        uint16_t result = (sign << 15) | 0x7C00;   // 0x7C00 = inf pattern
        if (mantissa != 0) {
            // NaN: set the highest mantissa bit to distinguish from Inf
            result |= 0x0200;   // bit 9 (0x0200) ensures a quiet NaN
        }
        return result;
    }

    // --- Normal float ---------------------------------------------------------
    int32_t half_exp = exp - 127 + 15;   // unbias float, rebias to half

    if (half_exp >= 31) {
        // Exponent too large → infinity (with sign)
        return (sign << 15) | 0x7C00;
    }

    if (half_exp <= 0) {
        // Value is too small for half normal → become denormal or zero
        // We need to shift the mantissa (including the implicit 1) to fit in 10 bits.
        // The float significand = (1 << 23) | mantissa (since exp > 0 here)
        uint32_t sig = (1 << 23) | mantissa;   // 24-bit significand (1.xxxx)
        // For denormal half, the value = 2^(-14) * (mh / 1024)
        // The float value = 2^(exp-127) * (sig / 2^23)
        // We want: 2^(exp-127) * sig / 2^23 = 2^(-14) * mh / 1024
        // => mh = sig * 2^(exp-127 + 14 - 23 + 10) = sig * 2^(exp - 126)
        // Since exp <= 126? Actually half_exp <= 0 implies exp - 127 + 15 <= 0 => exp <= 112.
        // Let's compute shift = 1 - half_exp (the number of right shifts needed to bring sig into the 10-bit range)
        // But we also need to round.
        int shift = 1 - half_exp;   // shift amount (>=1)
        uint32_t mantissa_half = sig >> shift;
        // Round to nearest even: check the bits we are discarding
        uint32_t round_bit = (sig >> (shift - 1)) & 1;
        uint32_t sticky = (sig & ((1u << (shift - 1)) - 1)) != 0;
        // Tie: if round_bit is 1 and sticky is 0 (exact half), round to even
        if (round_bit && (sticky || (mantissa_half & 1))) {
            mantissa_half++;
        }
        // After rounding, we might overflow into exponent 1 (normal)
        if (mantissa_half >> 10) {
            // The value rounded up to the smallest normal (2^(-14) * 1.0)
            return (sign << 15) | 0x0400;   // exponent = 1, mantissa = 0
        }
        // Denormal: exponent field = 0, mantissa = mantissa_half (10 bits)
        return (sign << 15) | (mantissa_half & 0x3FF);
    }

    // --- Normal half (1 <= half_exp <= 30) ------------------------------------
    // We have 10 bits of mantissa, we need to round the 23-bit float mantissa to 10 bits.
    uint32_t mantissa_half = mantissa >> 13;        // take top 10 bits
    // Round to nearest even: check bits 12..0 of the original mantissa
    uint32_t round_bit = (mantissa >> 12) & 1;      // bit 12 (0-indexed) is the first discarded bit
    uint32_t sticky = (mantissa & ((1u << 12) - 1)) != 0; // any lower bits set?
    // Tie: if round_bit is 1 and sticky is 0, round to even
    if (round_bit && (sticky || (mantissa_half & 1))) {
        mantissa_half++;
    }

    // After rounding, mantissa_half might overflow into the exponent
    if (mantissa_half >> 10) {
        mantissa_half = 0;
        half_exp++;
        if (half_exp >= 31) {
            // Overflow → infinity
            return (sign << 15) | 0x7C00;
        }
    }

    return (sign << 15) | (half_exp << 10) | (mantissa_half & 0x3FF);
}
/**
 * Convert a 16-bit half-precision float (IEEE 754) to a 32-bit single-precision float.
 *
 * \param h  The half-precision value stored as a 16-bit unsigned integer.
 * \return   The converted float value.
 */
float alm::HalfToFloat(uint16_t h)
{
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;          // 10 bits

    uint32_t result;
    if (exponent == 0)
    {
        // Zero or denormal
        if (mantissa == 0)
        {
            // Signed zero
            result = sign << 31;
        }
        else
        {
            // Denormal: value = 2^(-14) * (mantissa / 1024) = 2^(-24) * mantissa
            // In float: exponent = -24 + 127 = 103, mantissa shifted left by 13
            result = (sign << 31) | (103 << 23) | (mantissa << 13);
        }
    }
    else if (exponent == 31)
    {
        // Infinity or NaN
        // mantissa != 0 => NaN, else Inf
        uint32_t float_exp = 255 << 23;
        uint32_t float_mant = mantissa << 13;  // Preserve some NaN bits
        result = (sign << 31) | float_exp | float_mant;
    }
    else
    {
        // Normal: value = (-1)^sign * 2^(exponent-15) * (1 + mantissa/1024)
        int32_t float_exp = (int32_t)exponent - 15 + 127;
        result = (sign << 31) | (float_exp << 23) | (mantissa << 13);
    }

    float f;
    memcpy(&f, &result, sizeof(f));
    return f;
}

// Octahedron decoding
float3 alm::DecodeNormal(float2 enc)
{
    enc = enc * 2.f - 1.f;
    float3 n = float3{ enc.x, enc.y, 1.0f - std::abs(enc.x) - std::abs(enc.y) };
    float t = std::clamp(-n.z, 0.f, 1.f);

    n.x += n.x >= 0.f ? -t : t;
    n.y += n.y >= 0.f ? -t : t;

    return glm::normalize(n);
}