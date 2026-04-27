# DisplayFill Windows

English | [简体中文](#displayfill-windows-简体中文)

DisplayFill Windows is a Windows HDR screen fill-light application. It renders a bright HDR border around the desktop while leaving the center area open, so the screen can act as a soft fill light without blocking normal desktop usage.

**OLED / HDR safety warning:** This application can render very bright static areas for long periods of time. On OLED displays, this may accelerate pixel aging, cause image retention, or increase the risk of burn-in. Use lower brightness, avoid long unattended sessions, move/resize content when possible, and use the application at your own risk.

The project currently uses a two-process architecture:

```text
DisplayFill_Settings.exe   WinUI 3 settings app and user entry point
DisplayFill_Windows.exe    Win32 + Direct3D 11 HDR rendering engine
```

The settings app starts and controls the HDR engine through a named pipe. The HDR engine owns the tray icon and the rendering window.

## Features

- HDR fill-light frame rendered with Win32, Direct3D 11, DXGI, and FP16 swap chain.
- scRGB HDR output using `DXGI_FORMAT_R16G16B16A16_FLOAT` and `DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709`.
- Automatic HDR detection with SDR fallback when HDR is not available.
- Border-style fill light with a transparent/open center area.
- Win32 region-based hit-test hole for the center area.
- Mouse pass-through mode for interacting with windows behind the fill light.
- Smooth visual rounded corners and feathered edges in the D3D shader.
- Startup brightness breathing animation.
- Pointer-hover opacity transition for the fill-light frame.
- WinUI 3 settings window for runtime control.
- Tray menu for opening settings, toggling pass-through mode, and exiting.
- Named Pipe IPC for real-time settings updates.
- Release packaging for both x64 and ARM64.
- Two release package types: SelfContained and FrameworkDependent.

## Architecture

```text
+----------------------------+
| DisplayFill_Settings.exe   |
| WinUI 3 settings UI        |
| Starts engine if needed    |
+-------------+--------------+
              |
              | Named Pipe
              | \\.\pipe\DisplayFill_Windows_Control
              v
+-------------+--------------+
| DisplayFill_Windows.exe    |
| Win32 window manager       |
| D3D11 HDR renderer         |
| Tray icon                  |
+----------------------------+
```

`DisplayFill_Settings.exe` is intended to be the main user entry point. If the HDR engine is not already running, the settings app starts `DisplayFill_Windows.exe` from the same folder and then connects to it.

`DisplayFill_Windows.exe` is the background rendering engine. It creates the HDR fill-light window, manages pass-through behavior, renders the border, and exposes a pipe server for control commands.

## Repository Layout

```text
DisplayFill_Windows.slnx              Solution file
quick-build.cmd                       Release build and packaging script

DisplayFill_Windows/                  HDR rendering engine project
  HDRDriver.h                         Shared engine constants, settings, declarations
  main.cpp                            Engine entry point
  WindowManager.cpp                   Win32 window, tray icon, input, IPC command apply
  Renderer.cpp                        D3D11 renderer and HDR/scRGB output
  IpcServer.cpp                       Named Pipe server
  DisplayFill_Windows.vcxproj         Engine project file

DisplayFill_Settings/                 WinUI 3 settings app project
  App.xaml                            WinUI app definition
  MainWindow.xaml                     Settings UI layout
  MainWindow.xaml.cpp                 Settings UI logic and IPC calls
  PipeClient.h / PipeClient.cpp       Named Pipe client and engine startup logic
  DisplayFill_Settings.vcxproj        WinUI 3 project file

run/                                  Generated runtime folders, ignored by git
x64/                                  Generated x64 build output, ignored by git
ARM64/                                Generated ARM64 build output, ignored by git
```

## Runtime Controls

The settings window exposes runtime controls for:

- Target HDR brightness in nits.
- Fill-light frame margin ratio.
- Rounded corner radius.
- Visual corner feathering.
- Pass-through mode.
- Pointer-hover opacity.
- Pointer-hover transition duration.
- Language selection.
- Engine refresh and exit actions.

The engine also supports basic direct controls:

```text
Ctrl+F6    Toggle pass-through mode
ESC        Exit engine
```

The tray icon can be used to open the settings app, toggle pass-through mode, or exit the engine.

## IPC Protocol

The HDR engine listens on this named pipe:

```text
\\.\pipe\DisplayFill_Windows_Control
```

Example messages:

```json
{"type":"getState"}
```

```json
{"type":"set","key":"targetNits","value":800}
```

```json
{"type":"set","key":"frameMarginXRatio","value":0.08}
```

```json
{"type":"set","key":"frameMarginYRatio","value":0.08}
```

```json
{"type":"set","key":"cornerRadius","value":80}
```

```json
{"type":"set","key":"visualCornerFeatherPixels","value":24}
```

```json
{"type":"set","key":"hoverAlpha","value":40}
```

```json
{"type":"set","key":"hoverTransitionSeconds","value":0.45}
```

```json
{"type":"set","key":"passThroughMode","value":true}
```

```json
{"type":"set","key":"language","value":"zh-Hans"}
```

```json
{"type":"command","name":"togglePassThrough"}
```

```json
{"type":"command","name":"exit"}
```

Supported language values:

```text
zh-Hans
zh-Hant
en-US
```

## Requirements

### Runtime

- Windows 10/11.
- Direct3D 11 capable GPU.
- HDR display and Windows HDR enabled for true HDR brightness output.
- For FrameworkDependent packages only: Microsoft Windows App Runtime 1.8.

SelfContained packages include the Windows App SDK / WinUI 3 runtime files and do not require a separate Windows App Runtime installation.

FrameworkDependent packages are smaller, but the target machine must already have Windows App Runtime 1.8 installed.

Windows App Runtime downloads:

```text
https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads
```

### Build

- Visual Studio 2026 or newer compatible C++ toolchain.
- MSBuild at:

```text
C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe
```

- Windows SDK with Direct3D 11, DXGI, and WinUI 3 / Windows App SDK dependencies restored.

## Build And Package

Use the root script:

```cmd
quick-build.cmd x64
```

or:

```cmd
quick-build.cmd arm64
```

A platform argument is required. Running the script without `x64` or `arm64` will only print usage and will not build.

The script performs these steps:

1. Stops running `DisplayFill_Settings` and `DisplayFill_Windows` processes.
2. Cleans old runtime folders and old release zip files for the selected platform.
3. Builds Release SelfContained.
4. Creates a large unzip-and-run runtime folder.
5. Creates a SelfContained release zip.
6. Builds Release FrameworkDependent.
7. Creates a small runtime folder that depends on Windows App Runtime 1.8.
8. Creates a FrameworkDependent release zip.
9. Verifies that output folders and zip files are not empty.

## Release Outputs

For ARM64:

```text
run\Release-ARM64-SelfContained
run\Release-ARM64-FrameworkDependent
DisplayFill_Windows_ARM64_SelfContained_Release.zip
DisplayFill_Windows_ARM64_FrameworkDependent_Release.zip
```

For x64:

```text
run\Release-x64-SelfContained
run\Release-x64-FrameworkDependent
DisplayFill_Windows_x64_SelfContained_Release.zip
DisplayFill_Windows_x64_FrameworkDependent_Release.zip
```

## Package Types

### SelfContained

Use this package when you want the simplest installation experience.

```text
DisplayFill_Windows_ARM64_SelfContained_Release.zip
DisplayFill_Windows_x64_SelfContained_Release.zip
```

Characteristics:

- Larger package.
- Includes Windows App SDK / WinUI 3 runtime files.
- Does not require the user to install Windows App Runtime separately.
- Recommended for quick testing and portable distribution.

Run with:

```text
Start-DisplayFill.cmd
```

or:

```text
DisplayFill_Settings.exe
```

### FrameworkDependent

Use this package when package size matters and the user can install dependencies.

```text
DisplayFill_Windows_ARM64_FrameworkDependent_Release.zip
DisplayFill_Windows_x64_FrameworkDependent_Release.zip
```

Characteristics:

- Smaller package.
- Does not include the full Windows App SDK / WinUI 3 runtime.
- Requires Microsoft Windows App Runtime 1.8 for the matching CPU architecture.
- Useful for users who already have the runtime installed.

Run with:

```text
Start-DisplayFill.cmd
```

or:

```text
DisplayFill_Settings.exe
```

If `DisplayFill_Settings.exe` does not start, install Windows App Runtime 1.8.

## OLED And HDR Safety Warning

This application intentionally creates bright areas near the edge of the display. On OLED and other self-emissive panels, static bright UI can accelerate uneven pixel wear.

Potential risks include:

- Temporary image retention.
- Permanent burn-in.
- Accelerated pixel aging.
- Uneven brightness or color shift near the frame area.

Recommended precautions:

- Use the lowest brightness that works for your lighting setup.
- Avoid leaving the fill-light frame on for long unattended periods.
- Avoid using maximum HDR brightness on OLED displays.
- Prefer shorter sessions when using OLED panels.
- Let your display run its normal pixel refresh / panel care routines.
- Use at your own risk.

## Running From Source Build Output

After running `quick-build.cmd`, use the generated folders under `run/`.

For example:

```text
run\Release-ARM64-SelfContained\Start-DisplayFill.cmd
```

The settings app starts the HDR engine automatically. Both executables should remain in the same folder:

```text
DisplayFill_Settings.exe
DisplayFill_Windows.exe
```

## Tunable Engine Defaults

Most engine defaults are defined in:

```text
DisplayFill_Windows\HDRDriver.h
```

Important settings include:

- `targetNits`: HDR target brightness in nits.
- `frameMarginXRatio`: left/right frame width ratio.
- `frameMarginYRatio`: top/bottom frame height ratio.
- `holeCornerRadius`: physical center cutout corner radius.
- `visualCornerFeatherPixels`: shader-based visual edge feathering.
- `normalWindowAlpha`: normal fill-light window alpha.
- `mouseHoverWindowAlpha`: fill-light alpha while pointer is near the frame.
- `hoverOpacityTransitionSeconds`: hover opacity transition duration.
- `startupBreathMinBrightness`: startup breath minimum brightness ratio.
- `startupBreathMaxBrightness`: startup breath maximum brightness ratio.
- `startupBreathDurationSeconds`: startup breath duration.

At runtime, most of these values can be changed from the WinUI 3 settings window without recompiling.

## Notes About HDR

To get true HDR brightness:

- Use an HDR-capable display.
- Enable HDR in Windows display settings.
- Make sure the application is running on the HDR display.

If HDR is unavailable, the engine falls back to SDR output. The app will still run, but the fill-light will not exceed SDR brightness.

High brightness can cause eye strain. Use reasonable brightness values for your display and room lighting.

## Development Notes

- `DisplayFill_Settings.exe` is an unpackaged WinUI 3 app.
- SelfContained builds are large because they include Windows App SDK runtime files such as `Microsoft.ui.xaml.dll`, `Microsoft.UI.Xaml.Controls.dll`, `Microsoft.WindowsAppRuntime.dll`, `DWriteCore.dll`, and related resources.
- FrameworkDependent builds are smaller because they depend on an installed Windows App Runtime.

## Acknowledgements

The idea was inspired by [dashhuang/DisplayFill](https://github.com/dashhuang/DisplayFill). This Windows version extends the concept with a Win32/D3D11 HDR rendering engine and a WinUI 3 settings frontend.

# DisplayFill Windows 简体中文

[English](#displayfill-windows) | 简体中文

DisplayFill Windows 是一个 Windows HDR 屏幕补光灯应用。它会在桌面边缘渲染高亮 HDR 补光相框，同时保留中间区域，便于你继续使用桌面、终端、IDE 或其他应用。

**OLED / HDR 安全警告：** 本应用可能长时间渲染非常明亮且相对静止的画面区域。对于 OLED 屏幕，这可能加速像素老化，造成临时残影，或增加永久烧屏风险。请降低亮度，避免长时间无人值守运行，尽量不要在 OLED 屏幕上长时间使用最高 HDR 亮度，并自行承担使用风险。

当前项目采用双进程架构：

```text
DisplayFill_Settings.exe   WinUI 3 设置程序和用户入口
DisplayFill_Windows.exe    Win32 + Direct3D 11 HDR 渲染引擎
```

设置程序通过 Named Pipe 启动并控制 HDR 引擎。HDR 引擎负责托盘图标、置顶渲染窗口和 HDR 补光输出。

## 功能特性

- 使用 Win32、Direct3D 11、DXGI 和 FP16 swap chain 渲染 HDR 补光相框。
- 使用 `DXGI_FORMAT_R16G16B16A16_FLOAT` 和 `DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709` 输出 scRGB HDR。
- 自动检测 HDR，不可用时回退到 SDR。
- 边框式补光，中间区域保持开放。
- 使用 Win32 Region 实现中间区域命中测试挖空。
- 支持鼠标穿透模式，可点击补光窗口后方应用。
- 使用 D3D shader 实现视觉圆角和边缘羽化。
- 支持启动亮度呼吸动画。
- 支持鼠标靠近时补光相框透明度过渡。
- 提供 WinUI 3 设置窗口进行实时调节。
- 提供托盘菜单打开设置、切换穿透模式和退出。
- 使用 Named Pipe IPC 实时更新设置。

## 架构

```text
+----------------------------+
| DisplayFill_Settings.exe   |
| WinUI 3 设置界面           |
| 必要时启动渲染引擎         |
+-------------+--------------+
              |
              | Named Pipe
              | \\.\pipe\DisplayFill_Windows_Control
              v
+-------------+--------------+
| DisplayFill_Windows.exe    |
| Win32 窗口管理             |
| D3D11 HDR 渲染器           |
| 托盘图标                   |
+----------------------------+
```

`DisplayFill_Settings.exe` 是主要用户入口。如果 HDR 引擎尚未运行，设置程序会从同目录启动 `DisplayFill_Windows.exe`，随后连接管道并读取当前状态。

`DisplayFill_Windows.exe` 是后台渲染引擎。它创建 HDR 补光窗口，管理鼠标穿透，渲染相框，并提供管道服务接收控制命令。

## 目录结构

```text
DisplayFill_Windows.slnx              解决方案文件
quick-build.cmd                       Release 构建和打包脚本

DisplayFill_Windows/                  HDR 渲染引擎项目
  HDRDriver.h                         引擎设置、常量和声明
  main.cpp                            引擎入口
  WindowManager.cpp                   Win32 窗口、托盘、输入、IPC 命令应用
  Renderer.cpp                        D3D11 渲染器和 HDR/scRGB 输出
  IpcServer.cpp                       Named Pipe 服务端
  DisplayFill_Windows.vcxproj         引擎项目文件

DisplayFill_Settings/                 WinUI 3 设置程序项目
  App.xaml                            WinUI 应用定义
  MainWindow.xaml                     设置 UI 布局
  MainWindow.xaml.cpp                 设置 UI 逻辑和 IPC 调用
  PipeClient.h / PipeClient.cpp       Named Pipe 客户端和引擎启动逻辑
  DisplayFill_Settings.vcxproj        WinUI 3 项目文件

run/                                  生成的运行目录，忽略提交
x64/                                  x64 构建输出，忽略提交
ARM64/                                ARM64 构建输出，忽略提交
```

## 运行时控制

设置窗口可以实时调整：

- HDR 目标亮度，单位 nits。
- 补光相框比例。
- 圆角半径。
- 视觉边缘羽化。
- 鼠标穿透模式。
- 鼠标悬停透明度。
- 鼠标悬停过渡时间。
- 语言。
- 刷新状态和退出引擎。

引擎还支持快捷键：

```text
Ctrl+F6    切换鼠标穿透模式
ESC        退出引擎
```

托盘图标可用于打开设置程序、切换穿透模式或退出引擎。

## IPC 协议

HDR 引擎监听以下 Named Pipe：

```text
\\.\pipe\DisplayFill_Windows_Control
```

消息示例：

```json
{"type":"getState"}
```

```json
{"type":"set","key":"targetNits","value":800}
```

```json
{"type":"set","key":"frameMarginXRatio","value":0.08}
```

```json
{"type":"set","key":"frameMarginYRatio","value":0.08}
```

```json
{"type":"set","key":"cornerRadius","value":80}
```

```json
{"type":"set","key":"visualCornerFeatherPixels","value":24}
```

```json
{"type":"set","key":"hoverAlpha","value":40}
```

```json
{"type":"set","key":"hoverTransitionSeconds","value":0.45}
```

```json
{"type":"set","key":"passThroughMode","value":true}
```

```json
{"type":"set","key":"language","value":"zh-Hans"}
```

```json
{"type":"command","name":"togglePassThrough"}
```

```json
{"type":"command","name":"exit"}
```

支持的语言值：

```text
zh-Hans
zh-Hant
en-US
```

## 运行要求

### 运行环境

- Windows 10/11。
- 支持 Direct3D 11 的 GPU。
- 如果需要真正 HDR 亮度输出，需要 HDR 显示器，并在 Windows 显示设置中开启 HDR。
- 仅 FrameworkDependent 小包需要安装 Microsoft Windows App Runtime 1.8。

SelfContained 大包包含 Windows App SDK / WinUI 3 运行时文件，不需要单独安装 Windows App Runtime。

FrameworkDependent 小包体积更小，但目标机器必须已经安装 Windows App Runtime 1.8。

Windows App Runtime 下载地址：

```text
https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads
```

### 构建环境

- Visual Studio 2026 或兼容的新版本 C++ 工具链。
- MSBuild 路径：

```text
C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe
```

- Windows SDK，包含 Direct3D 11、DXGI、WinUI 3 / Windows App SDK 依赖。

## 构建和打包

在项目根目录运行：

```cmd
quick-build.cmd x64
```

或：

```cmd
quick-build.cmd arm64
```

必须指定平台参数。如果不传 `x64` 或 `arm64`，脚本只会显示用法，不会执行构建。

脚本会执行：

1. 停止正在运行的 `DisplayFill_Settings` 和 `DisplayFill_Windows` 进程。
2. 清理所选平台的旧运行目录和旧 zip。
3. 构建 Release SelfContained。
4. 创建解压即用的大运行目录。
5. 创建 SelfContained release zip。
6. 构建 Release FrameworkDependent。
7. 创建依赖 Windows App Runtime 1.8 的小运行目录。
8. 创建 FrameworkDependent release zip。
9. 验证输出目录和 zip 文件非空。

## 发布输出

ARM64：

```text
run\Release-ARM64-SelfContained
run\Release-ARM64-FrameworkDependent
DisplayFill_Windows_ARM64_SelfContained_Release.zip
DisplayFill_Windows_ARM64_FrameworkDependent_Release.zip
```

x64：

```text
run\Release-x64-SelfContained
run\Release-x64-FrameworkDependent
DisplayFill_Windows_x64_SelfContained_Release.zip
DisplayFill_Windows_x64_FrameworkDependent_Release.zip
```

## 发布包类型

### SelfContained 大包

适合需要最简单运行体验的用户。

```text
DisplayFill_Windows_ARM64_SelfContained_Release.zip
DisplayFill_Windows_x64_SelfContained_Release.zip
```

特点：

- 体积较大。
- 包含 Windows App SDK / WinUI 3 运行时文件。
- 不需要用户单独安装 Windows App Runtime。
- 推荐用于快速测试和绿色版分发。

运行：

```text
Start-DisplayFill.cmd
```

或：

```text
DisplayFill_Settings.exe
```

### FrameworkDependent 小包

适合在意体积，并且可以安装依赖的用户。

```text
DisplayFill_Windows_ARM64_FrameworkDependent_Release.zip
DisplayFill_Windows_x64_FrameworkDependent_Release.zip
```

特点：

- 体积较小。
- 不包含完整 Windows App SDK / WinUI 3 运行时。
- 需要安装匹配 CPU 架构的 Microsoft Windows App Runtime 1.8。
- 适合已经安装运行时的用户。

运行：

```text
Start-DisplayFill.cmd
```

或：

```text
DisplayFill_Settings.exe
```

如果 `DisplayFill_Settings.exe` 无法启动，请安装 Windows App Runtime 1.8。

## OLED 和 HDR 安全警告

本应用会有意在屏幕边缘创建明亮区域。对于 OLED 或其他自发光面板，静态高亮 UI 可能加速不均匀像素老化。

潜在风险包括：

- 临时残影。
- 永久烧屏。
- 像素老化加速。
- 相框区域亮度或色彩不均。

建议：

- 使用满足需求的最低亮度。
- 不要长时间无人值守运行补光相框。
- 避免在 OLED 显示器上长时间使用最高 HDR 亮度。
- OLED 面板建议缩短单次使用时间。
- 允许显示器执行正常的像素刷新 / 面板维护流程。
- 自行承担使用风险。

## 从构建输出运行

执行 `quick-build.cmd` 后，使用 `run/` 目录下生成的文件夹。

例如：

```text
run\Release-ARM64-SelfContained\Start-DisplayFill.cmd
```

设置程序会自动启动 HDR 引擎。两个 exe 应保持在同一目录：

```text
DisplayFill_Settings.exe
DisplayFill_Windows.exe
```

## 引擎默认参数

多数默认参数定义在：

```text
DisplayFill_Windows\HDRDriver.h
```

重要设置包括：

- `targetNits`：HDR 目标亮度，单位 nits。
- `frameMarginXRatio`：左右相框宽度比例。
- `frameMarginYRatio`：上下相框高度比例。
- `holeCornerRadius`：中间挖空区域物理圆角半径。
- `visualCornerFeatherPixels`：shader 视觉边缘羽化。
- `normalWindowAlpha`：正常状态窗口透明度。
- `mouseHoverWindowAlpha`：鼠标靠近时的窗口透明度。
- `hoverOpacityTransitionSeconds`：悬停透明度过渡时间。
- `startupBreathMinBrightness`：启动呼吸动画最低亮度比例。
- `startupBreathMaxBrightness`：启动呼吸动画最高亮度比例。
- `startupBreathDurationSeconds`：启动呼吸动画时长。

运行时可通过 WinUI 3 设置窗口调整大多数参数，无需重新编译。

## HDR 注意事项

要获得真正 HDR 亮度：

- 使用支持 HDR 的显示器。
- 在 Windows 显示设置中开启 HDR。
- 确保应用运行在 HDR 显示器上。

如果 HDR 不可用，引擎会回退到 SDR 输出。应用仍可运行，但补光亮度不会超过 SDR 范围。

高亮度可能造成眼部不适。请根据显示器能力和环境亮度使用合理亮度。

## 开发说明

- `DisplayFill_Settings.exe` 是 unpackaged WinUI 3 应用。
- SelfContained 构建体积较大，因为包含 `Microsoft.ui.xaml.dll`、`Microsoft.UI.Xaml.Controls.dll`、`Microsoft.WindowsAppRuntime.dll`、`DWriteCore.dll` 等 Windows App SDK 运行时文件。
- FrameworkDependent 构建体积较小，因为依赖已安装的 Windows App Runtime。


## 致谢

本项目的设计思路参考了 [dashhuang/DisplayFill](https://github.com/dashhuang/DisplayFill)。此 Windows 版本在该概念基础上扩展了 Win32/D3D11 HDR 渲染引擎和 WinUI 3 设置前端。
