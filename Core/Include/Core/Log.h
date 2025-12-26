#pragma once

#include <format>
#include <functional>

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

    using Callback = std::function<void(Severity, std::string_view msg, std::string_view file, int line)>;

    void SetCallback(Callback cb);
    void ResetCallback();

    void Message(Severity severity, std::string_view msg, std::string_view file, int line);

    template<typename... Args>
    void Message(Severity severity, std::string_view file, int line, std::string_view fmt, Args&&... args)
    {
        std::string msg = std::vformat(fmt, std::make_format_args(args...));
        Message(severity, msg.c_str(), file, line);
    }
    
    template<typename... Args>
    void Debug(std::string_view file, int line, std::string_view fmt, Args&&... args)
    {
        Message(Severity::Debug, file, line, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(std::string_view file, int line, std::string_view fmt, Args&&... args)
    {
        Message(Severity::Info, file, line, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(std::string_view fmt, Args&&... args)
    {
        Message(Severity::Info, {}, -1, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warning(std::string_view file, int line, std::string_view fmt, Args&&... args)
    {
        Message(Severity::Warning, file, line, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(std::string_view file, int line, std::string_view fmt, Args&&... args)
    {
        Message(Severity::Error, file, line, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Fatal(std::string_view file, int line, std::string_view fmt, Args&&... args)
    {
        Message(Severity::Fatal, file, line, fmt, std::forward<Args>(args)...);
    }
}

#define LOG(severity, fmt, ...) \
    Message(severity, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) LOG(st::log::Severity::Debug, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG(st::log::Severity::Info, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG(st::log::Severity::Warning, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(st::log::Severity::Error, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG(st::log::Severity::Fatal, fmt, ##__VA_ARGS__)