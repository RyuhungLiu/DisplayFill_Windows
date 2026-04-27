#pragma once

#include "SettingsWindow.g.h"

namespace winrt::DisplayFillWindows::implementation
{
    struct SettingsSnapshot
    {
        float targetNits = 1000.0f;
        float frameMarginRatio = 0.08f;
        int cornerRadius = 60;
        float cornerFeatherPixels = 24.0f;
        bool passThroughMode = true;
        uint8_t hoverAlpha = 40;
        float hoverTransitionSeconds = 0.45f;
        float startupBreathMinBrightness = 0.20f;
        float startupBreathDurationSeconds = 3.0f;
    };

    using SettingsChangedCallback = void(*)(const SettingsSnapshot& settings);

    struct SettingsWindow : SettingsWindowT<SettingsWindow>
    {
        SettingsWindow();

        void LoadSettings(SettingsSnapshot const& settings);
        void SetSettingsChangedCallback(SettingsChangedCallback callback);

    private:
        void ConnectEvents();
        void UpdateValueTexts();
        SettingsSnapshot BuildSnapshot() const;
        void ApplyCurrentSettings();
        void ResetDefaults();

        SettingsChangedCallback m_callback = nullptr;
    };
}

namespace winrt::DisplayFillWindows::factory_implementation
{
    struct SettingsWindow : SettingsWindowT<SettingsWindow, implementation::SettingsWindow>
    {
    };
}
