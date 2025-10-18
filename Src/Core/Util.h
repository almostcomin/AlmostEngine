#pragma once

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

std::string_view GetFilenameFromPath(const std::string_view& path);
std::string_view GetExtensionFromPath(const std::string_view& path);

// Based on 128 bits number
std::string MakeUniqueStringId();

} // namespace st