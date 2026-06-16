#include "HDRDriver.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <string_view>

namespace hdr_driver
{
namespace
{
    std::string ToUtf8(const wchar_t* text)
    {
        if (!text)
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
        {
            return {};
        }

        std::string result(static_cast<size_t>(size - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
        return result;
    }

    bool Contains(std::string_view text, std::string_view token)
    {
        return text.find(token) != std::string_view::npos;
    }

    bool ExtractStringValue(std::string_view text, std::string_view key, std::string& value)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = text.find(quotedKey);
        if (keyPos == std::string_view::npos)
        {
            return false;
        }

        const size_t colonPos = text.find(':', keyPos + quotedKey.size());
        if (colonPos == std::string_view::npos)
        {
            return false;
        }

        const size_t firstQuote = text.find('"', colonPos + 1);
        if (firstQuote == std::string_view::npos)
        {
            return false;
        }

        const size_t secondQuote = text.find('"', firstQuote + 1);
        if (secondQuote == std::string_view::npos)
        {
            return false;
        }

        value.assign(text.substr(firstQuote + 1, secondQuote - firstQuote - 1));
        return true;
    }

    std::string EscapeJsonString(std::string_view value)
    {
        std::ostringstream stream;
        for (const unsigned char ch : value)
        {
            switch (ch)
            {
            case '\\':
                stream << "\\\\";
                break;
            case '"':
                stream << "\\\"";
                break;
            case '\r':
                stream << "\\r";
                break;
            case '\n':
                stream << "\\n";
                break;
            case '\t':
                stream << "\\t";
                break;
            default:
                if (ch < 0x20)
                {
                    stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec;
                }
                else
                {
                    stream << static_cast<char>(ch);
                }
                break;
            }
        }
        return stream.str();
    }

    bool ExtractEscapedStringValue(std::string_view text, std::string_view key, std::string& value)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = text.find(quotedKey);
        if (keyPos == std::string_view::npos)
        {
            return false;
        }

        const size_t colonPos = text.find(':', keyPos + quotedKey.size());
        const size_t firstQuote = text.find('"', colonPos == std::string_view::npos ? keyPos : colonPos + 1);
        if (colonPos == std::string_view::npos || firstQuote == std::string_view::npos)
        {
            return false;
        }

        value.clear();
        for (size_t i = firstQuote + 1; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (ch == '"')
            {
                return true;
            }
            if (ch == '\\' && i + 1 < text.size())
            {
                const char escaped = text[++i];
                switch (escaped)
                {
                case 'r':
                    value.push_back('\r');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case '\\':
                case '"':
                    value.push_back(escaped);
                    break;
                default:
                    value.push_back(escaped);
                    break;
                }
            }
            else
            {
                value.push_back(ch);
            }
        }
        return false;
    }

    bool ExtractRawValue(std::string_view text, std::string_view key, std::string& value)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = text.find(quotedKey);
        if (keyPos == std::string_view::npos)
        {
            return false;
        }

        const size_t colonPos = text.find(':', keyPos + quotedKey.size());
        if (colonPos == std::string_view::npos)
        {
            return false;
        }

        size_t valueStart = colonPos + 1;
        while (valueStart < text.size() && std::isspace(static_cast<unsigned char>(text[valueStart])))
        {
            ++valueStart;
        }

        size_t valueEnd = valueStart;
        while (valueEnd < text.size() && text[valueEnd] != ',' && text[valueEnd] != '}' && text[valueEnd] != '\r' && text[valueEnd] != '\n')
        {
            ++valueEnd;
        }

