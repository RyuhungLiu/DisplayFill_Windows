#include "pch.h"
#include "PipeClient.h"

#include <filesystem>
#include <format>
#include <sstream>
#include <string_view>

namespace
{
    constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\DisplayFill_Windows_Control";

    bool Contains(std::string_view text, std::string_view token)
    {
        return text.find(token) != std::string_view::npos;
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
        if (valueStart < text.size() && text[valueStart] == '"')
        {
            ++valueStart;
            valueEnd = text.find('"', valueStart);
            if (valueEnd == std::string_view::npos)
            {
                return false;
            }
        }
        else
        {
            while (valueEnd < text.size() && text[valueEnd] != ',' && text[valueEnd] != '}' && text[valueEnd] != '\r' && text[valueEnd] != '\n')
            {
                ++valueEnd;
            }
        }

        value.assign(text.substr(valueStart, valueEnd - valueStart));
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }
        return !value.empty();
    }

    double ExtractDoubleOr(std::string_view text, std::string_view key, double fallback)
    {
        std::string raw;
        if (!ExtractRawValue(text, key, raw))
        {
            return fallback;
        }

        try
        {
            return std::stod(raw);
        }
        catch (...)
        {
            return fallback;
        }
    }

    bool ExtractBoolOr(std::string_view text, std::string_view key, bool fallback)
    {
        std::string raw;
        if (!ExtractRawValue(text, key, raw))
        {
            return fallback;
        }

        return raw == "true" || raw == "1";
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (size <= 1)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(size - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
        return result;
    }
}

bool PipeClient::EnsureEngineRunningAndConnected()
{
    if (TryConnectOnce())
    {
        return true;
    }

    StartEngineProcess();

    for (int i = 0; i < 60; ++i)
    {
        if (TryConnectOnce())
        {
            return true;
        }
        Sleep(100);
    }

    return false;
}

std::optional<EngineState> PipeClient::GetState()
{
    const std::string response = SendJson("{\"type\":\"getState\"}");
    if (response.empty() || !Contains(response, "\"state\""))
    {
        return std::nullopt;
    }

    return ParseState(response);
}

bool PipeClient::IsEngineRunning()
{
    return TryConnectOnce();
}

bool PipeClient::SetNumber(const char* key, double value)
{
    const std::string json = std::format("{{\"type\":\"set\",\"key\":\"{}\",\"value\":{}}}", key, value);
    return Contains(SendJson(json), "\"ok\":true");
}

bool PipeClient::SetBoolean(const char* key, bool value)
{
    const std::string json = std::format("{{\"type\":\"set\",\"key\":\"{}\",\"value\":{}}}", key, value ? "true" : "false");
    return Contains(SendJson(json), "\"ok\":true");
}

bool PipeClient::SetString(const char* key, const char* value)
{
    const std::string json = std::format("{{\"type\":\"set\",\"key\":\"{}\",\"value\":\"{}\"}}", key, value);
    return Contains(SendJson(json), "\"ok\":true");
}

bool PipeClient::SendCommand(const char* name)
{
    const std::string json = std::format("{{\"type\":\"command\",\"name\":\"{}\"}}", name);
    return Contains(SendJson(json), "\"ok\":true");
}

bool PipeClient::TryConnectOnce()
{
    HANDLE pipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    CloseHandle(pipe);
    return true;
}

bool PipeClient::StartEngineProcess()
{
    const std::wstring enginePath = GetEnginePath();
    if (enginePath.empty())
    {
        return false;
    }

    std::filesystem::path engineFile(enginePath);
    std::wstring commandLine = L"\"" + enginePath + L"\"";

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};

    const BOOL ok = CreateProcessW(
        enginePath.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        nullptr,
        engineFile.parent_path().c_str(),
        &startupInfo,
        &processInfo);

    if (ok)
    {
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return true;
    }

    return false;
}

