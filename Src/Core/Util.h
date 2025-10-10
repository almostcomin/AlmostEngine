#pragma once

#include <memory>
#include <string>
#include <string_view>

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

inline std::string_view GetFilenameFromPath(const std::string& path)
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
};

inline std::string_view GetExtensionFromPath(const std::string& path)
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

} // namespace st