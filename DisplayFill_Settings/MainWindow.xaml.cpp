#include "pch.h"
#include "MainWindow.xaml.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <string>
#include <thread>

#pragma comment(lib, "comctl32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Windowing;
using namespace Windows::Graphics;
using namespace Windows::ApplicationModel::DataTransfer;

namespace winrt::DisplayFill_Settings::implementation
{
    constexpr UINT_PTR kSettingsWindowSubclassId = 1;
    constexpr UINT_PTR kTitleBarInputSubclassId = 2;
    constexpr int32_t kSettingsWindowWidthDip = 613;
    constexpr int32_t kSettingsWindowHeightDip = 840;
    constexpr double kLogoSpinDurationMilliseconds = 680.0;

    std::wstring GetAssetPath(wchar_t const* relativePath)
    {
        std::wstring modulePath(MAX_PATH, L'\0');
        DWORD length = 0;
        while (true)
        {
            length = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
            if (length == 0)
            {
                return relativePath;
            }

            if (length < modulePath.size())
            {
                break;
            }

            modulePath.resize(modulePath.size() * 2);
        }

        modulePath.resize(length);
        const size_t slash = modulePath.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return relativePath;
        }

        modulePath.resize(slash + 1);
        modulePath += relativePath;
        return modulePath;
    }

    int32_t ScaleForDpi(int32_t value, UINT dpi)
    {
        return MulDiv(value, dpi ? dpi : USER_DEFAULT_SCREEN_DPI, USER_DEFAULT_SCREEN_DPI);
    }

    RECT GetWorkAreaForWindow(HWND hwnd)
    {
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (monitor && GetMonitorInfoW(monitor, &monitorInfo))
        {
            return monitorInfo.rcWork;
        }

        return {
            GetSystemMetrics(SM_XVIRTUALSCREEN),
            GetSystemMetrics(SM_YVIRTUALSCREEN),
            GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
            GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)
        };
    }

    SIZE GetFixedWindowSize(HWND hwnd)
    {
        const UINT dpi = GetDpiForWindow(hwnd);
        return {
            ScaleForDpi(kSettingsWindowWidthDip, dpi),
            ScaleForDpi(kSettingsWindowHeightDip, dpi)
        };
    }

    LRESULT CALLBACK SettingsWindowSubclassProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR refData)
    {
        UNREFERENCED_PARAMETER(wParam);
        UNREFERENCED_PARAMETER(subclassId);

        if (message == WM_GETMINMAXINFO)
        {
            auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
            const SIZE fixedSize = GetFixedWindowSize(hwnd);
            minMaxInfo->ptMinTrackSize.x = fixedSize.cx;
            minMaxInfo->ptMinTrackSize.y = fixedSize.cy;
            minMaxInfo->ptMaxTrackSize.x = fixedSize.cx;
            minMaxInfo->ptMaxTrackSize.y = fixedSize.cy;
            return 0;
        }

        if (auto* window = reinterpret_cast<MainWindow*>(refData))
        {
            LRESULT dragResult = 0;
            if (window->TryHandleTitleBarDragMessage(hwnd, message, wParam, lParam, dragResult))
            {
                return dragResult;
            }
        }

        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, SettingsWindowSubclassProc, kSettingsWindowSubclassId);
        }

        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    LRESULT CALLBACK TitleBarInputSubclassProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR refData)
    {
        UNREFERENCED_PARAMETER(subclassId);

        if (auto* window = reinterpret_cast<MainWindow*>(refData))
        {
            LRESULT dragResult = 0;
            if (window->TryHandleTitleBarDragMessage(hwnd, message, wParam, lParam, dragResult))
            {
                return dragResult;
            }
        }

        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, TitleBarInputSubclassProc, kTitleBarInputSubclassId);
        }

        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    BOOL CALLBACK FindContentBridgeWindowProc(HWND hwnd, LPARAM lParam)
    {
        wchar_t className[128] = {};
        GetClassNameW(hwnd, className, static_cast<int>(std::size(className)));
        if (std::wcscmp(className, L"Microsoft.UI.Content.DesktopChildSiteBridge") == 0)
        {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }

        return TRUE;
    }

    hstring FormatDouble(double value, int decimals)
    {
        wchar_t buffer[32] = {};
        swprintf_s(buffer, L"%.*f", decimals, value);
        return hstring(buffer);
    }

    struct UiText
    {
        hstring title;
        hstring status;
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
        hstring settings;
        hstring backdropStyle;
        hstring themeMode;
        hstring language;
        hstring actions;
        hstring refresh;
        hstring exitEngine;
        hstring copyConfig;
        hstring pasteConfig;
        hstring hideSettings;
        hstring configCopied;
        hstring configPasted;
        hstring configFailed;
        hstring settingsHidden;
        hstring appSubtitle;
        hstring logoTooltip;
        hstring targetBrightnessDescription;
        hstring centerBoostDescription;
        hstring colorTemperatureDescription;
        hstring colorTintDescription;
        hstring shadowStrengthDescription;
        hstring shadowSizeDescription;
        hstring frameMarginDescription;
        hstring cornerRadiusDescription;
        hstring visualFeatherDescription;
        hstring outerCornerRadiusDescription;
        hstring screenInsetDescription;
        hstring passThroughDescription;
        hstring hoverAlphaDescription;
        hstring hoverTransitionDescription;
        hstring backdropDescription;
        hstring themeModeDescription;
        hstring languageDescription;
    };

    UiText GetUiText(hstring const& language)
    {
        if (language == L"zh-Hant")
        {
            return {
                L"DisplayFill",
                L"狀態",
                L"正在連線到 HDR 引擎...",
                L"無法啟動或連線到 HDR 引擎。",
                L"HDR 引擎狀態讀取失敗。",
                L"HDR 已啟動。",
                L"SDR 備援模式已啟動。",
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
                L"設定",
                L"壓克力風格",
                L"主題",
                L"語言",
                L"操作",
                L"重新整理",
                L"結束引擎",
                L"複製設定檔",
                L"貼上設定檔",
                L"隱藏設定",
                L"已複製設定檔內容。",
                L"已貼上設定檔內容。",
                L"設定檔操作失敗。",
                L"已隱藏設定視窗。",
                L"HDR 螢幕補光燈",
                L"DisplayFill 標誌",
                L"HDR 補光亮度",
                L"燈帶中心亮度增強",
                L"由冷色到暖色的偏移",
                L"由綠色到洋紅色的偏移",
                L"內外陰影深度",
                L"柔和陰影擴散範圍",
                L"以垂直解析度百分比計算寬度",
                L"中間留空區域的圓角大小",
                L"邊緣柔化過渡",
                L"補光層外側形狀圓角",
                L"距離實體螢幕邊緣",
                L"讓滑鼠輸入穿透補光層",
                L"指標靠近時的透明度",
                L"透明度淡入淡出時間",
                L"設定視窗背景材質",
                L"跟隨系統、淺色或深色",
                L"套用到此視窗和 HDR 引擎"
            };
        }

        if (language == L"en-US")
        {
            return {
                L"DisplayFill",
                L"Status",
                L"Connecting to HDR engine...",
                L"Unable to start or connect to HDR engine.",
                L"Failed to read HDR engine state.",
                L"HDR started.",
                L"SDR fallback active.",
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
                L"Settings",
                L"Acrylic Style",
                L"Theme",
                L"Language",
                L"Actions",
                L"Refresh",
                L"Exit Engine",
                L"Copy Config",
                L"Paste Config",
                L"Hide Settings",
                L"Configuration copied.",
                L"Configuration pasted.",
                L"Configuration operation failed.",
                L"Settings window hidden.",
                L"HDR screen fill light",
                L"DisplayFill logo",
                L"HDR fill brightness",
                L"Brighter lamp tube center",
                L"Cool to warm shift",
                L"Green to magenta shift",
                L"Inner and outer depth shadow",
                L"Soft shadow spread",
                L"Width based on vertical resolution",
                L"Rounded center opening size",
                L"Soft edge transition",
                L"Rounded outside lamp shape",
                L"Distance from physical screen edge",
                L"Let mouse input pass through the fill layer",
                L"Opacity while pointer is nearby",
                L"Fade duration",
                L"Window background material",
                L"Follow system, light, or dark",
                L"Applies to this window and the HDR engine"
            };
        }

        return {
            L"DisplayFill",
            L"状态",
            L"正在连接 HDR 引擎...",
            L"无法启动或连接到 HDR 引擎。",
            L"HDR 引擎状态读取失败。",
            L"HDR 已启动。",
            L"SDR 回退模式已启动。",
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
            L"设置",
            L"亚克力风格",
            L"主题",
            L"语言",
            L"操作",
            L"刷新",
            L"退出引擎",
            L"复制配置文件",
            L"粘贴配置文件",
            L"隐藏设置",
            L"已复制配置文件内容。",
            L"已粘贴配置文件内容。",
            L"配置文件操作失败。",
            L"已隐藏设置窗口。",
            L"HDR 屏幕补光灯",
            L"DisplayFill 标志",
            L"HDR 补光亮度",
            L"灯带中心亮度增强",
            L"由冷色到暖色的偏移",
            L"由绿色到洋红色的偏移",
            L"内外阴影深度",
            L"柔和阴影扩散范围",
            L"以垂直分辨率百分比计算宽度",
            L"中间留空区域的圆角大小",
            L"边缘柔化过渡",
            L"补光层外侧形状圆角",
            L"距离物理屏幕边缘",
            L"让鼠标输入穿透补光层",
            L"指针靠近时的透明度",
            L"透明度淡入淡出时间",
            L"设置窗口背景材质",
            L"跟随系统、浅色或深色",
            L"应用到此窗口和 HDR 引擎"
        };
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        ConfigureWindow();
        ApplyInitialWindowPlacement();
        Activated([this](auto const&, WindowActivatedEventArgs const&)
        {
            if (!m_initialPlacementApplied)
            {
                ApplyInitialWindowPlacement();
            }
        });
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

        m_settingsWindowHwnd = hwnd;
        m_engineHost.SetSettingsWindow(hwnd);
        SetWindowSubclass(hwnd, SettingsWindowSubclassProc, kSettingsWindowSubclassId, reinterpret_cast<DWORD_PTR>(this));
        InstallTitleBarInputSubclass(hwnd);
        StartTitleBarDragPoll();

        auto const windowId = Microsoft::UI::GetWindowIdFromWindow(hwnd);
        auto appWindow = AppWindow::GetFromWindowId(windowId);
        appWindow.SetIcon(GetAssetPath(L"Assets\\SunLight.ico"));
        if (auto presenter = appWindow.Presenter().try_as<OverlappedPresenter>())
        {
            presenter.SetBorderAndTitleBar(false, false);
            presenter.IsResizable(false);
            presenter.IsMaximizable(false);
            presenter.IsMinimizable(false);
        }

        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_BORDER | WS_DLGFRAME | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        style |= WS_SYSMENU;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        InstallTitleBarInputSubclass(hwnd);
    }

    void MainWindow::ApplyInitialWindowPlacement()
    {
        HWND hwnd = nullptr;
        if (auto nativeWindow = this->try_as<IWindowNative>())
        {
            check_hresult(nativeWindow->get_WindowHandle(&hwnd));
        }

        if (!hwnd)
        {
            return;
        }

        constexpr int32_t workAreaMarginDip = 24;

        const UINT dpi = GetDpiForWindow(hwnd);
        const int32_t desiredWindowWidth = ScaleForDpi(kSettingsWindowWidthDip, dpi);
        const int32_t desiredWindowHeight = ScaleForDpi(kSettingsWindowHeightDip, dpi);
        const int32_t workAreaMargin = ScaleForDpi(workAreaMarginDip, dpi);
        const RECT workArea = GetWorkAreaForWindow(hwnd);
        const int32_t workAreaWidth = static_cast<int32_t>((std::max)(1L, workArea.right - workArea.left));
        const int32_t workAreaHeight = static_cast<int32_t>((std::max)(1L, workArea.bottom - workArea.top));
        const int32_t maxWindowWidth = (std::max)(1, workAreaWidth - workAreaMargin * 2);
        const int32_t maxWindowHeight = (std::max)(1, workAreaHeight - workAreaMargin * 2);
        const int32_t windowWidth = (std::min)(desiredWindowWidth, maxWindowWidth);
        const int32_t windowHeight = (std::min)(desiredWindowHeight, maxWindowHeight);
        const int32_t x = workArea.left + (std::max)(0, (workAreaWidth - windowWidth) / 2);
        const int32_t y = workArea.top + (std::max)(0, (workAreaHeight - windowHeight) / 2);

        auto const windowId = Microsoft::UI::GetWindowIdFromWindow(hwnd);
        auto appWindow = AppWindow::GetFromWindowId(windowId);
        appWindow.MoveAndResize(RectInt32{ x, y, windowWidth, windowHeight });
        m_initialPlacementApplied = true;
        InstallTitleBarInputSubclass(hwnd);
    }

    void MainWindow::InstallTitleBarInputSubclass(HWND hwnd)
    {
        HWND inputHwnd = nullptr;
        EnumChildWindows(hwnd, FindContentBridgeWindowProc, reinterpret_cast<LPARAM>(&inputHwnd));
        if (!inputHwnd)
        {
            return;
        }

        if (m_titleBarInputHwnd && m_titleBarInputHwnd != inputHwnd)
        {
            RemoveWindowSubclass(m_titleBarInputHwnd, TitleBarInputSubclassProc, kTitleBarInputSubclassId);
        }

        m_titleBarInputHwnd = inputHwnd;
        SetWindowSubclass(inputHwnd, TitleBarInputSubclassProc, kTitleBarInputSubclassId, reinterpret_cast<DWORD_PTR>(this));
    }

    void MainWindow::StartTitleBarDragPoll()
    {
        bool expected = false;
        if (!m_titleBarDragPollRunning.compare_exchange_strong(expected, true))
        {
            return;
        }

        m_titleBarDragPollThread = std::thread([this]
        {
            TitleBarDragPollLoop();
        });
    }

    void MainWindow::StopTitleBarDragPoll()
    {
        if (m_titleBarDragPollRunning.exchange(false) && m_titleBarDragPollThread.joinable())
        {
            m_titleBarDragPollThread.join();
        }
    }

    void MainWindow::TitleBarDragPollLoop()
    {
        bool dragging = false;
        bool wasLeftButtonDown = false;
        POINT dragStartCursor{};
        RECT dragStartRect{};

        while (m_titleBarDragPollRunning.load())
        {
            const HWND hwnd = m_settingsWindowHwnd;
            const bool visible = hwnd && IsWindowVisible(hwnd);
            const bool leftButtonDown = visible && ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);

            POINT cursor{};
            GetCursorPos(&cursor);

            if (!leftButtonDown)
            {
                dragging = false;
                wasLeftButtonDown = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }

            if (!dragging && !wasLeftButtonDown && IsScreenPointInTitleDragRegion(cursor.x, cursor.y))
            {
                dragging = true;
                dragStartCursor = cursor;
                GetWindowRect(hwnd, &dragStartRect);
                SetForegroundWindow(hwnd);
            }

            if (dragging)
            {
                const int dx = cursor.x - dragStartCursor.x;
                const int dy = cursor.y - dragStartCursor.y;
                SetWindowPos(
                    hwnd,
                    nullptr,
                    dragStartRect.left + dx,
                    dragStartRect.top + dy,
                    0,
                    0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            wasLeftButtonDown = leftButtonDown;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    void MainWindow::ApplyUiPreferences(hstring const& themeMode, hstring const& backdropKind)
    {
        ElementTheme theme = ElementTheme::Default;
        if (themeMode == L"light")
        {
            theme = ElementTheme::Light;
        }
        else if (themeMode == L"dark")
        {
            theme = ElementTheme::Dark;
        }
        RootGrid().RequestedTheme(theme);

        if (backdropKind == L"mica")
        {
            SystemBackdrop(Media::MicaBackdrop{});
            RootGrid().Background(Media::SolidColorBrush{ Windows::UI::Color{ 0, 0, 0, 0 } });
        }
        else if (backdropKind == L"solid")
        {
            SystemBackdrop(Media::SystemBackdrop{ nullptr });
            if (auto brush = Application::Current().Resources().Lookup(box_value(L"ApplicationPageBackgroundThemeBrush")).try_as<Media::Brush>())
            {
                RootGrid().Background(brush);
            }
        }
        else
        {
            SystemBackdrop(Media::DesktopAcrylicBackdrop{});
            RootGrid().Background(Media::SolidColorBrush{ Windows::UI::Color{ 0, 0, 0, 0 } });
        }
    }

    bool MainWindow::TryHandleTitleBarDragMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
    {
        if (message == WM_LBUTTONDOWN)
        {
            POINT clientPoint{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };
            if (!IsPointInTitleDragRegion(clientPoint.x, clientPoint.y))
            {
                return false;
            }

            const HWND targetHwnd = m_settingsWindowHwnd ? m_settingsWindowHwnd : hwnd;
            m_titleBarDragging = true;
            GetCursorPos(&m_titleBarDragStartCursor);
            GetWindowRect(targetHwnd, &m_titleBarDragStartRect);
            SetForegroundWindow(targetHwnd);
            SetCapture(hwnd);
            result = 0;
            return true;
        }

        if (message == WM_MOUSEMOVE && m_titleBarDragging)
        {
            if ((wParam & MK_LBUTTON) == 0)
            {
                m_titleBarDragging = false;
                if (GetCapture() == hwnd)
                {
                    ReleaseCapture();
                }
                result = 0;
                return true;
            }

            POINT cursor{};
            GetCursorPos(&cursor);
            const int dx = cursor.x - m_titleBarDragStartCursor.x;
            const int dy = cursor.y - m_titleBarDragStartCursor.y;
            const HWND targetHwnd = m_settingsWindowHwnd ? m_settingsWindowHwnd : hwnd;
            SetWindowPos(
                targetHwnd,
                nullptr,
                m_titleBarDragStartRect.left + dx,
                m_titleBarDragStartRect.top + dy,
                0,
                0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            result = 0;
            return true;
        }

        if ((message == WM_LBUTTONUP || message == WM_CANCELMODE) && m_titleBarDragging)
        {
            m_titleBarDragging = false;
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            result = 0;
            return true;
        }

        if (message == WM_CAPTURECHANGED)
        {
            m_titleBarDragging = false;
        }

        return false;
    }

    bool MainWindow::HandleGlobalTitleBarDragMouse(WPARAM message, MSLLHOOKSTRUCT const& mouseInfo)
    {
        if (!m_settingsWindowHwnd || !IsWindowVisible(m_settingsWindowHwnd))
        {
            return false;
        }

        if (message == WM_LBUTTONDOWN)
        {
            if (!IsScreenPointInTitleDragRegion(mouseInfo.pt.x, mouseInfo.pt.y))
            {
                return false;
            }

            m_titleBarDragging = true;
            m_titleBarDragStartCursor = mouseInfo.pt;
            GetWindowRect(m_settingsWindowHwnd, &m_titleBarDragStartRect);
            SetForegroundWindow(m_settingsWindowHwnd);
            return true;
        }

        if (message == WM_MOUSEMOVE && m_titleBarDragging)
        {
            const int dx = mouseInfo.pt.x - m_titleBarDragStartCursor.x;
            const int dy = mouseInfo.pt.y - m_titleBarDragStartCursor.y;
            SetWindowPos(
                m_settingsWindowHwnd,
                nullptr,
                m_titleBarDragStartRect.left + dx,
                m_titleBarDragStartRect.top + dy,
                0,
                0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return true;
        }

        if (message == WM_LBUTTONUP && m_titleBarDragging)
        {
            m_titleBarDragging = false;
            return true;
        }

        return false;
    }

    bool MainWindow::IsPointInTitleDragRegion(int clientX, int clientY)
    {
        auto const dragArea = TitleDragArea();
        auto const root = RootGrid();
        if (!dragArea || !root)
        {
            return false;
        }

        const double width = dragArea.ActualWidth();
        const double height = dragArea.ActualHeight();
        if (width <= 0.0 || height <= 0.0)
        {
            return false;
        }

        double scale = 1.0;
        if (auto xamlRoot = root.XamlRoot())
        {
            scale = xamlRoot.RasterizationScale();
        }

        auto const transform = dragArea.TransformToVisual(root);
        auto const bounds = transform.TransformBounds(
            winrt::Windows::Foundation::Rect{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) });
        const int left = static_cast<int>(std::floor(bounds.X * scale));
        const int top = static_cast<int>(std::floor(bounds.Y * scale));
        const int right = static_cast<int>(std::ceil((bounds.X + bounds.Width) * scale));
        const int bottom = static_cast<int>(std::ceil((bounds.Y + bounds.Height) * scale));

        return clientX >= left && clientX < right && clientY >= top && clientY < bottom;
    }

    bool MainWindow::IsScreenPointInTitleDragRegion(int screenX, int screenY)
    {
        if (!m_settingsWindowHwnd)
        {
            return false;
        }

        RECT windowRect{};
        if (!GetWindowRect(m_settingsWindowHwnd, &windowRect))
        {
            return false;
        }

        const UINT dpi = GetDpiForWindow(m_settingsWindowHwnd);
        const int left = windowRect.left + ScaleForDpi(72, dpi);
        const int top = windowRect.top;
        const int right = windowRect.left + ScaleForDpi(280, dpi);
        const int bottom = windowRect.top + ScaleForDpi(82, dpi);

        return screenX >= left && screenX < right && screenY >= top && screenY < bottom;
    }

    void MainWindow::SpinLogo()
    {
        if (!m_logoSpinTimer)
        {
            m_logoSpinTimer = DispatcherQueue().CreateTimer();
            m_logoSpinTimer.Interval(std::chrono::milliseconds(16));
            m_logoSpinTimer.Tick([this](auto const&, auto const&)
            {
                UpdateLogoSpin();
            });
        }

        m_logoSpinStartTick = GetTickCount64();
        m_logoSpinStartAngle = LogoRotateTransform().Angle();
        m_logoSpinTimer.Start();
    }

    void MainWindow::UpdateLogoSpin()
    {
        const double elapsed = static_cast<double>(GetTickCount64() - m_logoSpinStartTick);
        const double rawProgress = (std::min)(1.0, elapsed / kLogoSpinDurationMilliseconds);
        const double easedProgress = 1.0 - std::pow(1.0 - rawProgress, 3.0);
        const double angle = m_logoSpinStartAngle + 360.0 * easedProgress;
        LogoRotateTransform().Angle(angle);

        if (rawProgress >= 1.0)
        {
            LogoRotateTransform().Angle(std::fmod(m_logoSpinStartAngle + 360.0, 360.0));
            if (m_logoSpinTimer)
            {
                m_logoSpinTimer.Stop();
            }
        }
    }

    void MainWindow::StartEngineWatchdog()
    {
        Closed([this](auto const&, WindowEventArgs const&)
        {
            StopTitleBarDragPoll();
            m_engineHost.Stop();
        });

        m_engineWatchdogTimer = DispatcherQueue().CreateTimer();
        m_engineWatchdogTimer.Interval(std::chrono::seconds(1));
        m_engineWatchdogTimer.Tick([this](auto const&, auto const&)
        {
            if (m_everConnected && m_connected && !m_engineHost.IsEngineRunning())
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
        AppSubtitleText().Text(text.appSubtitle);
        HdrExpander().Header(box_value(text.hdr));
        TargetBrightnessText().Text(text.targetBrightness);
        TargetBrightnessDescriptionText().Text(text.targetBrightnessDescription);
        FrameExpander().Header(box_value(text.frame));
        FrameMarginText().Text(text.frameMargin);
        FrameMarginDescriptionText().Text(text.frameMarginDescription);
        CornerRadiusText().Text(text.cornerRadius);
        CornerRadiusDescriptionText().Text(text.cornerRadiusDescription);
        VisualFeatherText().Text(text.visualFeather);
        VisualFeatherDescriptionText().Text(text.visualFeatherDescription);
        OuterCornerRadiusText().Text(text.outerCornerRadius);
        OuterCornerRadiusDescriptionText().Text(text.outerCornerRadiusDescription);
        ScreenInsetText().Text(text.screenInset);
        ScreenInsetDescriptionText().Text(text.screenInsetDescription);
        CenterBoostText().Text(text.centerBoost);
        CenterBoostDescriptionText().Text(text.centerBoostDescription);
        ColorTemperatureText().Text(text.colorTemperature);
        ColorTemperatureDescriptionText().Text(text.colorTemperatureDescription);
        ColorTintText().Text(text.colorTint);
        ColorTintDescriptionText().Text(text.colorTintDescription);
        ShadowStrengthText().Text(text.shadowStrength);
        ShadowStrengthDescriptionText().Text(text.shadowStrengthDescription);
        ShadowSizeText().Text(text.shadowSize);
        ShadowSizeDescriptionText().Text(text.shadowSizeDescription);
        WindowExpander().Header(box_value(text.window));
        PassThroughText().Text(text.passThroughMode);
        PassThroughDescriptionText().Text(text.passThroughDescription);
        HoverAlphaText().Text(text.hoverAlpha);
        HoverAlphaDescriptionText().Text(text.hoverAlphaDescription);
        HoverTransitionText().Text(text.hoverTransition);
        HoverTransitionDescriptionText().Text(text.hoverTransitionDescription);
        LanguageExpander().Header(box_value(text.settings));
        BackdropText().Text(text.backdropStyle);
        BackdropDescriptionText().Text(text.backdropDescription);
        ThemeModeText().Text(text.themeMode);
        ThemeModeDescriptionText().Text(text.themeModeDescription);
        LanguageText().Text(text.language);
        LanguageDescriptionText().Text(text.languageDescription);
        RefreshButton().Label(text.refresh);
        ExitEngineButton().Label(text.exitEngine);
        CopyConfigButton().Label(text.copyConfig);
        PasteConfigButton().Label(text.pasteConfig);
        HideSettingsButton().Label(text.hideSettings);
        ToolTipService::SetToolTip(RefreshButton(), box_value(text.refresh));
        ToolTipService::SetToolTip(ExitEngineButton(), box_value(text.exitEngine));
        ToolTipService::SetToolTip(CopyConfigButton(), box_value(text.copyConfig));
        ToolTipService::SetToolTip(PasteConfigButton(), box_value(text.pasteConfig));
        ToolTipService::SetToolTip(HideSettingsButton(), box_value(text.hideSettings));
        ToolTipService::SetToolTip(LogoButton(), box_value(text.logoTooltip));
        ConnectionStatusInfoBar().Title(text.status);
    }

    void MainWindow::ConnectEvents()
    {
        LogoButton().Click([this](auto const&, RoutedEventArgs const&)
        {
            SpinLogo();
        });

        TitleDragArea().PointerPressed([this](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
        {
            const auto pointerPoint = e.GetCurrentPoint(TitleDragArea());
            if (!pointerPoint.Properties().IsLeftButtonPressed())
            {
                return;
            }

            HWND hwnd = m_settingsWindowHwnd;
            if (!hwnd && this->try_as<IWindowNative>())
            {
                if (auto nativeWindow = this->try_as<IWindowNative>())
                {
                    check_hresult(nativeWindow->get_WindowHandle(&hwnd));
                }
            }

            if (!hwnd)
            {
                return;
            }

            m_titleBarDragging = true;
            GetCursorPos(&m_titleBarDragStartCursor);
            GetWindowRect(hwnd, &m_titleBarDragStartRect);
            SetForegroundWindow(hwnd);
            TitleDragArea().CapturePointer(e.Pointer());
            e.Handled(true);
        });

        TitleDragArea().PointerMoved([this](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
        {
            if (!m_titleBarDragging)
            {
                return;
            }

            const auto pointerPoint = e.GetCurrentPoint(TitleDragArea());
            if (!pointerPoint.Properties().IsLeftButtonPressed())
            {
                m_titleBarDragging = false;
                TitleDragArea().ReleasePointerCapture(e.Pointer());
                e.Handled(true);
                return;
            }

            const HWND hwnd = m_settingsWindowHwnd;
            if (!hwnd)
            {
                return;
            }

            POINT cursor{};
            GetCursorPos(&cursor);
            const int dx = cursor.x - m_titleBarDragStartCursor.x;
            const int dy = cursor.y - m_titleBarDragStartCursor.y;
            SetWindowPos(
                hwnd,
                nullptr,
                m_titleBarDragStartRect.left + dx,
                m_titleBarDragStartRect.top + dy,
                0,
                0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            e.Handled(true);
        });

        auto stopTitleDrag = [this](Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
        {
            if (m_titleBarDragging)
            {
                m_titleBarDragging = false;
                TitleDragArea().ReleasePointerCapture(e.Pointer());
                e.Handled(true);
            }
        };

        TitleDragArea().PointerReleased([stopTitleDrag](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
        {
            stopTitleDrag(e);
        });

        TitleDragArea().PointerCanceled([stopTitleDrag](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
        {
            stopTitleDrag(e);
        });

        TitleDragArea().PointerCaptureLost([stopTitleDrag](auto const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
        {
            stopTitleDrag(e);
        });

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

        BackdropComboBox().SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            OnBackdropChanged();
        });

        ThemeModeComboBox().SelectionChanged([this](auto const&, SelectionChangedEventArgs const&)
        {
            OnThemeModeChanged();
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

        HideSettingsButton().Click([this](auto const&, RoutedEventArgs const&)
        {
            HideSettingsWindow();
        });

        ExitEngineButton().Click([this](auto const&, RoutedEventArgs const&)
        {
            if (m_connected && m_engineHost.SendCommand("exit"))
            {
                SetStatus(GetUiText(m_language).exitSent, InfoBarSeverity::Success);
                m_connected = false;
                Close();
            }
        });
    }

    void MainWindow::ConnectEngineAndLoadState()
    {
        SetStatus(GetUiText(m_language).connecting);
        m_connected = m_engineHost.EnsureEngineRunningAndConnected();
        if (!m_connected)
        {
            SetStatus(GetUiText(m_language).connectFailed, InfoBarSeverity::Error);
            return;
        }
        m_everConnected = true;

        std::optional<EngineState> state;
        for (int i = 0; i < 50; ++i)
        {
            state = m_engineHost.GetState();
            if (state && state->rendererReady)
            {
                break;
            }
            Sleep(100);
        }

        if (!state || !state->rendererReady)
        {
            SetStatus(GetUiText(m_language).stateFailed, InfoBarSeverity::Warning);
            return;
        }

        LoadState(*state);
        const auto text = GetUiText(m_language);
        SetStatus(
            state->hdrActive ? text.hdrActive : text.sdrFallback,
            state->hdrActive ? InfoBarSeverity::Success : InfoBarSeverity::Warning);
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

        if (state.backdropKind == L"mica")
        {
            BackdropComboBox().SelectedIndex(1);
        }
        else if (state.backdropKind == L"solid")
        {
            BackdropComboBox().SelectedIndex(2);
        }
        else
        {
            BackdropComboBox().SelectedIndex(0);
        }

        if (state.themeMode == L"light")
        {
            ThemeModeComboBox().SelectedIndex(1);
        }
        else if (state.themeMode == L"dark")
        {
            ThemeModeComboBox().SelectedIndex(2);
        }
        else
        {
            ThemeModeComboBox().SelectedIndex(0);
        }
        ApplyUiPreferences(hstring{ state.themeMode.c_str() }, hstring{ state.backdropKind.c_str() });

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

    void MainWindow::SetStatus(hstring const& text, InfoBarSeverity severity)
    {
        ConnectionStatusInfoBar().IsOpen(true);
        ConnectionStatusInfoBar().Severity(severity);
        ConnectionStatusInfoBar().Message(text);
    }

    void MainWindow::OnTargetNitsChanged(double value)
    {
        UpdateValueTexts();

        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("targetNits", value);
        }
    }

    void MainWindow::OnFrameMarginChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("frameMarginRatio", value);
        }
    }

    void MainWindow::OnCornerRadiusChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("cornerRadius", value);
        }
    }

    void MainWindow::OnCornerFeatherChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("visualCornerFeatherPixels", value);
        }
    }

    void MainWindow::OnOuterCornerRadiusChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("outerCornerRadius", value);
        }
    }

    void MainWindow::OnScreenInsetChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("screenInsetPixels", value);
        }
    }

    void MainWindow::OnCenterBrightnessBoostChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("centerBrightnessBoost", value);
        }
    }

    void MainWindow::OnColorTemperatureChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("colorTemperatureShift", value);
        }
    }

    void MainWindow::OnColorTintChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("colorTintShift", value);
        }
    }

    void MainWindow::OnShadowStrengthChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("shadowStrength", value);
        }
    }

    void MainWindow::OnShadowSizeChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("shadowSizePixels", value);
        }
    }

    void MainWindow::OnHoverAlphaChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("hoverAlpha", value);
        }
    }

    void MainWindow::OnHoverTransitionChanged(double value)
    {
        UpdateValueTexts();
        if (!m_loading && m_connected)
        {
            m_engineHost.SetNumber("hoverTransitionSeconds", value);
        }
    }

    void MainWindow::OnPassThroughChanged(bool value)
    {
        if (!m_loading && m_connected)
        {
            m_engineHost.SetBoolean("passThroughMode", value);
        }
    }

    void MainWindow::OnThemeModeChanged()
    {
        hstring themeMode = L"system";
        const int32_t selectedIndex = ThemeModeComboBox().SelectedIndex();
        if (selectedIndex == 1)
        {
            themeMode = L"light";
        }
        else if (selectedIndex == 2)
        {
            themeMode = L"dark";
        }

        hstring backdropKind = L"acrylic";
        const int32_t backdropIndex = BackdropComboBox().SelectedIndex();
        if (backdropIndex == 1)
        {
            backdropKind = L"mica";
        }
        else if (backdropIndex == 2)
        {
            backdropKind = L"solid";
        }

        ApplyUiPreferences(themeMode, backdropKind);
        if (!m_loading && m_connected)
        {
            m_engineHost.SetString("themeMode", to_string(themeMode).c_str());
        }
    }

    void MainWindow::OnBackdropChanged()
    {
        hstring backdropKind = L"acrylic";
        const int32_t selectedIndex = BackdropComboBox().SelectedIndex();
        if (selectedIndex == 1)
        {
            backdropKind = L"mica";
        }
        else if (selectedIndex == 2)
        {
            backdropKind = L"solid";
        }

        hstring themeMode = L"system";
        const int32_t themeIndex = ThemeModeComboBox().SelectedIndex();
        if (themeIndex == 1)
        {
            themeMode = L"light";
        }
        else if (themeIndex == 2)
        {
            themeMode = L"dark";
        }

        ApplyUiPreferences(themeMode, backdropKind);
        if (!m_loading && m_connected)
        {
            m_engineHost.SetString("backdropKind", to_string(backdropKind).c_str());
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
            m_engineHost.SetString("language", "zh-Hant");
        }
        else if (selectedIndex == 2)
        {
            ApplyLanguage(L"en-US");
            m_engineHost.SetString("language", "en-US");
        }
        else
        {
            ApplyLanguage(L"zh-Hans");
            m_engineHost.SetString("language", "zh-Hans");
        }
    }

    void MainWindow::HideSettingsWindow()
    {
        SetStatus(GetUiText(m_language).settingsHidden, InfoBarSeverity::Success);

        HWND hwnd = nullptr;
        if (auto nativeWindow = this->try_as<IWindowNative>())
        {
            check_hresult(nativeWindow->get_WindowHandle(&hwnd));
        }

        if (hwnd)
        {
            ShowWindow(hwnd, SW_HIDE);
        }
    }

    void MainWindow::CopyConfigToClipboard()
    {
        if (!m_connected)
        {
            SetStatus(GetUiText(m_language).configFailed, InfoBarSeverity::Error);
            return;
        }

        const auto config = m_engineHost.GetConfigText();
        if (!config)
        {
            SetStatus(GetUiText(m_language).configFailed, InfoBarSeverity::Error);
            return;
        }

        DataPackage package;
        package.SetText(to_hstring(*config));
        Clipboard::SetContent(package);
        SetStatus(GetUiText(m_language).configCopied, InfoBarSeverity::Success);
    }

    void MainWindow::PasteConfigFromClipboard()
    {
        if (!m_connected)
        {
            SetStatus(GetUiText(m_language).configFailed, InfoBarSeverity::Error);
            return;
        }

        auto content = Clipboard::GetContent();
        if (!content.Contains(StandardDataFormats::Text()))
        {
            SetStatus(GetUiText(m_language).configFailed, InfoBarSeverity::Error);
            return;
        }

        content.GetTextAsync().Completed([this](auto const& operation, Windows::Foundation::AsyncStatus status)
        {
            if (status != Windows::Foundation::AsyncStatus::Completed)
            {
                DispatcherQueue().TryEnqueue([this]
                {
                    SetStatus(GetUiText(m_language).configFailed, InfoBarSeverity::Error);
                });
                return;
            }

            const hstring text = operation.GetResults();
            const std::string utf8 = to_string(text);
            const bool ok = m_engineHost.SetConfigText(utf8);
            DispatcherQueue().TryEnqueue([this, ok]
            {
                if (ok)
                {
                    ConnectEngineAndLoadState();
                    SetStatus(GetUiText(m_language).configPasted, InfoBarSeverity::Success);
                }
                else
                {
                    SetStatus(GetUiText(m_language).configFailed, InfoBarSeverity::Error);
                }
            });
        });
    }
}
