#include "pch.h"
#include "MainWindow.xaml.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <cwchar>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Windowing;
using namespace Windows::Graphics;
using namespace Windows::ApplicationModel::DataTransfer;

namespace winrt::DisplayFill_Settings::implementation
{
    hstring FormatDouble(double value, int decimals)
    {
        wchar_t buffer[32] = {};
        swprintf_s(buffer, L"%.*f", decimals, value);
        return hstring(buffer);
    }

    struct UiText
    {
        hstring title;
        hstring connecting;
        hstring connectFailed;
        hstring stateFailed;
        hstring hdrActive;
        hstring sdrFallback;
        hstring exitSent;
        hstring hdr;
        hstring targetBrightness;
        hstring frame;
        hstring frameMargin;
        hstring cornerRadius;
        hstring visualFeather;
        hstring outerCornerRadius;
        hstring screenInset;
        hstring centerBoost;
        hstring colorTemperature;
        hstring colorTint;
        hstring shadowStrength;
        hstring shadowSize;
        hstring window;
        hstring passThroughMode;
        hstring hoverAlpha;
        hstring hoverTransition;
        hstring language;
        hstring actions;
        hstring refresh;
        hstring exitEngine;
        hstring copyConfig;
        hstring pasteConfig;
        hstring configCopied;
        hstring configPasted;
        hstring configFailed;
    };

    UiText GetUiText(hstring const& language)
    {
        if (language == L"zh-Hant")
        {
            return {
                L"DisplayFill 設定",
                L"正在連線到 HDR 引擎...",
                L"無法啟動或連線到 HDR 引擎。",
                L"已連線，但讀取引擎狀態失敗。",
                L"已連線。HDR 已啟用。",
                L"已連線。正在使用 SDR 備援模式。",
                L"已送出結束 HDR 引擎命令。",
                L"HDR",
                L"目標亮度",
                L"相框",
                L"相框寬度",
                L"圓角半徑",
                L"邊緣羽化",
                L"外圓角",
                L"螢幕邊距",
                L"中心亮度增強",
                L"色溫",
                L"色調",
                L"陰影強度",
                L"陰影大小",
                L"視窗",
                L"滑鼠穿透模式",
                L"滑鼠停留透明度",
                L"滑鼠停留轉場",
                L"語言",
                L"操作",
                L"重新整理",
                L"結束引擎",
                L"複製設定檔",
                L"貼上設定檔",
                L"已複製設定檔內容。",
                L"已貼上設定檔內容。",
                L"設定檔操作失敗。"
            };
        }

        if (language == L"en-US")
        {
            return {
                L"DisplayFill Settings",
                L"Connecting to HDR engine...",
                L"Unable to start or connect to HDR engine.",
                L"Connected, but failed to read engine state.",
                L"Connected. HDR active.",
                L"Connected. SDR fallback active.",
                L"HDR engine exit command sent.",
                L"HDR",
                L"Target Brightness",
                L"Frame",
                L"Frame Margin",
                L"Corner Radius",
                L"Visual Feather",
                L"Outer Corner",
                L"Screen Inset",
                L"Center Boost",
                L"Color Temperature",
                L"Tint",
                L"Shadow Strength",
                L"Shadow Size",
                L"Window",
                L"Pass-through Mode",
                L"Hover Alpha",
                L"Hover Transition",
                L"Language",
                L"Actions",
                L"Refresh",
                L"Exit Engine",
                L"Copy Config",
                L"Paste Config",
                L"Configuration copied.",
                L"Configuration pasted.",
                L"Configuration operation failed."
            };
        }

        return {
            L"DisplayFill 设置",
            L"正在连接 HDR 引擎...",
            L"无法启动或连接到 HDR 引擎。",
            L"已连接，但读取引擎状态失败。",
            L"已连接。HDR 已启用。",
            L"已连接。正在使用 SDR 回退模式。",
            L"已发送退出 HDR 引擎命令。",
            L"HDR",
            L"目标亮度",
            L"相框",
            L"相框宽度",
            L"圆角半径",
            L"边缘羽化",
            L"外圆角",
            L"屏幕边距",
            L"中心亮度增强",
            L"色温",
            L"色调",
            L"阴影强度",
            L"阴影大小",
            L"窗口",
            L"鼠标穿透模式",
            L"鼠标悬停透明度",
            L"鼠标悬停过渡",
            L"语言",
            L"操作",
            L"刷新",
            L"退出引擎",
            L"复制配置文件",
            L"粘贴配置文件",
            L"已复制配置文件内容。",
            L"已粘贴配置文件内容。",
            L"配置文件操作失败。"
        };
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();
        ConfigureWindow();
        ApplyLanguage(m_language);
        ConnectEvents();
        ConnectEngineAndLoadState();
        StartEngineWatchdog();
    }

