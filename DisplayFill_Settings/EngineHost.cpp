#include "pch.h"
#include "EngineHost.h"
#include "../DisplayFill_Windows/HDRDriver.h"

#include <chrono>
#include <memory>
#include <string_view>

namespace
{
    constexpr auto kStartupTimeout = std::chrono::seconds(8);

    std::wstring LanguageToString(hdr_driver::AppLanguage language)
    {
        switch (language)
        {
        case hdr_driver::AppLanguage::ZhHant:
            return L"zh-Hant";
        case hdr_driver::AppLanguage::EnUs:
            return L"en-US";
        case hdr_driver::AppLanguage::ZhHans:
        default:
            return L"zh-Hans";
        }
    }

    hdr_driver::AppLanguage ParseLanguage(std::string_view value)
    {
        if (value == "zh-Hant" || value == "zh-TW" || value == "traditional")
        {
            return hdr_driver::AppLanguage::ZhHant;
        }
        if (value == "en-US" || value == "en" || value == "english")
        {
            return hdr_driver::AppLanguage::EnUs;
        }
        return hdr_driver::AppLanguage::ZhHans;
    }

    std::wstring ThemeModeToString(hdr_driver::AppThemeMode mode)
    {
        switch (mode)
        {
        case hdr_driver::AppThemeMode::Light:
            return L"light";
        case hdr_driver::AppThemeMode::Dark:
            return L"dark";
        case hdr_driver::AppThemeMode::System:
        default:
            return L"system";
        }
    }

    hdr_driver::AppThemeMode ParseThemeMode(std::string_view value)
    {
        if (value == "light")
        {
            return hdr_driver::AppThemeMode::Light;
        }
        if (value == "dark")
        {
            return hdr_driver::AppThemeMode::Dark;
        }
        return hdr_driver::AppThemeMode::System;
    }

    std::wstring BackdropKindToString(hdr_driver::AppBackdropKind kind)
    {
        switch (kind)
        {
        case hdr_driver::AppBackdropKind::Mica:
            return L"mica";
        case hdr_driver::AppBackdropKind::Solid:
            return L"solid";
        case hdr_driver::AppBackdropKind::Acrylic:
        default:
            return L"acrylic";
        }
    }

    hdr_driver::AppBackdropKind ParseBackdropKind(std::string_view value)
    {
        if (value == "mica")
        {
            return hdr_driver::AppBackdropKind::Mica;
        }
        if (value == "solid")
        {
            return hdr_driver::AppBackdropKind::Solid;
        }
        return hdr_driver::AppBackdropKind::Acrylic;
    }

    void CopyKey(wchar_t (&destination)[64], const char* key)
    {
        if (!key)
        {
            destination[0] = L'\0';
            return;
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, key, -1, destination, static_cast<int>(_countof(destination)));
        if (size <= 0)
        {
            destination[0] = L'\0';
        }
    }

    EngineState ToEngineState(hdr_driver::AppState const& appState)
    {
        EngineState state;
        auto const& settings = appState.settings;
        state.targetNits = settings.targetNits;
        state.frameMarginXRatio = settings.frameMarginXRatio;
        state.frameMarginYRatio = settings.frameMarginYRatio;
        state.cornerRadius = settings.holeCornerRadius;
        state.visualCornerFeatherPixels = settings.visualCornerFeatherPixels;
        state.outerCornerRadius = settings.outerCornerRadius;
        state.screenInsetPixels = settings.screenInsetPixels;
        state.centerBrightnessBoost = settings.centerBrightnessBoost;
        state.colorTemperatureShift = settings.colorTemperatureShift;
        state.colorTintShift = settings.colorTintShift;
        state.shadowStrength = settings.shadowStrength;
        state.shadowSizePixels = settings.shadowSizePixels;
        state.normalAlpha = settings.normalWindowAlpha;
        state.hoverAlpha = settings.mouseHoverWindowAlpha;
        state.hoverTransitionSeconds = settings.hoverOpacityTransitionSeconds;
        state.startupBreathMinBrightness = settings.startupBreathMinBrightness;
        state.startupBreathMaxBrightness = settings.startupBreathMaxBrightness;
        state.startupBreathDurationSeconds = settings.startupBreathDurationSeconds;
        state.passThroughMode = settings.passThroughMode;
        state.rendererReady = appState.rendererReady;
        state.hdrActive = appState.hdrActive;
        state.language = LanguageToString(settings.language);
        state.themeMode = ThemeModeToString(settings.themeMode);
        state.backdropKind = BackdropKindToString(settings.backdropKind);
        return state;
    }
}

EngineHost::~EngineHost()
{
    Stop();
}

EngineHost::EngineHost()
    : m_appState(std::make_unique<hdr_driver::AppState>())
{
}

void EngineHost::SetSettingsWindow(HWND hwnd)
{
    hdr_driver::SetSettingsWindowHandle(hwnd);
}

bool EngineHost::EnsureEngineRunningAndConnected()
{
    if (IsEngineRunning())
    {
        return true;
    }

    if (!StartEngineThread())
    {
        return false;
    }

    return WaitForStartup() && IsEngineRunning();
}

bool EngineHost::IsEngineRunning() const
{
    HWND hwnd = EngineWindow();
    return m_threadActive.load() && hwnd && IsWindow(hwnd);
}

void EngineHost::Stop()
{
    if (IsEngineRunning())
    {
        SendCommand("exit");
    }

    if (m_worker.joinable() && m_worker.get_id() != std::this_thread::get_id())
    {
        m_worker.join();
    }
}

