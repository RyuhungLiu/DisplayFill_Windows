#include "HDRDriver.h"
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

namespace hdr_driver
{

// Global instance pointers for static window proc
namespace
{
    WindowManager* g_pWindowManager = nullptr;
    std::atomic<HWND> g_settingsWindowHandle = nullptr;

    struct LocalizedText
    {
        const wchar_t* tooltip;
        const wchar_t* settingsTitle;
        const wchar_t* passThroughMode;
        const wchar_t* brightness;
        const wchar_t* frameWidth;
        const wchar_t* hoverOpacity;
        const wchar_t* veryTransparent;
        const wchar_t* transparent;
        const wchar_t* slightlyTransparent;
        const wchar_t* language;
        const wchar_t* languageZhHans;
        const wchar_t* languageZhHant;
        const wchar_t* languageEnUs;
        const wchar_t* exit;
    };

    const LocalizedText& GetText(AppLanguage language)
    {
        static constexpr LocalizedText zhHans =
        {
            L"HDR 屏幕补光灯",
            L"设置（托盘菜单）",
            L"穿透模式",
            L"亮度",
            L"相框宽度",
            L"悬停透明度",
            L"非常透明",
            L"透明",
            L"轻微透明",
            L"语言",
            L"简体中文",
            L"繁体中文",
            L"英文（美国）",
            L"退出",
        };

        static constexpr LocalizedText zhHant =
        {
            L"HDR 螢幕補光燈",
            L"設定（系統匣選單）",
            L"穿透模式",
            L"亮度",
            L"相框寬度",
            L"懸停透明度",
            L"非常透明",
            L"透明",
            L"輕微透明",
            L"語言",
            L"簡體中文",
            L"繁體中文",
            L"英文（美國）",
            L"結束",
        };

        static constexpr LocalizedText enUs =
        {
            L"HDR Screen Fill Light",
            L"Settings (Tray Menu)",
            L"Pass-through Mode",
            L"Brightness",
            L"Frame Width",
            L"Hover Opacity",
            L"Very Transparent",
            L"Transparent",
            L"Slightly Transparent",
            L"Language",
            L"Simplified Chinese",
            L"Traditional Chinese",
            L"English (United States)",
            L"Exit",
        };

        switch (language)
        {
        case AppLanguage::ZhHant:
            return zhHant;
        case AppLanguage::EnUs:
            return enUs;
        case AppLanguage::ZhHans:
        default:
            return zhHans;
        }
    }

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

    RECT GetVirtualScreenRect()
    {
        RECT rect = {
            GetSystemMetrics(SM_XVIRTUALSCREEN),
            GetSystemMetrics(SM_YVIRTUALSCREEN),
            GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
            GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)
        };

        if (rect.right <= rect.left || rect.bottom <= rect.top)
        {
            rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        }

        return rect;
    }
}

void SetSettingsWindowHandle(HWND hwnd)
{
    g_settingsWindowHandle.store(hwnd);
}

//=============================================================================
// WindowManager Implementation
//=============================================================================

WindowManager::WindowManager()
{
}

WindowManager::~WindowManager()
{
    Shutdown();
}

bool WindowManager::Initialize(HINSTANCE instance, AppState* appState, Renderer* renderer)
{
    m_instance = instance;
    m_appState = appState;
    m_renderer = renderer;
    g_pWindowManager = this;

    if (!CreateMainWindow(instance))
    {
        return false;
    }

    RegisterHotkeys();
    ApplyPassThroughMode();
    AddTrayIcon();
    m_ipcServer = std::make_unique<IpcServer>(m_hwnd, m_appState);
    if (!m_ipcServer->Start())
    {
        std::printf("IPC server failed to start.\n");
    }

    return true;
}

void WindowManager::Shutdown()
{
    if (m_ipcServer)
    {
        m_ipcServer->Stop();
        m_ipcServer.reset();
    }

    RemoveTrayIcon();
    UnregisterHotkeys();

    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    g_pWindowManager = nullptr;
}