        value.assign(text.substr(valueStart, valueEnd - valueStart));
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }
        return !value.empty();
    }

    bool ExtractDoubleValue(std::string_view text, std::string_view key, double& value)
    {
        std::string raw;
        if (!ExtractRawValue(text, key, raw))
        {
            return false;
        }

        try
        {
            value = std::stod(raw);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ExtractBoolValue(std::string_view text, std::string_view key, bool& value)
    {
        std::string raw;
        if (!ExtractRawValue(text, key, raw))
        {
            return false;
        }

        std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });

        if (raw == "true" || raw == "1")
        {
            value = true;
            return true;
        }
        if (raw == "false" || raw == "0")
        {
            value = false;
            return true;
        }
        return false;
    }

    AppLanguage ParseLanguage(std::string_view value)
    {
        if (value == "zh-Hant" || value == "zh-TW" || value == "traditional")
        {
            return AppLanguage::ZhHant;
        }
        if (value == "en-US" || value == "en" || value == "english")
        {
            return AppLanguage::EnUs;
        }
        return AppLanguage::ZhHans;
    }

    const char* LanguageToString(AppLanguage language)
    {
        switch (language)
        {
        case AppLanguage::ZhHant:
            return "zh-Hant";
        case AppLanguage::EnUs:
            return "en-US";
        case AppLanguage::ZhHans:
        default:
            return "zh-Hans";
        }
    }

    AppThemeMode ParseThemeMode(std::string_view value)
    {
        if (value == "light")
        {
            return AppThemeMode::Light;
        }
        if (value == "dark")
        {
            return AppThemeMode::Dark;
        }
        return AppThemeMode::System;
    }

    const char* ThemeModeToString(AppThemeMode mode)
    {
        switch (mode)
        {
        case AppThemeMode::Light:
            return "light";
        case AppThemeMode::Dark:
            return "dark";
        case AppThemeMode::System:
        default:
            return "system";
        }
    }

    AppBackdropKind ParseBackdropKind(std::string_view value)
    {
        if (value == "mica")
        {
            return AppBackdropKind::Mica;
        }
        if (value == "solid")
        {
            return AppBackdropKind::Solid;
        }
        return AppBackdropKind::Acrylic;
    }

    const char* BackdropKindToString(AppBackdropKind kind)
    {
        switch (kind)
        {
        case AppBackdropKind::Mica:
            return "mica";
        case AppBackdropKind::Solid:
            return "solid";
        case AppBackdropKind::Acrylic:
        default:
            return "acrylic";
        }
    }

    void CopyKey(wchar_t (&destination)[64], const std::string& key)
    {
        const int size = MultiByteToWideChar(CP_UTF8, 0, key.c_str(), -1, destination, static_cast<int>(std::size(destination)));
        if (size <= 0)
        {
            destination[0] = L'\0';
        }
    }
}

IpcServer::IpcServer(HWND targetWindow, AppState* appState)
    : m_targetWindow(targetWindow), m_appState(appState)
{
}

IpcServer::~IpcServer()
{
    Stop();
}

bool IpcServer::Start()
{
    if (m_running.exchange(true))
    {
        return true;
    }

    try
    {
        m_worker = std::thread(&IpcServer::WorkerLoop, this);
        return true;
    }
    catch (...)
    {
        m_running = false;
        return false;
    }
}

