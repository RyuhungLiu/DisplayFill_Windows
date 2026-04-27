#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct EngineState
{
    float targetNits = 1000.0f;
    float frameMarginXRatio = 0.08f;
    float frameMarginYRatio = 0.08f;
    int cornerRadius = 60;
    float visualCornerFeatherPixels = 24.0f;
    int normalAlpha = 255;
    int hoverAlpha = 40;
    float hoverTransitionSeconds = 0.45f;
    float startupBreathMinBrightness = 0.20f;
    float startupBreathMaxBrightness = 1.00f;
    float startupBreathDurationSeconds = 3.0f;
    bool passThroughMode = true;
    bool hdrActive = false;
    std::wstring language = L"zh-Hans";
};

class PipeClient
{
public:
    bool EnsureEngineRunningAndConnected();
    bool IsEngineRunning();
    std::optional<EngineState> GetState();
    bool SetNumber(const char* key, double value);
    bool SetBoolean(const char* key, bool value);
    bool SetString(const char* key, const char* value);
    bool SendCommand(const char* name);

private:
    bool TryConnectOnce();
    bool StartEngineProcess();
    std::string SendJson(const std::string& json);
    std::optional<EngineState> ParseState(const std::string& json) const;
    std::wstring GetEnginePath() const;
};
