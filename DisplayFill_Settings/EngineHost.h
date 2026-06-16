#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#ifndef _WINDEF_
struct HWND__;
using HWND = HWND__*;
#endif

namespace hdr_driver
{
    struct AppState;
    struct IpcCommand;
}

struct EngineState
{
    float targetNits = 1000.0f;
    float frameMarginXRatio = 0.08f;
    float frameMarginYRatio = 0.08f;
    int cornerRadius = 60;
    float visualCornerFeatherPixels = 24.0f;
    int outerCornerRadius = 0;
    int screenInsetPixels = 0;
    float centerBrightnessBoost = 0.18f;
    float colorTemperatureShift = 0.0f;
    float colorTintShift = 0.0f;
    float shadowStrength = 0.28f;
    float shadowSizePixels = 42.0f;
    int normalAlpha = 255;
    int hoverAlpha = 40;
    float hoverTransitionSeconds = 0.45f;
    float startupBreathMinBrightness = 0.20f;
    float startupBreathMaxBrightness = 1.00f;
    float startupBreathDurationSeconds = 3.0f;
    bool passThroughMode = true;
    bool rendererReady = false;
    bool hdrActive = false;
    std::wstring language = L"zh-Hans";
    std::wstring themeMode = L"system";
    std::wstring backdropKind = L"acrylic";
};

class EngineHost
{
public:
    EngineHost();
    ~EngineHost();

    EngineHost(const EngineHost&) = delete;
    EngineHost& operator=(const EngineHost&) = delete;

    void SetSettingsWindow(HWND hwnd);
    bool EnsureEngineRunningAndConnected();
    bool IsEngineRunning() const;
    void Stop();

    std::optional<EngineState> GetState() const;
    bool SetNumber(const char* key, double value);
    bool SetBoolean(const char* key, bool value);
    bool SetString(const char* key, const char* value);
    bool SendCommand(const char* name);
    std::optional<std::string> GetConfigText() const;
    bool SetConfigText(const std::string& content);

private:
    bool StartEngineThread();
    bool WaitForStartup();
    void EngineThreadMain();
    HWND EngineWindow() const;
    bool PostCommand(hdr_driver::IpcCommand const& command) const;

    mutable std::mutex m_mutex;
    std::condition_variable m_startupCondition;
    std::thread m_worker;
    std::unique_ptr<hdr_driver::AppState> m_appState;
    HWND m_engineWindow = nullptr;
    bool m_startAttemptComplete = false;
    bool m_startSucceeded = false;
    std::atomic_bool m_threadActive = false;
};