bool WindowManager::CreateMainWindow(HINSTANCE instance)
{
    const wchar_t* className = kWindowClassName;

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;

    if (!RegisterClassExW(&windowClass))
    {
        std::printf("RegisterClassExW failed. GetLastError=%lu\n", GetLastError());
        return false;
    }

    if (!RefreshMonitorMetrics())
    {
        return false;
    }

    const int monitorWidth = m_appState->monitorWidth;
    const int monitorHeight = m_appState->monitorHeight;
    const int monitorLeft = m_appState->monitorLeft;
    const int monitorTop = m_appState->monitorTop;
    const int inset = AppState::ClampInt(m_appState->settings.screenInsetPixels, 0, (std::min)(monitorWidth, monitorHeight) / 3);
    const int windowWidth = (std::max)(1, monitorWidth - inset * 2);
    const int windowHeight = (std::max)(1, monitorHeight - inset * 2);
    const int windowLeft = monitorLeft + inset;
    const int windowTop = monitorTop + inset;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        className,
        L"HDR Screen Fill Light",
        WS_POPUP,
        windowLeft,
        windowTop,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!m_hwnd)
    {
        std::printf("CreateWindowExW failed. GetLastError=%lu\n", GetLastError());
        return false;
    }

    SetWindowOpacity(m_appState->settings.normalWindowAlpha);

    // Initialize state dimensions
    m_appState->clientWidth = windowWidth;
    m_appState->clientHeight = windowHeight;
    m_appState->windowLeft = windowLeft;
    m_appState->windowTop = windowTop;
    m_appState->UpdateMarginsFromSettings();

    std::printf("Monitor resolution: %dx%d at (%d,%d)\n", monitorWidth, monitorHeight, monitorLeft, monitorTop);
    std::printf("Light window: %dx%d at (%d,%d), screen inset=%d px\n", windowWidth, windowHeight, windowLeft, windowTop, inset);
    std::printf("Frame margins: left/right=%d px, top/bottom=%d px\n", m_appState->marginLeft, m_appState->marginTop);

    return true;
}

bool WindowManager::RefreshMonitorMetrics()
{
    if (!m_appState)
    {
        return false;
    }

    HMONITOR primaryMonitor = m_hwnd
        ? MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY)
        : MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(primaryMonitor, &monitorInfo))
    {
        std::printf("GetMonitorInfoW failed. GetLastError=%lu\n", GetLastError());
        monitorInfo.rcMonitor = GetVirtualScreenRect();
    }

    m_appState->monitorWidth = static_cast<int>((std::max)(1L, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left));
    m_appState->monitorHeight = static_cast<int>((std::max)(1L, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top));
    m_appState->monitorLeft = monitorInfo.rcMonitor.left;
    m_appState->monitorTop = monitorInfo.rcMonitor.top;

    return true;
}

