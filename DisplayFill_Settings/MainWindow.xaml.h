#pragma once

#include <atomic>
#include <thread>

#include "MainWindow.g.h"
#include "EngineHost.h"

namespace winrt::DisplayFill_Settings::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        bool TryHandleTitleBarDragMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);
        bool HandleGlobalTitleBarDragMouse(WPARAM message, MSLLHOOKSTRUCT const& mouseInfo);

    private:
        void ConfigureWindow();
        void ApplyInitialWindowPlacement();
        void InstallTitleBarInputSubclass(HWND hwnd);
        void StartTitleBarDragPoll();
        void StopTitleBarDragPoll();
        void TitleBarDragPollLoop();
        void ApplyUiPreferences(winrt::hstring const& themeMode, winrt::hstring const& backdropKind);
        void ApplyLanguage(winrt::hstring const& language);
        bool IsPointInTitleDragRegion(int clientX, int clientY);
        bool IsScreenPointInTitleDragRegion(int screenX, int screenY);
        void SpinLogo();
        void UpdateLogoSpin();
        void StartEngineWatchdog();
        void ConnectEvents();
        void ConnectEngineAndLoadState();
        void LoadState(EngineState const& state);
        void UpdateValueTexts();
        void SetStatus(
            winrt::hstring const& text,
            winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity = winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);

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
        void OnThemeModeChanged();
        void OnBackdropChanged();
        void OnLanguageChanged();
        void HideSettingsWindow();
        void CopyConfigToClipboard();
        void PasteConfigFromClipboard();

        EngineHost m_engineHost;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_engineWatchdogTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_logoSpinTimer{ nullptr };
        std::thread m_titleBarDragPollThread;
        std::atomic_bool m_titleBarDragPollRunning{ false };
        winrt::hstring m_language = L"zh-Hans";
        ULONGLONG m_logoSpinStartTick = 0;
        double m_logoSpinStartAngle = 0.0;
        POINT m_titleBarDragStartCursor{};
        RECT m_titleBarDragStartRect{};
        HWND m_settingsWindowHwnd = nullptr;
        HWND m_titleBarInputHwnd = nullptr;
        bool m_loading = false;
        bool m_connected = false;
        bool m_everConnected = false;
        bool m_initialPlacementApplied = false;
        bool m_titleBarDragging = false;
    };
}

namespace winrt::DisplayFill_Settings::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
