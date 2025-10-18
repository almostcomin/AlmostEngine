#include "Util.h"
#include <chrono>
#include <random>

std::string_view st::GetFilenameFromPath(const std::string& path)
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

std::string_view st::GetExtensionFromPath(const std::string& path)
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