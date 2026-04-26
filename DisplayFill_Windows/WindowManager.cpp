#include "HDRDriver.h"
#include <cmath>
#include <cstdio>
#include <cwchar>

namespace hdr_driver
{

// Global instance pointers for static window proc
namespace
{
    WindowManager* g_pWindowManager = nullptr;

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

    return true;
}

void WindowManager::Shutdown()
{
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

    HMONITOR primaryMonitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(primaryMonitor, &monitorInfo))
    {
        std::printf("GetMonitorInfoW failed. GetLastError=%lu\n", GetLastError());
        monitorInfo.rcMonitor = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    }

    const int monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    const int monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

    // Get work area to avoid taskbar for the visible window bounds.
    RECT workArea = {};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0))
    {
        std::printf("SystemParametersInfoW(SPI_GETWORKAREA) failed. Using screen metrics.\n");
        workArea.left = 0;
        workArea.top = 0;
        workArea.right = GetSystemMetrics(SM_CXSCREEN);
        workArea.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    const int windowWidth = workArea.right - workArea.left;
    const int windowHeight = workArea.bottom - workArea.top;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        className,
        L"HDR Screen Fill Light",
        WS_POPUP,
        workArea.left,
        workArea.top,
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
    m_appState->monitorWidth = monitorWidth;
    m_appState->monitorHeight = monitorHeight;
    m_appState->UpdateMarginsFromSettings();

    std::printf("Monitor resolution: %dx%d\n", monitorWidth, monitorHeight);
    std::printf("Work area: %dx%d at (%ld,%ld)\n", windowWidth, windowHeight, workArea.left, workArea.top);
    std::printf("Frame margins: left/right=%d px, top/bottom=%d px\n", m_appState->marginLeft, m_appState->marginTop);

    return true;
}

void WindowManager::UpdateWindowRegion()
{
    if (!m_hwnd || m_appState->clientWidth <= 0 || m_appState->clientHeight <= 0)
    {
        return;
    }

    RECT clientRect = { 0, 0, m_appState->clientWidth, m_appState->clientHeight };

    HRGN fullRegion = CreateRectRgn(clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);
    HRGN holeRegion = CreateRoundRectRgn(
        m_appState->holeRect.left,
        m_appState->holeRect.top,
        m_appState->holeRect.right,
        m_appState->holeRect.bottom,
        m_appState->settings.holeCornerRadius * 2,
        m_appState->settings.holeCornerRadius * 2);

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
    m_trayIconData.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(m_trayIconData.szTip, GetText(m_appState->settings.language).tooltip);

    m_trayIconAdded = Shell_NotifyIconW(NIM_ADD, &m_trayIconData) != FALSE;
    if (m_trayIconAdded)
    {
        m_trayIconData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &m_trayIconData);
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

    AppendMenuW(menu, MF_STRING | MF_DISABLED, kTrayMenuOpenSettings, text.settingsTitle);
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
}

void WindowManager::ApplyFrameSettings()
{
    m_appState->UpdateMarginsFromSettings();
    UpdateWindowRegion();
    m_appState->renderNeeded = true;
}

void WindowManager::SetTargetNits(float nits)
{
    m_appState->settings.targetNits = nits;
    m_appState->renderNeeded = true;
    std::printf("Target brightness: %.0f nits\n", nits);
}

void WindowManager::SetFrameMarginRatio(float ratio)
{
    m_appState->settings.frameMarginXRatio = ratio;
    m_appState->settings.frameMarginYRatio = ratio;
    ApplyFrameSettings();
    std::printf("Frame margin ratio: %.0f%%\n", ratio * 100.0f);
}

void WindowManager::SetHoverAlpha(BYTE alpha)
{
    m_appState->settings.mouseHoverWindowAlpha = alpha;
    m_opacityTargetAlpha = IsCursorOverFrame() ? alpha : m_appState->settings.normalWindowAlpha;
    std::printf("Hover alpha: %u\n", static_cast<unsigned int>(alpha));
}

void WindowManager::SetLanguage(AppLanguage language)
{
    m_appState->settings.language = language;
    UpdateTrayTooltip();
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
    m_appState->clientWidth = width;
    m_appState->clientHeight = height;
    m_appState->UpdateHoleRect();

    m_renderer->Resize(width, height);
    UpdateWindowRegion();

    m_appState->renderNeeded = true;
}

void WindowManager::OnDestroy()
{
    PostQuitMessage(0);
}

int WindowManager::Run()
{
    // Initialize renderer first (before showing window)
    if (!m_renderer->Initialize(m_hwnd, m_appState->clientWidth, m_appState->clientHeight))
    {
        std::printf("Renderer initialization failed.\n");
        return 1;
    }

    m_appState->hdrActive = m_renderer->IsHDRSupported();

    // Apply initial region BEFORE showing window
    UpdateWindowRegion();

    // NOW show window
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

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

    case WM_HOTKEY:
        pThis->OnHotkey(static_cast<int>(wParam));
        return 0;

    case WM_COMMAND:
        pThis->OnCommand(LOWORD(wParam));
        return 0;

    case kTrayIconMessage:
        pThis->OnTrayIcon(lParam);
        return 0;

    case WM_DESTROY:
        pThis->OnDestroy();
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

} // namespace hdr_driver
