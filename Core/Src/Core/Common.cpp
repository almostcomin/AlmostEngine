#include "Core/CorePCH.h"
#include "Core/Common.h"

std::string_view st::GetFilenameFromPath(const std::string_view& path)
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

std::string_view st::GetExtensionFromPath(const std::string_view& path)
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

std::string st::MakeUniqueStringId()
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

std::wstring st::ToWide(const char* utf8)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring wide(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), size);
    return wide;
}

std::string st::ToUtf8(const wchar_t* wide)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
        return {};
    std::string utf8(size_needed - 1, 0); // -1 to remove last '\0'
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), size_needed, nullptr, nullptr);
    return utf8;
}

// Convert float (32 bits) to half-float (16 bits) (IEEE 754)
uint16_t st::FloatToHalf(float f)
{
    uint32_t i = *((uint32_t*)&f);
    uint32_t s = (i >> 16) & 0x00008000;
    uint32_t e = ((i >> 23) & 0x000000ff) - (127 - 15);
    uint32_t m = i & 0x007fffff;
    if (e <= 0) return (uint16_t)s;
    if (e >= 31) return (uint16_t)(s | 0x7c00);
    return (uint16_t)(s | (e << 10) | (m >> 13));
}