void IpcServer::Stop()
{
    if (!m_running.exchange(false))
    {
        return;
    }

    HANDLE wakePipe = CreateFileW(kIpcPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (wakePipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(wakePipe);
    }

    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void IpcServer::WorkerLoop()
{
    std::printf("IPC server listening: %s\n", ToUtf8(kIpcPipeName).c_str());

    while (m_running)
    {
        HANDLE pipeHandle = CreateNamedPipeW(
            kIpcPipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            8192,
            8192,
            0,
            nullptr);

        if (pipeHandle == INVALID_HANDLE_VALUE)
        {
            std::printf("CreateNamedPipeW failed. GetLastError=%lu\n", GetLastError());
            Sleep(250);
            continue;
        }

        const BOOL connected = ConnectNamedPipe(pipeHandle, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected && m_running)
        {
            HandleClient(pipeHandle);
        }

        DisconnectNamedPipe(pipeHandle);
        CloseHandle(pipeHandle);
    }
}

void IpcServer::HandleClient(HANDLE pipeHandle)
{
    char buffer[8192] = {};
    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(pipeHandle, buffer, static_cast<DWORD>(sizeof(buffer) - 1), &bytesRead, nullptr);
    if (!readOk || bytesRead == 0)
    {
        return;
    }

    std::string message(buffer, buffer + bytesRead);
    std::string response = HandleMessage(message);
    if (response.empty())
    {
        response = "{\"ok\":false,\"error\":\"empty response\"}";
    }

    DWORD bytesWritten = 0;
    WriteFile(pipeHandle, response.data(), static_cast<DWORD>(response.size()), &bytesWritten, nullptr);
    FlushFileBuffers(pipeHandle);
}

std::string IpcServer::HandleMessage(const std::string& message)
{
    if (Contains(message, "\"type\""))
    {
        if (Contains(message, "\"getState\""))
        {
            return BuildStateJson();
        }

        if (Contains(message, "\"getConfig\""))
        {
            return std::string("{\"ok\":true,\"content\":\"") + EscapeJsonString(ReadSettingsIniUtf8()) + "\"}";
        }

        if (Contains(message, "\"setConfig\""))
        {
            std::string content;
            if (!ExtractEscapedStringValue(message, "content", content) || !m_appState)
            {
                return "{\"ok\":false,\"error\":\"invalid config content\"}";
            }
            const bool ok = WriteSettingsIniUtf8(content, m_appState->settings);
            if (ok)
            {
                m_appState->UpdateMarginsFromSettings();
                m_appState->renderNeeded = true;
                IpcCommand command;
                command.commandType = IpcCommandType::ReloadConfig;
                PostCommand(command);
            }
            return ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"write failed\"}";
        }

        IpcCommand command;
        if (TryCreateCommand(message, command))
        {
            PostCommand(command);
            return "{\"ok\":true}";
        }
    }

    return "{\"ok\":false,\"error\":\"unsupported message\"}";
}

std::string IpcServer::BuildStateJson() const
{
    if (!m_appState)
    {
        return "{\"ok\":false,\"error\":\"state unavailable\"}";
    }

    const AppSettings& settings = m_appState->settings;
    std::ostringstream json;
    json << "{"
        << "\"type\":\"state\"," 
        << "\"targetNits\":" << settings.targetNits << ","
        << "\"frameMarginXRatio\":" << settings.frameMarginXRatio << ","
        << "\"frameMarginYRatio\":" << settings.frameMarginYRatio << ","
        << "\"cornerRadius\":" << settings.holeCornerRadius << ","
        << "\"visualCornerFeatherPixels\":" << settings.visualCornerFeatherPixels << ","
        << "\"outerCornerRadius\":" << settings.outerCornerRadius << ","
        << "\"screenInsetPixels\":" << settings.screenInsetPixels << ","
        << "\"centerBrightnessBoost\":" << settings.centerBrightnessBoost << ","
        << "\"colorTemperatureShift\":" << settings.colorTemperatureShift << ","
        << "\"colorTintShift\":" << settings.colorTintShift << ","
        << "\"shadowStrength\":" << settings.shadowStrength << ","
        << "\"shadowSizePixels\":" << settings.shadowSizePixels << ","
        << "\"normalAlpha\":" << static_cast<int>(settings.normalWindowAlpha) << ","
        << "\"hoverAlpha\":" << static_cast<int>(settings.mouseHoverWindowAlpha) << ","
        << "\"hoverTransitionSeconds\":" << settings.hoverOpacityTransitionSeconds << ","
        << "\"startupBreathMinBrightness\":" << settings.startupBreathMinBrightness << ","
        << "\"startupBreathMaxBrightness\":" << settings.startupBreathMaxBrightness << ","
        << "\"startupBreathDurationSeconds\":" << settings.startupBreathDurationSeconds << ","
        << "\"passThroughMode\":" << (settings.passThroughMode ? "true" : "false") << ","
        << "\"language\":\"" << LanguageToString(settings.language) << "\"," 
        << "\"themeMode\":\"" << ThemeModeToString(settings.themeMode) << "\","
        << "\"backdropKind\":\"" << BackdropKindToString(settings.backdropKind) << "\","
        << "\"rendererReady\":" << (m_appState->rendererReady ? "true" : "false") << ","
        << "\"hdrActive\":" << (m_appState->hdrActive ? "true" : "false")
        << "}";
    return json.str();
}

bool IpcServer::TryCreateCommand(const std::string& message, IpcCommand& command) const
{
    if (Contains(message, "\"command\""))
    {
        std::string name;
        if (!ExtractStringValue(message, "name", name))
        {
            return false;
        }

        if (name == "exit")
        {
            command.commandType = IpcCommandType::Exit;
            return true;
        }
        if (name == "togglePassThrough")
        {
            command.commandType = IpcCommandType::TogglePassThrough;
            return true;
        }
        return false;
    }

    if (!Contains(message, "\"set\"") && !Contains(message, "\"setMany\""))
    {
        return false;
    }

    std::string key;
    if (!ExtractStringValue(message, "key", key))
    {
        return false;
    }

    command.commandType = IpcCommandType::SetValue;
    CopyKey(command.key, key);

    if (key == "passThroughMode")
    {
        command.valueType = IpcValueType::Boolean;
        return ExtractBoolValue(message, "value", command.boolValue);
    }

    if (key == "language")
    {
        std::string value;
        if (!ExtractStringValue(message, "value", value))
        {
            return false;
        }
        command.valueType = IpcValueType::Language;
        command.languageValue = ParseLanguage(value);
        return true;
    }

    if (key == "themeMode")
    {
        std::string value;
        if (!ExtractStringValue(message, "value", value))
        {
            return false;
        }
        command.valueType = IpcValueType::ThemeMode;
        command.themeModeValue = ParseThemeMode(value);
        return true;
    }

    if (key == "backdropKind")
    {
        std::string value;
        if (!ExtractStringValue(message, "value", value))
        {
            return false;
        }
        command.valueType = IpcValueType::BackdropKind;
        command.backdropKindValue = ParseBackdropKind(value);
        return true;
    }

    command.valueType = IpcValueType::Number;
    return ExtractDoubleValue(message, "value", command.numberValue);
}

void IpcServer::PostCommand(const IpcCommand& command) const
{
    if (!m_targetWindow)
    {
        return;
    }

    IpcCommand* heapCommand = new IpcCommand(command);
    if (!PostMessageW(m_targetWindow, kIpcApplyMessage, 0, reinterpret_cast<LPARAM>(heapCommand)))
    {
        delete heapCommand;
    }
}

} // namespace hdr_driver
