#include "Core/CorePCH.h"
#include "Core/Log.h"

namespace
{

void DefaultCallback(alm::log::Severity severity, std::string_view msg, std::string_view file, int line); // forward decl

alm::log::Callback g_Callback = DefaultCallback;
bool g_OutputToDebug = true;
bool g_OutputToMessageBox = true;
bool g_OutputToConsole = false;
std::unordered_set<std::string> g_IgnoredMessages;
std::mutex g_LogMutex;

void DefaultCallback(alm::log::Severity severity, std::string_view msg, std::string_view file, int line)
{
    std::string_view severityText;
    switch (severity)
    {
    case alm::log::Severity::Debug:   severityText = "DEBUG";         break;
    case alm::log::Severity::Info:    severityText = "INFO";          break;
    case alm::log::Severity::Warning: severityText = "WARNING";       break;
    case alm::log::Severity::Error:   severityText = "ERROR";         break;
    case alm::log::Severity::Fatal:   severityText = "FATAL ERROR";   break;
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
            if (severity == alm::log::Severity::Error || severity == alm::log::Severity::Fatal)
            {
                std::string id = std::format("{}({})", file, line);
                if (g_IgnoredMessages.find(id) == g_IgnoredMessages.end())
                {
                    int ret = MessageBoxA(0, actualMsg.c_str(), severityText.data(), MB_ABORTRETRYIGNORE | MB_ICONERROR);
                    switch (ret)
                    {
                    case IDABORT:
                        break;
                    case IDRETRY:
                        __debugbreak();
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
            if (severity == alm::log::Severity::Error || severity == alm::log::Severity::Fatal)
                fprintf(stderr, "%s\n", actualMsg.c_str());
            else
                fprintf(stdout, "%s\n", actualMsg.c_str());
        }
    }

    if (severity == alm::log::Severity::Fatal)
        abort();
}

} // anonimous namespace

void alm::log::SetCallback(Callback cb)
{
    g_Callback = cb;
}

void alm::log::ResetCallback()
{
    g_Callback = DefaultCallback;
}

void alm::log::Message(Severity severity, std::string_view msg, std::string_view file, int line)
{
    g_Callback(severity, msg, file, line);
}
