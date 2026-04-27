# WinUI 3 Settings Window Design

This folder contains a WinUI 3 settings-window design skeleton for the HDR fill-light app.

It is intentionally not wired into the current pure Win32/D3D11 `.vcxproj` yet, because the existing project does not include Windows App SDK or C++/WinRT package references. Directly adding these files to the current project without those dependencies would break the build.

Recommended integration path:

1. Install Visual Studio workloads/components:
   - Desktop development with C++
   - Windows App SDK C++ templates
   - C++/WinRT tooling

2. Add packages to a WinUI 3 C++ project:
   - `Microsoft.WindowsAppSDK`
   - `Microsoft.Windows.CppWinRT`

3. Reuse the files in this folder:
   - `SettingsWindow.xaml`
   - `SettingsWindow.xaml.h`
   - `SettingsWindow.xaml.cpp`

4. Bridge setting changes back to the existing Win32 host through a small adapter around `AppSettings`.

The current application still exposes runtime settings through the tray menu, so this design can be integrated later without blocking the existing Win32 build.