    void MainWindow::ConfigureWindow()
    {
        SystemBackdrop(Media::DesktopAcrylicBackdrop{});

        HWND hwnd = nullptr;
        if (auto nativeWindow = this->try_as<IWindowNative>())
        {
            check_hresult(nativeWindow->get_WindowHandle(&hwnd));
        }

        if (!hwnd)
        {
            return;
        }

        ExtendsContentIntoTitleBar(true);
        SetTitleBar(TitleBar());

        if (auto titleBar = AppWindow::GetFromWindowId(Microsoft::UI::GetWindowIdFromWindow(hwnd)).TitleBar())
        {
            titleBar.ButtonBackgroundColor(Windows::UI::Colors::Transparent());
            titleBar.ButtonInactiveBackgroundColor(Windows::UI::Colors::Transparent());
            titleBar.ButtonHoverBackgroundColor(Windows::UI::ColorHelper::FromArgb(48, 255, 255, 255));
            titleBar.ButtonPressedBackgroundColor(Windows::UI::ColorHelper::FromArgb(72, 255, 255, 255));
        }

        constexpr int32_t windowWidth = 1000;
        constexpr int32_t windowHeight = 1200;

        const int32_t screenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int32_t screenHeight = GetSystemMetrics(SM_CYSCREEN);
        const int32_t x = (std::max)(0, (screenWidth - windowWidth) / 2);
        const int32_t y = (std::max)(0, (screenHeight - windowHeight) / 2);
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, windowWidth, windowHeight, SWP_NOACTIVATE);

