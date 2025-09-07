#include "Core/Log.h"
#include <mutex>
#if _WIN32
#include <Windows.h>
#endif

namespace st::log
{

void DefaultCallback(Severity severity, std::string_view msg); // forward decl

Callback g_Callback = DefaultCallback;
bool g_OutputToDebug = true;
bool g_OutputToMessageBox = true;
bool g_OutputToConsole = false;

std::string g_ErrorMessageCaption = "Error";

constexpr size_t g_MessageBufferSize = 4096;
std::mutex g_LogMutex;


void DefaultCallback(Severity severity, std::string_view msg)
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

    std::string actualMsg = std::format("{}: {}", severityText, msg);

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
                MessageBoxA(0, actualMsg.c_str(), g_ErrorMessageCaption.c_str(), MB_ICONERROR);
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

void Message(Severity severity, std::string_view msg)
{
    g_Callback(severity, msg);
}

} // namespace st::log