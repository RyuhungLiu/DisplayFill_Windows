#pragma once

#include "MainWindow.g.h"
#include "PipeClient.h"

namespace winrt::DisplayFill_Settings::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

    private:
        void ConfigureWindow();
        void ApplyLanguage(winrt::hstring const& language);
        void StartEngineWatchdog();
        void ConnectEvents();
        void ConnectEngineAndLoadState();
        void LoadState(EngineState const& state);
        void UpdateValueTexts();
        void SetStatus(winrt::hstring const& text);

        void OnTargetNitsChanged(double value);
        void OnFrameMarginChanged(double value);
        void OnCornerRadiusChanged(double value);
        void OnCornerFeatherChanged(double value);
        void OnOuterCornerRadiusChanged(double value);
        void OnScreenInsetChanged(double value);
        void OnCenterBrightnessBoostChanged(double value);
        void OnColorTemperatureChanged(double value);
        void OnColorTintChanged(double value);
        void OnShadowStrengthChanged(double value);
        void OnShadowSizeChanged(double value);
        void OnHoverAlphaChanged(double value);
        void OnHoverTransitionChanged(double value);
        void OnPassThroughChanged(bool value);
        void OnLanguageChanged();
        void CopyConfigToClipboard();
        void PasteConfigFromClipboard();

        PipeClient m_pipeClient;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_engineWatchdogTimer{ nullptr };
        winrt::hstring m_language = L"zh-Hans";
        bool m_loading = false;
        bool m_connected = false;
        bool m_everConnected = false;
    };
}

namespace winrt::DisplayFill_Settings::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
