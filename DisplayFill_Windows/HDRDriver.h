#ifndef HDR_DRIVER_H
#define HDR_DRIVER_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

namespace hdr_driver
{

//=============================================================================
// Constants
//=============================================================================

constexpr wchar_t kWindowClassName[] = L"HdrScreenFillWindow";
constexpr UINT kTrayIconMessage = WM_APP + 1;
constexpr UINT kIpcApplyMessage = WM_APP + 2;
constexpr wchar_t kIpcPipeName[] = L"\\\\.\\pipe\\DisplayFill_Windows_Control";

// scRGB 中 1.0 通常约等于 80 nits，程序会用 targetNits / 80 计算 HDR 白色强度。
constexpr float kSdrReferenceWhiteNits = 80.0f;

// 目标 HDR 亮度，单位 nits。常见设置：600、800、1000、1200。
// 如果屏幕太刺眼，请先降低这个值，而不是降低窗口透明度。
constexpr float kDefaultTargetNits = 1000.0f;

// 相框左右边距比例。0.08 表示按当前主显示器物理宽度取 8%。
// 例如 1920 宽度时，左右各约 154 像素。
constexpr float kFrameMarginXRatio = 0.08f;

// 相框上下边距比例。0.08 表示按当前主显示器物理高度取 8%。
// 例如 1080 高度时，上下各约 86 像素。
constexpr float kFrameMarginYRatio = 0.08f;

// 中间挖空区域的圆角半径，单位像素。数值越大，圆角越明显。
// 注意：真实点击区域仍由 Win32 Region 决定，视觉平滑由下面的羽化参数增强。
constexpr int kHoleCornerRadius = 60;

// 视觉圆角羽化宽度，单位像素。数值越大，圆角边缘越柔和、越不容易看到像素阶梯。
// 推荐范围：12.0 - 48.0。过大时边缘会显得发虚。
constexpr float kVisualCornerFeatherPixels = 24.0f;

// 正常状态窗口不透明度。255 为完全不透明，0 为完全透明。
constexpr BYTE kNormalWindowAlpha = 255;

// 鼠标悬停到补光相框区域时的目标透明度。数值越小越透明。
// 推荐范围：20 - 120。当前 40 表示非常透明，但仍能看到补光。
constexpr BYTE kMouseHoverWindowAlpha = 40;

// 鼠标悬停透明度变化时长，单位秒。程序使用非线性 ease-out 曲线。
// 数值越大变化越慢，越小越接近瞬间变化。
constexpr float kHoverOpacityTransitionSeconds = 0.45f;

// 启动亮度呼吸的起始亮度比例。0.20 表示从 20% 亮度开始。
constexpr float kStartupBreathMinBrightness = 0.20f;

// 启动亮度呼吸的结束亮度比例。1.00 表示最终达到 100% 亮度。
constexpr float kStartupBreathMaxBrightness = 1.00f;

// 启动亮度呼吸持续时间，单位秒。0 表示理论上可关闭，但建议保留一个很小正数。
constexpr float kStartupBreathDurationSeconds = 3.0f;

// Ctrl+F6 全局热键 ID：切换穿透模式/非穿透模式。这个值只是内部 ID，一般不用改。
constexpr int kHotkeyTogglePassThrough = 4001;

constexpr UINT kTrayMenuOpenSettings = 5001;
constexpr UINT kTrayMenuTogglePassThrough = 5002;
constexpr UINT kTrayMenuBrightness600 = 5010;
constexpr UINT kTrayMenuBrightness800 = 5011;
constexpr UINT kTrayMenuBrightness1000 = 5012;
constexpr UINT kTrayMenuBrightness1200 = 5013;
constexpr UINT kTrayMenuFrame5 = 5020;
constexpr UINT kTrayMenuFrame8 = 5021;
constexpr UINT kTrayMenuFrame10 = 5022;
constexpr UINT kTrayMenuFrame12 = 5023;
constexpr UINT kTrayMenuHoverAlpha20 = 5030;
constexpr UINT kTrayMenuHoverAlpha40 = 5031;
constexpr UINT kTrayMenuHoverAlpha70 = 5032;
constexpr UINT kTrayMenuLanguageZhHans = 5040;
constexpr UINT kTrayMenuLanguageZhHant = 5041;
constexpr UINT kTrayMenuLanguageEnUs = 5042;
constexpr UINT kTrayMenuExit = 5099;

enum class AppLanguage
{
    ZhHans,
    ZhHant,
    EnUs,
};

// 运行时设置。托盘菜单会直接修改这里的值，因此这些参数不再只依赖 constexpr。
struct AppSettings
{
    float targetNits = kDefaultTargetNits;
    float frameMarginXRatio = kFrameMarginXRatio;
    float frameMarginYRatio = kFrameMarginYRatio;
    int holeCornerRadius = kHoleCornerRadius;
    float visualCornerFeatherPixels = kVisualCornerFeatherPixels;
    BYTE normalWindowAlpha = kNormalWindowAlpha;
    BYTE mouseHoverWindowAlpha = kMouseHoverWindowAlpha;
    float hoverOpacityTransitionSeconds = kHoverOpacityTransitionSeconds;
    float startupBreathMinBrightness = kStartupBreathMinBrightness;
    float startupBreathMaxBrightness = kStartupBreathMaxBrightness;
    float startupBreathDurationSeconds = kStartupBreathDurationSeconds;
    bool passThroughMode = true;
    AppLanguage language = AppLanguage::ZhHans;
};

//=============================================================================
// Forward Declarations
//=============================================================================

class Renderer;
class WindowManager;
class IpcServer;

enum class IpcCommandType
{
    SetValue,
    TogglePassThrough,
    Exit,
};

enum class IpcValueType
{
    Number,
    Boolean,
    Language,
};

struct IpcCommand
{
    IpcCommandType commandType = IpcCommandType::SetValue;
    IpcValueType valueType = IpcValueType::Number;
    wchar_t key[64] = {};
    double numberValue = 0.0;
    bool boolValue = false;
    AppLanguage languageValue = AppLanguage::ZhHans;
};

//=============================================================================
// AppState
//=============================================================================

struct AppState
{
    AppSettings settings;

