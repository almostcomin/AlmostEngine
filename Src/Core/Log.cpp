#include "Core/Log.h"
#include <mutex>
#include <unordered_set>

#if _WIN32
#include <Windows.h>
#endif

namespace st::log
{

void DefaultCallback(Severity severity, std::string_view msg, std::string_view file, int line); // forward decl

Callback g_Callback = DefaultCallback;
bool g_OutputToDebug = true;
bool g_OutputToMessageBox = true;
bool g_OutputToConsole = false;
std::unordered_set<std::string> g_IgnoredMessages;

constexpr size_t g_MessageBufferSize = 4096;
std::mutex g_LogMutex;


void DefaultCallback(Severity severity, std::string_view msg, std::string_view file, int line)
{
    std::string_view severityText;
    switch (severity)
    {
    case Severity::Debug:   severityText = "DEBUG";         break;
    case Severity::Info:    severityText = "INFO";          break;
    case Severity::Warning: severityText = "WARNING";       break;
    case Severity::Error:   severityText = "ERROR";         break;
    case Severity::Fatal:   severityText = "FATAL ERROR";   break;
    default:
        break;
    }

    std::string actualMsg = file.empty() ?
        std::format("{}: {}", severityText, msg) : std::format("{}({}): {}: {}", file, line, severityText, msg);

    {
        std::lock_guard<std::mutex> lockGuard(g_LogMutex);

#if _WIN32
        if (g_OutputToDebug)
        {
            OutputDebugStringA(actualMsg.c_str());
            OutputDebugStringA("\n");
        }

        if (g_OutputToMessageBox)
        {
            if (severity == Severity::Error || severity == Severity::Fatal)
            {
                std::string id = std::format("{}({})", file, line);
                if (g_IgnoredMessages.find(id) == g_IgnoredMessages.end())
                {
                    int ret = MessageBoxA(0, actualMsg.c_str(), severityText.data(), MB_ABORTRETRYIGNORE | MB_ICONERROR);
                    switch (ret)
                    {
                    case IDABORT:
                        __debugbreak();
                        break;
                    case IDRETRY:
                        break;
                    case IDIGNORE:
                        g_IgnoredMessages.insert(id);
                        break;
                    }
                }
            }
        }

#endif
        if (g_OutputToConsole)
        {
            if (severity == Severity::Error || severity == Severity::Fatal)
                fprintf(stderr, "%s\n", actualMsg.c_str());
            else
                fprintf(stdout, "%s\n", actualMsg.c_str());
        }
    }

    if (severity == Severity::Fatal)
        abort();
}

void SetCallback(Callback cb)
{
    g_Callback = cb;
}

void ResetCallback()
{
    g_Callback = DefaultCallback;
}

void Message(Severity severity, std::string_view msg, std::string_view file, int line)
{
    g_Callback(severity, msg, file, line);
}

} // namespace st::log