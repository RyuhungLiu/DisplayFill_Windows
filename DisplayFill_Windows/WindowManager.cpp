#include "HDRDriver.h"
#include <cmath>
#include <cstdio>

namespace hdr_driver
{

// Global instance pointers for static window proc
namespace
{
    WindowManager* g_pWindowManager = nullptr;
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

    return true;
}

void WindowManager::Shutdown()
{
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

    SetWindowOpacity(kNormalWindowAlpha);

    // Initialize state dimensions
    m_appState->clientWidth = windowWidth;
    m_appState->clientHeight = windowHeight;
    m_appState->marginLeft = AppState::ClampInt(static_cast<int>(static_cast<float>(monitorWidth) * kFrameMarginXRatio), 1, windowWidth / 2 - 1);
    m_appState->marginRight = m_appState->marginLeft;
    m_appState->marginTop = AppState::ClampInt(static_cast<int>(static_cast<float>(monitorHeight) * kFrameMarginYRatio), 1, windowHeight / 2 - 1);
    m_appState->marginBottom = m_appState->marginTop;
    m_appState->UpdateHoleRect();

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
        kHoleCornerRadius * 2,
        kHoleCornerRadius * 2);

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

void WindowManager::ApplyPassThroughMode()
{
    if (!m_hwnd)
    {
        return;
    }

    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TOPMOST;
    if (m_passThroughMode)
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

    std::printf("Pass-through mode: %s\n", m_passThroughMode ? "ON" : "OFF");
}

void WindowManager::TogglePassThroughMode()
{
    m_passThroughMode = !m_passThroughMode;
    ApplyPassThroughMode();
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
    const BYTE desiredAlpha = shouldHover ? kMouseHoverWindowAlpha : kNormalWindowAlpha;

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
    const float rawT = AppState::Clamp(elapsedSeconds / kHoverOpacityTransitionSeconds, 0.0f, 1.0f);
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
            if (m_appState->renderNeeded || m_renderer->IsStartupBreathingActive() || opacityAnimating)
            {
                m_renderer->Render(*m_appState);
                m_appState->renderNeeded = m_renderer->IsStartupBreathingActive() || opacityAnimating;
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

    case WM_DESTROY:
        pThis->OnDestroy();
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

} // namespace hdr_driver