void WindowManager::UpdateWindowRegion()
{
    if (!m_hwnd || m_appState->clientWidth <= 0 || m_appState->clientHeight <= 0)
    {
        return;
    }

    RECT clientRect = { 0, 0, m_appState->clientWidth, m_appState->clientHeight };

    const int outerRadius = AppState::ClampInt(m_appState->settings.outerCornerRadius, 0, (std::min)(m_appState->clientWidth, m_appState->clientHeight) / 2);
    HRGN fullRegion = outerRadius > 0
        ? CreateRoundRectRgn(clientRect.left, clientRect.top, clientRect.right, clientRect.bottom, outerRadius * 2, outerRadius * 2)
        : CreateRectRgn(clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);
    const int holeWidth = (std::max)(1L, m_appState->holeRect.right - m_appState->holeRect.left);
    const int holeHeight = (std::max)(1L, m_appState->holeRect.bottom - m_appState->holeRect.top);
    const int holeRadius = AppState::ClampInt(
        m_appState->settings.holeCornerRadius,
        0,
        static_cast<int>((std::min)(holeWidth, holeHeight) / 2));

    HRGN holeRegion = CreateRoundRectRgn(
        m_appState->holeRect.left,
        m_appState->holeRect.top,
        m_appState->holeRect.right,
        m_appState->holeRect.bottom,
        holeRadius * 2,
        holeRadius * 2);

    if (!fullRegion || !holeRegion)
    {
        if (fullRegion) DeleteObject(fullRegion);
        if (holeRegion) DeleteObject(holeRegion);
        std::printf("CreateRectRgn/CreateRoundRectRgn failed. GetLastError=%lu\n", GetLastError());
        return;
    }

    if (CombineRgn(fullRegion, fullRegion, holeRegion, RGN_DIFF) == ERROR)
    {
        std::printf("CombineRgn failed. GetLastError=%lu\n", GetLastError());
        DeleteObject(fullRegion);
        DeleteObject(holeRegion);
        return;
    }

    DeleteObject(holeRegion);

    if (SetWindowRgn(m_hwnd, fullRegion, TRUE) == 0)
    {
        std::printf("SetWindowRgn failed. GetLastError=%lu\n", GetLastError());
        DeleteObject(fullRegion);
        return;
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void WindowManager::SetWindowOpacity(BYTE alpha)
{
    if (!m_hwnd)
    {
        return;
    }

    SetLayeredWindowAttributes(m_hwnd, 0, alpha, LWA_ALPHA);
    m_currentWindowAlpha = alpha;
}

void WindowManager::RegisterHotkeys()
{
    if (!RegisterHotKey(m_hwnd, kHotkeyTogglePassThrough, MOD_CONTROL | MOD_NOREPEAT, VK_F6))
    {
        std::printf("RegisterHotKey(Ctrl+F6) failed. GetLastError=%lu\n", GetLastError());
    }
}

void WindowManager::UnregisterHotkeys()
{
    if (m_hwnd)
    {
        UnregisterHotKey(m_hwnd, kHotkeyTogglePassThrough);
    }
}

void WindowManager::AddTrayIcon()
{
    if (!m_hwnd || m_trayIconAdded)
    {
        return;
    }

    m_trayIconData = {};
    m_trayIconData.cbSize = sizeof(m_trayIconData);
    m_trayIconData.hWnd = m_hwnd;
    m_trayIconData.uID = 1;
    m_trayIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_trayIconData.uCallbackMessage = kTrayIconMessage;
    const UINT dpi = m_hwnd ? GetDpiForWindow(m_hwnd) : USER_DEFAULT_SCREEN_DPI;
    const std::wstring iconPath = GetAssetPath(L"Assets\\SunLight.ico");
    m_trayIconHandle = static_cast<HICON>(LoadImageW(
        nullptr,
        iconPath.c_str(),
        IMAGE_ICON,
        GetSystemMetricsForDpi(SM_CXSMICON, dpi),
        GetSystemMetricsForDpi(SM_CYSMICON, dpi),
        LR_LOADFROMFILE));
    m_trayIconData.hIcon = m_trayIconHandle ? m_trayIconHandle : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(m_trayIconData.szTip, GetText(m_appState->settings.language).tooltip);

    m_trayIconAdded = Shell_NotifyIconW(NIM_ADD, &m_trayIconData) != FALSE;
    if (m_trayIconAdded)
    {
        m_trayIconData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &m_trayIconData);
    }
    else if (m_trayIconHandle)
    {
        DestroyIcon(m_trayIconHandle);
        m_trayIconHandle = nullptr;
    }
}

void WindowManager::UpdateTrayTooltip()
{
    if (!m_trayIconAdded)
    {
        return;
    }

    m_trayIconData.uFlags = NIF_TIP;
    wcscpy_s(m_trayIconData.szTip, GetText(m_appState->settings.language).tooltip);
    Shell_NotifyIconW(NIM_MODIFY, &m_trayIconData);
}

void WindowManager::RemoveTrayIcon()
{
    if (m_trayIconAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_trayIconData);
        m_trayIconAdded = false;
    }

    if (m_trayIconHandle)
    {
        DestroyIcon(m_trayIconHandle);
        m_trayIconHandle = nullptr;
    }
}

