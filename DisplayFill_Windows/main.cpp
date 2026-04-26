#include "HDRDriver.h"
#include <memory>

using namespace hdr_driver;

int main()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    EnsureConsole();

    std::printf("=== HDR Screen Fill Light ===\n");
    std::printf("ESC or Double-click: Exit\n");
    std::printf("Ctrl+F6: Toggle pass-through mode, default ON\n");
    std::printf("Tray icon: right-click for settings\n");
    std::printf("===============================\n\n");

    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!instance)
    {
        std::printf("GetModuleHandleW failed.\n");
        return 1;
    }

    // Create shared state
    AppState appState;

    // Create renderer
    auto renderer = std::make_unique<Renderer>();

    // Create window manager
    auto windowManager = std::make_unique<WindowManager>();

    // Initialize window manager
    if (!windowManager->Initialize(instance, &appState, renderer.get()))
    {
        std::printf("WindowManager initialization failed.\n");
        return 1;
    }

    // Run event loop
    int result = windowManager->Run();

    // Cleanup (RAII)
    windowManager->Shutdown();

    std::printf("Application exited with code %d\n", result);

    return result;
}

#ifdef _WIN32
int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int showCommand)
{
    return main();
}
#endif