        auto const windowId = Microsoft::UI::GetWindowIdFromWindow(hwnd);
        auto appWindow = AppWindow::GetFromWindowId(windowId);
        appWindow.Resize(SizeInt32{ windowWidth, windowHeight });
        if (auto presenter = appWindow.Presenter().try_as<OverlappedPresenter>())
        {
            presenter.IsResizable(false);
            presenter.IsMaximizable(false);
        }
    }

    void MainWindow::StartEngineWatchdog()
    {
        m_engineWatchdogTimer = DispatcherQueue().CreateTimer();
        m_engineWatchdogTimer.Interval(std::chrono::seconds(1));
        m_engineWatchdogTimer.Tick([this](auto const&, auto const&)
        {
            if (m_everConnected && m_connected && !m_pipeClient.IsEngineRunning())
            {
                m_connected = false;
                Close();
            }
        });
        m_engineWatchdogTimer.Start();
    }

    void MainWindow::ApplyLanguage(hstring const& language)
    {
        m_language = language;
        const auto text = GetUiText(m_language);

        Title(text.title);
        AppTitleText().Text(text.title);
        HdrExpander().Header(box_value(text.hdr));
        TargetBrightnessText().Text(text.targetBrightness);
        FrameExpander().Header(box_value(text.frame));
        FrameMarginText().Text(text.frameMargin);
        CornerRadiusText().Text(text.cornerRadius);
        VisualFeatherText().Text(text.visualFeather);
        OuterCornerRadiusText().Text(text.outerCornerRadius);
        ScreenInsetText().Text(text.screenInset);
        CenterBoostText().Text(text.centerBoost);
        ColorTemperatureText().Text(text.colorTemperature);
        ColorTintText().Text(text.colorTint);
        ShadowStrengthText().Text(text.shadowStrength);
        ShadowSizeText().Text(text.shadowSize);
        WindowExpander().Header(box_value(text.window));
        PassThroughText().Text(text.passThroughMode);
        HoverAlphaText().Text(text.hoverAlpha);
        HoverTransitionText().Text(text.hoverTransition);
        LanguageExpander().Header(box_value(text.language));
        ActionsExpander().Header(box_value(text.actions));
        RefreshButton().Content(box_value(text.refresh));
        ExitEngineButton().Content(box_value(text.exitEngine));
        CopyConfigButton().Content(box_value(text.copyConfig));
        PasteConfigButton().Content(box_value(text.pasteConfig));
    }

    void MainWindow::ConnectEvents()
    {
        TargetNitsSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnTargetNitsChanged(e.NewValue());
        });

        FrameMarginSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnFrameMarginChanged(e.NewValue());
        });

        CornerRadiusSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnCornerRadiusChanged(e.NewValue());
        });

        CornerFeatherSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnCornerFeatherChanged(e.NewValue());
        });

        OuterCornerRadiusSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnOuterCornerRadiusChanged(e.NewValue());
        });

        ScreenInsetSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnScreenInsetChanged(e.NewValue());
        });

        CenterBoostSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnCenterBrightnessBoostChanged(e.NewValue());
        });

        ColorTemperatureSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnColorTemperatureChanged(e.NewValue());
        });

        ColorTintSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnColorTintChanged(e.NewValue());
        });

        ShadowStrengthSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnShadowStrengthChanged(e.NewValue());
        });

        ShadowSizeSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnShadowSizeChanged(e.NewValue());
        });

        HoverAlphaSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnHoverAlphaChanged(e.NewValue());
        });

        HoverTransitionSlider().ValueChanged([this](auto const&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
        {
            OnHoverTransitionChanged(e.NewValue());
        });

        PassThroughToggle().Toggled([this](auto const&, RoutedEventArgs const&)
        {
            OnPassThroughChanged(PassThroughToggle().IsOn());
        });

        LanguageComboBox().SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            OnLanguageChanged();
        });

        RefreshButton().Click([this](auto const&, RoutedEventArgs const&)
        {
            ConnectEngineAndLoadState();
        });

        CopyConfigButton().Click([this](auto const&, RoutedEventArgs const&)
        {
            CopyConfigToClipboard();
        });

        PasteConfigButton().Click([this](auto const&, RoutedEventArgs const&)
        {
            PasteConfigFromClipboard();
        });

        ExitEngineButton().Click([this](auto const&, RoutedEventArgs const&)
        {
            if (m_connected && m_pipeClient.SendCommand("exit"))
            {
                SetStatus(GetUiText(m_language).exitSent);
                m_connected = false;
                Close();
            }
        });
    }

    void MainWindow::ConnectEngineAndLoadState()
    {
        SetStatus(GetUiText(m_language).connecting);
        m_connected = m_pipeClient.EnsureEngineRunningAndConnected();
        if (!m_connected)
        {
            SetStatus(GetUiText(m_language).connectFailed);
            return;
        }
        m_everConnected = true;

        std::optional<EngineState> state;
        for (int i = 0; i < 20; ++i)
        {
            state = m_pipeClient.GetState();
            if (state)
            {
                break;
            }
            Sleep(100);
        }

        if (!state)
        {
            SetStatus(GetUiText(m_language).stateFailed);
            return;
        }

        LoadState(*state);
        const auto text = GetUiText(m_language);
        SetStatus(state->hdrActive ? text.hdrActive : text.sdrFallback);
    }

    void MainWindow::LoadState(EngineState const& state)
    {
        m_loading = true;
        TargetNitsSlider().Value(state.targetNits);
        FrameMarginSlider().Value(state.frameMarginXRatio);
        CornerRadiusSlider().Value(state.cornerRadius);
        CornerFeatherSlider().Value(state.visualCornerFeatherPixels);
        OuterCornerRadiusSlider().Value(state.outerCornerRadius);
        ScreenInsetSlider().Value(state.screenInsetPixels);
        CenterBoostSlider().Value(state.centerBrightnessBoost);
        ColorTemperatureSlider().Value(state.colorTemperatureShift);
        ColorTintSlider().Value(state.colorTintShift);
        ShadowStrengthSlider().Value(state.shadowStrength);
        ShadowSizeSlider().Value(state.shadowSizePixels);
        HoverAlphaSlider().Value(state.hoverAlpha);
        HoverTransitionSlider().Value(state.hoverTransitionSeconds);
        PassThroughToggle().IsOn(state.passThroughMode);

        if (state.language == L"zh-Hant")
        {
            ApplyLanguage(L"zh-Hant");
            LanguageComboBox().SelectedIndex(1);
        }
        else if (state.language == L"en-US")
        {
            ApplyLanguage(L"en-US");
            LanguageComboBox().SelectedIndex(2);
        }
        else
        {
            ApplyLanguage(L"zh-Hans");
            LanguageComboBox().SelectedIndex(0);
        }

        UpdateValueTexts();
        m_loading = false;
    }

    void MainWindow::UpdateValueTexts()
    {
        TargetNitsValueText().Text(to_hstring(static_cast<int>(TargetNitsSlider().Value())) + L" nits");
        FrameMarginValueText().Text(to_hstring(static_cast<int>(FrameMarginSlider().Value() * 100.0)) + L"%");
        CornerRadiusValueText().Text(to_hstring(static_cast<int>(CornerRadiusSlider().Value())) + L" px");
        CornerFeatherValueText().Text(to_hstring(static_cast<int>(CornerFeatherSlider().Value())) + L" px");
        OuterCornerRadiusValueText().Text(to_hstring(static_cast<int>(OuterCornerRadiusSlider().Value())) + L" px");
        ScreenInsetValueText().Text(to_hstring(static_cast<int>(ScreenInsetSlider().Value())) + L" px");
        CenterBoostValueText().Text(to_hstring(static_cast<int>(CenterBoostSlider().Value() * 100.0)) + L"%");
        ColorTemperatureValueText().Text(to_hstring(static_cast<int>(ColorTemperatureSlider().Value() * 100.0)));
        ColorTintValueText().Text(to_hstring(static_cast<int>(ColorTintSlider().Value() * 100.0)));
        ShadowStrengthValueText().Text(to_hstring(static_cast<int>(ShadowStrengthSlider().Value() * 100.0)) + L"%");
        ShadowSizeValueText().Text(to_hstring(static_cast<int>(ShadowSizeSlider().Value())) + L" px");
        HoverAlphaValueText().Text(to_hstring(static_cast<int>(HoverAlphaSlider().Value())));
        HoverTransitionValueText().Text(FormatDouble(HoverTransitionSlider().Value(), 2) + L" s");
    }

    void MainWindow::SetStatus(hstring const& text)
    {
        ConnectionStatusText().Text(text);
    }

    void MainWindow::OnTargetNitsChanged(double value)
    {
        UpdateValueTexts();

        const double t = std::clamp((value - 80.0) / (2000.0 - 80.0), 0.0, 1.0);
        const uint8_t red = static_cast<uint8_t>(138 + (194 - 138) * t);
        const uint8_t green = static_cast<uint8_t>(90 + (65 - 90) * t);
        const uint8_t blue = static_cast<uint8_t>(0 + (12 - 0) * t);
        if (auto brush = RootGrid().Resources().Lookup(box_value(L"TargetNitsReadoutBrush")).try_as<Media::SolidColorBrush>())
        {
            brush.Color(Windows::UI::ColorHelper::FromArgb(255, red, green, blue));
        }

        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("targetNits", value);
        }
    }

    void MainWindow::OnFrameMarginChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("frameMarginRatio", value);
        }
    }

    void MainWindow::OnCornerRadiusChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("cornerRadius", value);
        }
    }

    void MainWindow::OnCornerFeatherChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("visualCornerFeatherPixels", value);
        }
    }

    void MainWindow::OnOuterCornerRadiusChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("outerCornerRadius", value);
        }
    }

    void MainWindow::OnScreenInsetChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("screenInsetPixels", value);
        }
    }

    void MainWindow::OnCenterBrightnessBoostChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("centerBrightnessBoost", value);
        }
    }

    void MainWindow::OnColorTemperatureChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("colorTemperatureShift", value);
        }
    }

    void MainWindow::OnColorTintChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("colorTintShift", value);
        }
    }

    void MainWindow::OnShadowStrengthChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("shadowStrength", value);
        }
    }

    void MainWindow::OnShadowSizeChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("shadowSizePixels", value);
        }
    }

    void MainWindow::OnHoverAlphaChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("hoverAlpha", value);
        }
    }

    void MainWindow::OnHoverTransitionChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetNumber("hoverTransitionSeconds", value);
        }
    }

    void MainWindow::OnPassThroughChanged(bool value)
    {
        if (!m_loading && m_connected)
        {
            m_pipeClient.SetBoolean("passThroughMode", value);
        }
    }

    void MainWindow::OnLanguageChanged()
    {
        if (m_loading || !m_connected)
        {
            return;
        }

        const int32_t selectedIndex = LanguageComboBox().SelectedIndex();
        if (selectedIndex == 1)
        {
            ApplyLanguage(L"zh-Hant");
            m_pipeClient.SetString("language", "zh-Hant");
        }
        else if (selectedIndex == 2)
        {
            ApplyLanguage(L"en-US");
            m_pipeClient.SetString("language", "en-US");
        }
        else
        {
            ApplyLanguage(L"zh-Hans");
            m_pipeClient.SetString("language", "zh-Hans");
        }
    }

    void MainWindow::CopyConfigToClipboard()
    {
        if (!m_connected)
        {
            SetStatus(GetUiText(m_language).configFailed);
            return;
        }

        const auto config = m_pipeClient.GetConfigText();
        if (!config)
        {
            SetStatus(GetUiText(m_language).configFailed);
            return;
        }

        DataPackage package;
        package.SetText(to_hstring(*config));
        Clipboard::SetContent(package);
        SetStatus(GetUiText(m_language).configCopied);
    }

    void MainWindow::PasteConfigFromClipboard()
    {
        if (!m_connected)
        {
            SetStatus(GetUiText(m_language).configFailed);
            return;
        }

        auto content = Clipboard::GetContent();
        if (!content.Contains(StandardDataFormats::Text()))
        {
            SetStatus(GetUiText(m_language).configFailed);
            return;
        }

        content.GetTextAsync().Completed([this](auto const& operation, Windows::Foundation::AsyncStatus status)
        {
            if (status != Windows::Foundation::AsyncStatus::Completed)
            {
                DispatcherQueue().TryEnqueue([this]
                {
                    SetStatus(GetUiText(m_language).configFailed);
                });
                return;
            }

            const hstring text = operation.GetResults();
            const std::string utf8 = to_string(text);
            const bool ok = m_pipeClient.SetConfigText(utf8);
            DispatcherQueue().TryEnqueue([this, ok]
            {
                if (ok)
                {
                    ConnectEngineAndLoadState();
                    SetStatus(GetUiText(m_language).configPasted);
                }
                else
                {
                    SetStatus(GetUiText(m_language).configFailed);
                }
            });
        });
    }
}