void WindowManager::ShowTrayMenu()
{
    if (!m_hwnd || !m_appState)
    {
        return;
    }

    HMENU menu = CreatePopupMenu();
    HMENU brightnessMenu = CreatePopupMenu();
    HMENU frameMenu = CreatePopupMenu();
    HMENU alphaMenu = CreatePopupMenu();
    HMENU languageMenu = CreatePopupMenu();
    if (!menu || !brightnessMenu || !frameMenu || !alphaMenu || !languageMenu)
    {
        if (menu) DestroyMenu(menu);
        if (brightnessMenu) DestroyMenu(brightnessMenu);
        if (frameMenu) DestroyMenu(frameMenu);
        if (alphaMenu) DestroyMenu(alphaMenu);
        if (languageMenu) DestroyMenu(languageMenu);
        return;
    }

    const LocalizedText& text = GetText(m_appState->settings.language);

    AppendMenuW(menu, MF_STRING, kTrayMenuOpenSettings, text.settingsTitle);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING | (m_appState->settings.passThroughMode ? MF_CHECKED : MF_UNCHECKED),
        kTrayMenuTogglePassThrough, text.passThroughMode);

    AppendMenuW(brightnessMenu, MF_STRING | (m_appState->settings.targetNits == 600.0f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuBrightness600, L"600 nits");
    AppendMenuW(brightnessMenu, MF_STRING | (m_appState->settings.targetNits == 800.0f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuBrightness800, L"800 nits");
    AppendMenuW(brightnessMenu, MF_STRING | (m_appState->settings.targetNits == 1000.0f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuBrightness1000, L"1000 nits");
    AppendMenuW(brightnessMenu, MF_STRING | (m_appState->settings.targetNits == 1200.0f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuBrightness1200, L"1200 nits");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(brightnessMenu), text.brightness);

    AppendMenuW(frameMenu, MF_STRING | (m_appState->settings.frameMarginXRatio == 0.05f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuFrame5, L"5%");
    AppendMenuW(frameMenu, MF_STRING | (m_appState->settings.frameMarginXRatio == 0.08f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuFrame8, L"8%");
    AppendMenuW(frameMenu, MF_STRING | (m_appState->settings.frameMarginXRatio == 0.10f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuFrame10, L"10%");
    AppendMenuW(frameMenu, MF_STRING | (m_appState->settings.frameMarginXRatio == 0.12f ? MF_CHECKED : MF_UNCHECKED), kTrayMenuFrame12, L"12%");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(frameMenu), text.frameWidth);

    AppendMenuW(alphaMenu, MF_STRING | (m_appState->settings.mouseHoverWindowAlpha == 20 ? MF_CHECKED : MF_UNCHECKED), kTrayMenuHoverAlpha20, text.veryTransparent);
    AppendMenuW(alphaMenu, MF_STRING | (m_appState->settings.mouseHoverWindowAlpha == 40 ? MF_CHECKED : MF_UNCHECKED), kTrayMenuHoverAlpha40, text.transparent);
    AppendMenuW(alphaMenu, MF_STRING | (m_appState->settings.mouseHoverWindowAlpha == 70 ? MF_CHECKED : MF_UNCHECKED), kTrayMenuHoverAlpha70, text.slightlyTransparent);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(alphaMenu), text.hoverOpacity);

    AppendMenuW(languageMenu, MF_STRING | (m_appState->settings.language == AppLanguage::ZhHans ? MF_CHECKED : MF_UNCHECKED), kTrayMenuLanguageZhHans, text.languageZhHans);
    AppendMenuW(languageMenu, MF_STRING | (m_appState->settings.language == AppLanguage::ZhHant ? MF_CHECKED : MF_UNCHECKED), kTrayMenuLanguageZhHant, text.languageZhHant);
    AppendMenuW(languageMenu, MF_STRING | (m_appState->settings.language == AppLanguage::EnUs ? MF_CHECKED : MF_UNCHECKED), kTrayMenuLanguageEnUs, text.languageEnUs);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(languageMenu), text.language);

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayMenuExit, text.exit);

    POINT cursorPoint = {};
    GetCursorPos(&cursorPoint);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursorPoint.x, cursorPoint.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void WindowManager::OnTrayIcon(LPARAM lParam)
{
    const UINT message = LOWORD(lParam);
    if (message == WM_RBUTTONUP || message == WM_CONTEXTMENU)
    {
        ShowTrayMenu();
    }
    else if (message == WM_LBUTTONDBLCLK)
    {
        TogglePassThroughMode();
    }
}

void WindowManager::OpenSettingsApp()
{
    HWND settingsWindow = g_settingsWindowHandle.load();
    if (settingsWindow && IsWindow(settingsWindow))
    {
        ShowWindow(settingsWindow, SW_RESTORE);
        SetForegroundWindow(settingsWindow);
        return;
    }

    MessageBoxW(m_hwnd, L"设置窗口当前不可用。请重新启动 DisplayFill。", L"DisplayFill", MB_OK | MB_ICONWARNING);
}

void WindowManager::ApplyPassThroughMode()
{
    if (!m_hwnd)
    {
        return;
    }

    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TOPMOST;
    if (m_appState->settings.passThroughMode)
    {
        exStyle |= WS_EX_TRANSPARENT;
    }
    else
    {
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }

    SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    std::printf("Pass-through mode: %s\n", m_appState->settings.passThroughMode ? "ON" : "OFF");
}

void WindowManager::TogglePassThroughMode()
{
    m_appState->settings.passThroughMode = !m_appState->settings.passThroughMode;
    ApplyPassThroughMode();
    SaveCurrentSettings();
}

void WindowManager::ApplyFrameSettings()
{
    m_appState->UpdateMarginsFromSettings();
    UpdateWindowRegion();
    m_appState->renderNeeded = true;
}

void WindowManager::ApplyScreenInset()
{
    if (!m_hwnd || !m_appState)
    {
        return;
    }

    RefreshMonitorMetrics();

    const int inset = AppState::ClampInt(m_appState->settings.screenInsetPixels, 0, (std::min)(m_appState->monitorWidth, m_appState->monitorHeight) / 3);
    const int width = (std::max)(1, m_appState->monitorWidth - inset * 2);
    const int height = (std::max)(1, m_appState->monitorHeight - inset * 2);
    const int left = m_appState->monitorLeft + inset;
    const int top = m_appState->monitorTop + inset;

    m_appState->windowLeft = left;
    m_appState->windowTop = top;
    SetWindowPos(m_hwnd, HWND_TOPMOST, left, top, width, height, SWP_NOACTIVATE);
    m_appState->clientWidth = width;
    m_appState->clientHeight = height;
    m_appState->UpdateMarginsFromSettings();
    UpdateWindowRegion();
    m_appState->hdrActive = m_renderer->RefreshHDRState(m_hwnd);
    m_appState->renderNeeded = true;
}

void WindowManager::SetTargetNits(float nits)
{
    m_appState->settings.targetNits = nits;
    m_appState->renderNeeded = true;
    SaveCurrentSettings();
    std::printf("Target brightness: %.0f nits\n", nits);
}

void WindowManager::SetFrameMarginRatio(float ratio)
{
    m_appState->settings.frameMarginXRatio = ratio;
    m_appState->settings.frameMarginYRatio = ratio;
    ApplyFrameSettings();
    SaveCurrentSettings();
    std::printf("Frame margin ratio: %.0f%%\n", ratio * 100.0f);
}

void WindowManager::SetHoverAlpha(BYTE alpha)
{
    m_appState->settings.mouseHoverWindowAlpha = alpha;
    m_opacityTargetAlpha = IsCursorOverFrame() ? alpha : m_appState->settings.normalWindowAlpha;
    SaveCurrentSettings();
    std::printf("Hover alpha: %u\n", static_cast<unsigned int>(alpha));
}

void WindowManager::SetLanguage(AppLanguage language)
{
    m_appState->settings.language = language;
    UpdateTrayTooltip();
    SaveCurrentSettings();
}

void WindowManager::SaveCurrentSettings() const
{
    if (m_appState)
    {
        SaveSettingsToIni(m_appState->settings);
    }
}

void WindowManager::OnIpcCommand(IpcCommand* command)
{
    if (!command)
    {
        return;
    }

    std::unique_ptr<IpcCommand> scopedCommand(command);
    switch (scopedCommand->commandType)
    {
    case IpcCommandType::Exit:
        DestroyWindow(m_hwnd);
        return;

    case IpcCommandType::TogglePassThrough:
        TogglePassThroughMode();
        return;

    case IpcCommandType::ReloadConfig:
        ApplyScreenInset();
        ApplyFrameSettings();
        ApplyPassThroughMode();
        return;

    case IpcCommandType::SetValue:
        break;
    }

    const std::wstring key(scopedCommand->key);
    if (key == L"targetNits" && scopedCommand->valueType == IpcValueType::Number)
    {
        SetTargetNits(static_cast<float>(scopedCommand->numberValue));
    }
    else if (key == L"frameMarginRatio" && scopedCommand->valueType == IpcValueType::Number)
    {
        SetFrameMarginRatio(static_cast<float>(scopedCommand->numberValue));
    }
    else if (key == L"frameMarginXRatio" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.frameMarginXRatio = static_cast<float>(scopedCommand->numberValue);
        ApplyFrameSettings();
        SaveCurrentSettings();
    }
    else if (key == L"frameMarginYRatio" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.frameMarginYRatio = static_cast<float>(scopedCommand->numberValue);
        ApplyFrameSettings();
        SaveCurrentSettings();
    }
    else if (key == L"cornerRadius" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.holeCornerRadius = AppState::ClampInt(static_cast<int>(scopedCommand->numberValue), 0, 480);
        UpdateWindowRegion();
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"outerCornerRadius" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.outerCornerRadius = AppState::ClampInt(static_cast<int>(scopedCommand->numberValue), 0, 480);
        UpdateWindowRegion();
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"screenInsetPixels" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.screenInsetPixels = AppState::ClampInt(static_cast<int>(scopedCommand->numberValue), 0, 2000);
        ApplyScreenInset();
        SaveCurrentSettings();
    }
    else if (key == L"visualCornerFeatherPixels" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.visualCornerFeatherPixels = static_cast<float>(scopedCommand->numberValue);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"centerBrightnessBoost" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.centerBrightnessBoost = AppState::Clamp(static_cast<float>(scopedCommand->numberValue), 0.0f, 1.0f);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"colorTemperatureShift" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.colorTemperatureShift = AppState::Clamp(static_cast<float>(scopedCommand->numberValue), -1.0f, 1.0f);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"colorTintShift" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.colorTintShift = AppState::Clamp(static_cast<float>(scopedCommand->numberValue), -1.0f, 1.0f);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"shadowStrength" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.shadowStrength = AppState::Clamp(static_cast<float>(scopedCommand->numberValue), 0.0f, 1.0f);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"shadowSizePixels" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.shadowSizePixels = AppState::Clamp(static_cast<float>(scopedCommand->numberValue), 0.0f, 160.0f);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"normalAlpha" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.normalWindowAlpha = static_cast<BYTE>(AppState::ClampInt(static_cast<int>(scopedCommand->numberValue), 0, 255));
        SaveCurrentSettings();
    }
    else if ((key == L"hoverAlpha" || key == L"mouseHoverWindowAlpha") && scopedCommand->valueType == IpcValueType::Number)
    {
        SetHoverAlpha(static_cast<BYTE>(AppState::ClampInt(static_cast<int>(scopedCommand->numberValue), 0, 255)));
    }
    else if (key == L"hoverTransitionSeconds" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.hoverOpacityTransitionSeconds = static_cast<float>(scopedCommand->numberValue);
        SaveCurrentSettings();
    }
    else if (key == L"startupBreathMinBrightness" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.startupBreathMinBrightness = static_cast<float>(scopedCommand->numberValue);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"startupBreathMaxBrightness" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.startupBreathMaxBrightness = static_cast<float>(scopedCommand->numberValue);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"startupBreathDurationSeconds" && scopedCommand->valueType == IpcValueType::Number)
    {
        m_appState->settings.startupBreathDurationSeconds = static_cast<float>(scopedCommand->numberValue);
        m_appState->renderNeeded = true;
        SaveCurrentSettings();
    }
    else if (key == L"passThroughMode" && scopedCommand->valueType == IpcValueType::Boolean)
    {
        m_appState->settings.passThroughMode = scopedCommand->boolValue;
        ApplyPassThroughMode();
        SaveCurrentSettings();
    }
    else if (key == L"language" && scopedCommand->valueType == IpcValueType::Language)
    {
        SetLanguage(scopedCommand->languageValue);
    }
    else if (key == L"themeMode" && scopedCommand->valueType == IpcValueType::ThemeMode)
    {
        m_appState->settings.themeMode = scopedCommand->themeModeValue;
        SaveCurrentSettings();
    }
    else if (key == L"backdropKind" && scopedCommand->valueType == IpcValueType::BackdropKind)
    {
        m_appState->settings.backdropKind = scopedCommand->backdropKindValue;
        SaveCurrentSettings();
    }
}

bool WindowManager::IsCursorOverFrame() const
{
    if (!m_hwnd || !m_appState)
    {
        return false;
    }

    POINT cursorPoint = {};
    if (!GetCursorPos(&cursorPoint))
    {
        return false;
    }

    ScreenToClient(m_hwnd, &cursorPoint);
    const bool insideClient =
        cursorPoint.x >= 0 && cursorPoint.x < m_appState->clientWidth &&
        cursorPoint.y >= 0 && cursorPoint.y < m_appState->clientHeight;
    if (!insideClient)
    {
        return false;
    }

    const bool insideHole =
        cursorPoint.x >= m_appState->holeRect.left && cursorPoint.x < m_appState->holeRect.right &&
        cursorPoint.y >= m_appState->holeRect.top && cursorPoint.y < m_appState->holeRect.bottom;
    return !insideHole;
}

bool WindowManager::UpdateHoverOpacity()
{
    const bool shouldHover = IsCursorOverFrame();
    const BYTE desiredAlpha = shouldHover ? m_appState->settings.mouseHoverWindowAlpha : m_appState->settings.normalWindowAlpha;

    if (shouldHover != m_hoverTargetActive || desiredAlpha != m_opacityTargetAlpha)
    {
        m_hoverTargetActive = shouldHover;
        m_opacityStartAlpha = m_currentWindowAlpha;
        m_opacityTargetAlpha = desiredAlpha;
        m_opacityTransitionStartTick = GetTickCount64();
    }

    if (m_currentWindowAlpha == m_opacityTargetAlpha)
    {
        return false;
    }

    const float elapsedSeconds = static_cast<float>(GetTickCount64() - m_opacityTransitionStartTick) / 1000.0f;
    const float rawT = AppState::Clamp(elapsedSeconds / m_appState->settings.hoverOpacityTransitionSeconds, 0.0f, 1.0f);
    const float easedT = 1.0f - std::pow(1.0f - rawT, 3.0f);
    const float alpha = static_cast<float>(m_opacityStartAlpha) +
        (static_cast<float>(m_opacityTargetAlpha) - static_cast<float>(m_opacityStartAlpha)) * easedT;

    SetWindowOpacity(static_cast<BYTE>(AppState::ClampInt(static_cast<int>(alpha + 0.5f), 0, 255)));
    return m_currentWindowAlpha != m_opacityTargetAlpha;
}

void WindowManager::OnHotkey(int hotkeyId)
{
    if (hotkeyId == kHotkeyTogglePassThrough)
    {
        TogglePassThroughMode();
    }
}

void WindowManager::OnCommand(UINT commandId)
{
    switch (commandId)
    {
    case kTrayMenuOpenSettings:
        OpenSettingsApp();
        break;
    case kTrayMenuTogglePassThrough:
        TogglePassThroughMode();
        break;
    case kTrayMenuBrightness600:
        SetTargetNits(600.0f);
        break;
    case kTrayMenuBrightness800:
        SetTargetNits(800.0f);
        break;
    case kTrayMenuBrightness1000:
        SetTargetNits(1000.0f);
        break;
    case kTrayMenuBrightness1200:
        SetTargetNits(1200.0f);
        break;
    case kTrayMenuFrame5:
        SetFrameMarginRatio(0.05f);
        break;
    case kTrayMenuFrame8:
        SetFrameMarginRatio(0.08f);
        break;
    case kTrayMenuFrame10:
        SetFrameMarginRatio(0.10f);
        break;
    case kTrayMenuFrame12:
        SetFrameMarginRatio(0.12f);
        break;
    case kTrayMenuHoverAlpha20:
        SetHoverAlpha(20);
        break;
    case kTrayMenuHoverAlpha40:
        SetHoverAlpha(40);
        break;
    case kTrayMenuHoverAlpha70:
        SetHoverAlpha(70);
        break;
    case kTrayMenuLanguageZhHans:
        SetLanguage(AppLanguage::ZhHans);
        break;
    case kTrayMenuLanguageZhHant:
        SetLanguage(AppLanguage::ZhHant);
        break;
    case kTrayMenuLanguageEnUs:
        SetLanguage(AppLanguage::EnUs);
        break;
    case kTrayMenuExit:
        DestroyWindow(m_hwnd);
        break;
    default:
        break;
    }
}

void WindowManager::OnSize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    m_appState->clientWidth = width;
    m_appState->clientHeight = height;
    m_appState->UpdateMarginsFromSettings();

    m_renderer->Resize(width, height);
    m_appState->hdrActive = m_renderer->IsHDRSupported();
    UpdateWindowRegion();

    m_appState->renderNeeded = true;
}

void WindowManager::OnDestroy()
{
    PostQuitMessage(0);
}

int WindowManager::Run()
{
    m_appState->rendererReady = false;
    m_appState->hdrActive = false;

    // Initialize renderer first (before showing window)
    if (!m_renderer->Initialize(m_hwnd, m_appState->clientWidth, m_appState->clientHeight))
    {
        std::printf("Renderer initialization failed.\n");
        return 1;
    }

    // Apply initial region BEFORE showing window
    UpdateWindowRegion();

    // NOW show window
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    m_appState->hdrActive = m_renderer->RefreshHDRState(m_hwnd);
    m_appState->rendererReady = true;

    // Force initial render
    m_appState->renderNeeded = true;
    m_renderer->Render(*m_appState);

    // Message loop
    MSG message = {};
    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        else
        {
            const bool opacityAnimating = UpdateHoverOpacity();
            if (m_appState->renderNeeded || m_renderer->IsStartupBreathingActive(*m_appState) || opacityAnimating)
            {
                m_renderer->Render(*m_appState);
                m_appState->renderNeeded = m_renderer->IsStartupBreathingActive(*m_appState) || opacityAnimating;
            }
            else
            {
                Sleep(1);
            }
        }
    }

    m_renderer->Shutdown();

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK WindowManager::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowManager* pThis = nullptr;

    if (message == WM_CREATE)
    {
        pThis = g_pWindowManager;
    }
    else
    {
        pThis = g_pWindowManager;
    }

    if (!pThis)
    {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_LBUTTONDBLCLK:
        DestroyWindow(hwnd);
        return 0;

    case WM_SIZE:
        pThis->OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        pThis->ApplyScreenInset();
        return 0;

    case WM_HOTKEY:
        pThis->OnHotkey(static_cast<int>(wParam));
        return 0;

    case WM_COMMAND:
        pThis->OnCommand(LOWORD(wParam));
        return 0;

    case kTrayIconMessage:
        pThis->OnTrayIcon(lParam);
        return 0;

    case kIpcApplyMessage:
        pThis->OnIpcCommand(reinterpret_cast<IpcCommand*>(lParam));
        return 0;

    case WM_DESTROY:
        pThis->OnDestroy();
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

} // namespace hdr_driver