    // HDR Rendering
    bool hdrActive = false;

    // Rendering control
    bool renderNeeded = true;

    // RECT for current hole in client coordinates
    RECT holeRect = {};

    // Current client area dimensions
    int clientWidth = 0;
    int clientHeight = 0;
    int monitorWidth = 0;
    int monitorHeight = 0;

    // Fixed frame margins calculated from current monitor resolution.
    int marginLeft = 0;
    int marginRight = 0;
    int marginTop = 0;
    int marginBottom = 0;

    AppState() = default;

    // Clamp helper for floats
    static float Clamp(float value, float minVal, float maxVal)
    {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }

    // Clamp helper for ints
    static int ClampInt(int value, int minVal, int maxVal)
    {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }

    float GetWhiteLevel() const
    {
        return hdrActive ? (settings.targetNits / kSdrReferenceWhiteNits) : 1.0f;
    }

    void GetClearColor(float rgba[4]) const
    {
        const float whiteLevel = GetWhiteLevel();
        rgba[0] = whiteLevel;
        rgba[1] = whiteLevel;
        rgba[2] = whiteLevel;
        rgba[3] = 1.0f;
    }

    // Calculate hole rect from current scale values
    void UpdateHoleRect()
    {
        if (clientWidth <= 0 || clientHeight <= 0)
        {
            holeRect = {};
            return;
        }

        const int left = ClampInt(marginLeft, 0, clientWidth - 1);
        const int top = ClampInt(marginTop, 0, clientHeight - 1);
        const int right = ClampInt(clientWidth - marginRight, left + 1, clientWidth);
        const int bottom = ClampInt(clientHeight - marginBottom, top + 1, clientHeight);

        holeRect = { left, top, right, bottom };
    }