std::string PipeClient::SendJson(const std::string& json)
{
    HANDLE pipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return {};
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(pipe, json.data(), static_cast<DWORD>(json.size()), &bytesWritten, nullptr))
    {
        CloseHandle(pipe);
        return {};
    }

    FlushFileBuffers(pipe);

    char buffer[8192] = {};
    DWORD bytesRead = 0;
    std::string response;
    if (ReadFile(pipe, buffer, static_cast<DWORD>(sizeof(buffer) - 1), &bytesRead, nullptr) && bytesRead > 0)
    {
        response.assign(buffer, buffer + bytesRead);
    }

    CloseHandle(pipe);
    return response;
}

std::optional<EngineState> PipeClient::ParseState(const std::string& json) const
{
    EngineState state;
    state.targetNits = static_cast<float>(ExtractDoubleOr(json, "targetNits", state.targetNits));
    state.frameMarginXRatio = static_cast<float>(ExtractDoubleOr(json, "frameMarginXRatio", state.frameMarginXRatio));
    state.frameMarginYRatio = static_cast<float>(ExtractDoubleOr(json, "frameMarginYRatio", state.frameMarginYRatio));
    state.cornerRadius = static_cast<int>(ExtractDoubleOr(json, "cornerRadius", state.cornerRadius));
    state.visualCornerFeatherPixels = static_cast<float>(ExtractDoubleOr(json, "visualCornerFeatherPixels", state.visualCornerFeatherPixels));
    state.normalAlpha = static_cast<int>(ExtractDoubleOr(json, "normalAlpha", state.normalAlpha));
    state.hoverAlpha = static_cast<int>(ExtractDoubleOr(json, "hoverAlpha", state.hoverAlpha));
    state.hoverTransitionSeconds = static_cast<float>(ExtractDoubleOr(json, "hoverTransitionSeconds", state.hoverTransitionSeconds));
    state.startupBreathMinBrightness = static_cast<float>(ExtractDoubleOr(json, "startupBreathMinBrightness", state.startupBreathMinBrightness));
    state.startupBreathMaxBrightness = static_cast<float>(ExtractDoubleOr(json, "startupBreathMaxBrightness", state.startupBreathMaxBrightness));
    state.startupBreathDurationSeconds = static_cast<float>(ExtractDoubleOr(json, "startupBreathDurationSeconds", state.startupBreathDurationSeconds));
    state.passThroughMode = ExtractBoolOr(json, "passThroughMode", state.passThroughMode);
    state.hdrActive = ExtractBoolOr(json, "hdrActive", state.hdrActive);

    std::string language;
    if (ExtractRawValue(json, "language", language))
    {
        state.language = Utf8ToWide(language);
    }

    return state;
}

std::wstring PipeClient::GetEnginePath() const
{
    wchar_t modulePathBuffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePathBuffer, MAX_PATH);
    const std::filesystem::path modulePath(modulePathBuffer);

    const std::filesystem::path sameDirectory = modulePath.parent_path() / L"DisplayFill_Windows.exe";
    if (std::filesystem::exists(sameDirectory))
    {
        return sameDirectory.wstring();
    }

    const std::filesystem::path developmentPath = modulePath.parent_path() / L".." / L".." / L".." / L".." / L"DisplayFill_Windows" / L"x64" / L"Debug" / L"DisplayFill_Windows.exe";
    if (std::filesystem::exists(developmentPath))
    {
        return std::filesystem::weakly_canonical(developmentPath).wstring();
    }

    const std::filesystem::path arm64DevelopmentPath = modulePath.parent_path() / L".." / L".." / L".." / L".." / L"DisplayFill_Windows" / L"ARM64" / L"Debug" / L"DisplayFill_Windows.exe";
    if (std::filesystem::exists(arm64DevelopmentPath))
    {
        return std::filesystem::weakly_canonical(arm64DevelopmentPath).wstring();
    }

    return {};
}
