#pragma once

#include "Core/Core.h"

namespace st::log
{
    enum class Severity : uint8_t
    {
        None = 0,
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    using Callback = std::function<void(Severity, std::string_view msg)>;

    void SetCallback(Callback cb);
    void ResetCallback();

    void Message(Severity severity, std::string_view msg);

    template<typename... Args>
    void Message(Severity severity, std::string_view fmt, Args&&... args)
    {
        std::string msg = std::vformat(fmt, std::make_format_args(args...));
        Message(severity, msg.c_str());
    }
    
    template<typename... Args>
    void Debug(std::string_view fmt, Args&&... args)
    {
        Message(Severity::Debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(std::string_view fmt, Args&&... args)
    {
        Message(Severity::Info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warning(std::string_view fmt, Args&&... args)
    {
        Message(Severity::Warning, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(std::string_view fmt, Args&&... args)
    {
        Message(Severity::Error, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Fatal(std::string_view fmt, Args&&... args)
    {
        Message(Severity::Fatal, fmt, std::forward<Args>(args)...);
    }
}