    void UpdateMarginsFromSettings()
    {
        if (clientWidth <= 0 || clientHeight <= 0)
        {
            return;
        }

        const int sourceWidth = monitorWidth > 0 ? monitorWidth : clientWidth;
        const int sourceHeight = monitorHeight > 0 ? monitorHeight : clientHeight;
        marginLeft = ClampInt(static_cast<int>(static_cast<float>(sourceWidth) * settings.frameMarginXRatio), 1, clientWidth / 2 - 1);
        marginRight = marginLeft;
        marginTop = ClampInt(static_cast<int>(static_cast<float>(sourceHeight) * settings.frameMarginYRatio), 1, clientHeight / 2 - 1);
        marginBottom = marginTop;
        UpdateHoleRect();
        renderNeeded = true;
    }
};

//=============================================================================
// Renderer Class
//=============================================================================

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();

    void Resize(int width, int height);
    void Render(const AppState& state);
    bool IsStartupBreathingActive(const AppState& state) const;
    bool IsHDRSupported() const { return m_hdrActive; }

    void SetHDRActive(bool active) { m_hdrActive = active; }

private:
    bool CreateDeviceAndSwapChain(HWND hwnd, int width, int height);
    bool CreateRenderTarget();
    bool CreateRenderPipeline();
    void ReleaseRenderTarget();
    float GetStartupBrightnessScale(const AppState& state) const;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> m_context1;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain3;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;

    bool m_hdrActive = false;
    HWND m_hwnd = nullptr;
    ULONGLONG m_startTick = 0;
};

//=============================================================================
// WindowManager Class
//=============================================================================

class WindowManager
{
public:
    WindowManager();
    ~WindowManager();

    bool Initialize(HINSTANCE instance, AppState* appState, Renderer* renderer);
    void Shutdown();

    int Run();

    HWND GetWindowHandle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow(HINSTANCE instance);

    void UpdateWindowRegion();
    void SetWindowOpacity(BYTE alpha);
    void RegisterHotkeys();
    void UnregisterHotkeys();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayTooltip();
    void ShowTrayMenu();
    void OnTrayIcon(LPARAM lParam);
    void OpenSettingsApp();
    void ApplyPassThroughMode();
    void TogglePassThroughMode();
    void ApplyFrameSettings();
    void SetTargetNits(float nits);
    void SetFrameMarginRatio(float ratio);
    void SetHoverAlpha(BYTE alpha);
    void SetLanguage(AppLanguage language);
    void OnIpcCommand(IpcCommand* command);
    bool IsCursorOverFrame() const;
    bool UpdateHoverOpacity();

    void OnSize(int width, int height);
    void OnHotkey(int hotkeyId);
    void OnCommand(UINT commandId);
    void OnDestroy();

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    bool m_hoverTargetActive = false;
    BYTE m_currentWindowAlpha = kNormalWindowAlpha;
    BYTE m_opacityStartAlpha = kNormalWindowAlpha;
    BYTE m_opacityTargetAlpha = kNormalWindowAlpha;
    ULONGLONG m_opacityTransitionStartTick = 0;
    NOTIFYICONDATAW m_trayIconData = {};
    bool m_trayIconAdded = false;
    std::unique_ptr<IpcServer> m_ipcServer;

    AppState* m_appState = nullptr;
    Renderer* m_renderer = nullptr;
};

class IpcServer
{
public:
    IpcServer(HWND targetWindow, AppState* appState);
    ~IpcServer();

    bool Start();
    void Stop();

private:
    void WorkerLoop();
    void HandleClient(HANDLE pipeHandle);
    std::string HandleMessage(const std::string& message);
    std::string BuildStateJson() const;
    bool TryCreateCommand(const std::string& message, IpcCommand& command) const;
    void PostCommand(const IpcCommand& command) const;

    HWND m_targetWindow = nullptr;
    AppState* m_appState = nullptr;
    std::atomic_bool m_running = false;
    std::thread m_worker;
};

//=============================================================================
// Utility Functions
//=============================================================================

void PrintLastError(const char* message, HRESULT hr);
void EnsureConsole();

} // namespace hdr_driver

#endif // HDR_DRIVER_H