std::optional<EngineState> EngineHost::GetState() const
{
    if (!IsEngineRunning())
    {
        return std::nullopt;
    }

    return ToEngineState(*m_appState);
}

bool EngineHost::SetNumber(const char* key, double value)
{
    hdr_driver::IpcCommand command;
    command.commandType = hdr_driver::IpcCommandType::SetValue;
    command.valueType = hdr_driver::IpcValueType::Number;
    command.numberValue = value;
    CopyKey(command.key, key);
    return PostCommand(command);
}

bool EngineHost::SetBoolean(const char* key, bool value)
{
    hdr_driver::IpcCommand command;
    command.commandType = hdr_driver::IpcCommandType::SetValue;
    command.valueType = hdr_driver::IpcValueType::Boolean;
    command.boolValue = value;
    CopyKey(command.key, key);
    return PostCommand(command);
}

bool EngineHost::SetString(const char* key, const char* value)
{
    hdr_driver::IpcCommand command;
    command.commandType = hdr_driver::IpcCommandType::SetValue;
    CopyKey(command.key, key);
    const std::string_view keyView(key ? key : "");
    const std::string_view valueView(value ? value : "");
    if (keyView == "themeMode")
    {
        command.valueType = hdr_driver::IpcValueType::ThemeMode;
        command.themeModeValue = ParseThemeMode(valueView);
    }
    else if (keyView == "backdropKind")
    {
        command.valueType = hdr_driver::IpcValueType::BackdropKind;
        command.backdropKindValue = ParseBackdropKind(valueView);
    }
    else
    {
        command.valueType = hdr_driver::IpcValueType::Language;
        command.languageValue = ParseLanguage(valueView);
    }
    return PostCommand(command);
}

bool EngineHost::SendCommand(const char* name)
{
    hdr_driver::IpcCommand command;
    if (std::string_view(name ? name : "") == "exit")
    {
        command.commandType = hdr_driver::IpcCommandType::Exit;
    }
    else if (std::string_view(name ? name : "") == "togglePassThrough")
    {
        command.commandType = hdr_driver::IpcCommandType::TogglePassThrough;
    }
    else
    {
        return false;
    }

    return PostCommand(command);
}

std::optional<std::string> EngineHost::GetConfigText() const
{
    if (!IsEngineRunning())
    {
        return std::nullopt;
    }

    return hdr_driver::ReadSettingsIniUtf8();
}

bool EngineHost::SetConfigText(const std::string& content)
{
    if (!IsEngineRunning())
    {
        return false;
    }

    if (!hdr_driver::WriteSettingsIniUtf8(content, m_appState->settings))
    {
        return false;
    }

    m_appState->UpdateMarginsFromSettings();
    m_appState->renderNeeded = true;

    hdr_driver::IpcCommand command;
    command.commandType = hdr_driver::IpcCommandType::ReloadConfig;
    return PostCommand(command);
}

bool EngineHost::StartEngineThread()
{
    if (m_worker.joinable())
    {
        if (m_threadActive.load())
        {
            return true;
        }

        m_worker.join();
    }

    {
        std::lock_guard lock(m_mutex);
        m_engineWindow = nullptr;
        m_startAttemptComplete = false;
        m_startSucceeded = false;
        m_appState->rendererReady = false;
        m_appState->hdrActive = false;
    }

    try
    {
        m_worker = std::thread(&EngineHost::EngineThreadMain, this);
        return true;
    }
    catch (...)
    {
        std::lock_guard lock(m_mutex);
        m_startAttemptComplete = true;
        m_startSucceeded = false;
        m_startupCondition.notify_all();
        return false;
    }
}

bool EngineHost::WaitForStartup()
{
    std::unique_lock lock(m_mutex);
    const bool completed = m_startupCondition.wait_for(lock, kStartupTimeout, [this]
    {
        return m_startAttemptComplete;
    });

    return completed && m_startSucceeded;
}

void EngineHost::EngineThreadMain()
{
    m_threadActive = true;
    m_appState->rendererReady = false;
    m_appState->hdrActive = false;

    auto renderer = std::make_unique<hdr_driver::Renderer>();
    auto windowManager = std::make_unique<hdr_driver::WindowManager>();

    hdr_driver::LoadSettingsFromIni(m_appState->settings);

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const bool initialized = instance && windowManager->Initialize(instance, m_appState.get(), renderer.get());

    {
        std::lock_guard lock(m_mutex);
        m_engineWindow = initialized ? windowManager->GetWindowHandle() : nullptr;
        m_startSucceeded = initialized && m_engineWindow != nullptr;
        m_startAttemptComplete = true;
    }
    m_startupCondition.notify_all();

    if (initialized)
    {
        windowManager->Run();
        windowManager->Shutdown();
    }

    {
        std::lock_guard lock(m_mutex);
        m_engineWindow = nullptr;
    }
    m_threadActive = false;
    m_startupCondition.notify_all();
}

HWND EngineHost::EngineWindow() const
{
    std::lock_guard lock(m_mutex);
    return m_engineWindow;
}

bool EngineHost::PostCommand(hdr_driver::IpcCommand const& command) const
{
    HWND hwnd = EngineWindow();
    if (!hwnd || !IsWindow(hwnd))
    {
        return false;
    }

    auto* heapCommand = new hdr_driver::IpcCommand(command);
    if (!PostMessageW(hwnd, hdr_driver::kIpcApplyMessage, 0, reinterpret_cast<LPARAM>(heapCommand)))
    {
        delete heapCommand;
        return false;
    }

    return true;
}
