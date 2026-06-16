# DisplayFill Windows

DisplayFill Windows 是一个 Windows HDR 屏幕补光灯应用。它会在桌面边缘渲染高亮 HDR 相框，把屏幕变成柔和的补光源，同时保留中间区域供正常使用。

> OLED / HDR 安全提示：本应用会显示高亮、相对静止的画面区域。OLED 或其他自发光屏幕可能出现残影、像素老化或烧屏风险。请使用较低亮度，避免长时间无人值守运行，并自行承担使用风险。

## 当前状态

项目已经重构为单个 WinUI 3 主程序：

```text
DisplayFill_Settings.exe
```

这个 EXE 同时包含：

- WinUI 3 设置窗口。
- Win32 / Direct3D 11 HDR 渲染引擎。
- 托盘图标和菜单。
- INI 配置读写。
- 可选 Named Pipe 控制接口。

也就是说，用户只需要启动 `DisplayFill_Settings.exe`。发布包里可能仍会有 WinUI / Windows App SDK 运行时 DLL，但应用入口只有这一个 EXE。

## 功能

- HDR 边缘补光相框，中央区域保持开放。
- Direct3D 11 + DXGI + FP16 swap chain 渲染。
- scRGB HDR 输出，HDR 不可用时自动回退到 SDR。
- 相框宽度以屏幕垂直分辨率作为统一基准，横向和纵向厚度比例一致。
- 支持圆角、边缘羽化、屏幕边距、中心亮度增强、色温、色调和柔和阴影。
- 支持鼠标穿透模式，方便操作补光窗口下方的应用。
- 支持鼠标靠近时透明度过渡和启动亮度呼吸动画。
- 单 WinUI 设置窗口，支持隐藏设置窗口并通过托盘恢复。
- 支持 Acrylic / Mica / Solid 背景材质。
- 支持跟随系统、浅色、深色主题。
- 支持简体中文、繁体中文和英文界面。
- 支持复制 / 粘贴 INI 配置文本。

## 截图和图标

主程序使用 Fluent 风格的简洁太阳光图标。应用图标资源位于：

```text
DisplayFill_Settings/Assets/
DisplayFill_Settings/Assets/SunLight.ico
```

## 运行要求

- Windows 10 / Windows 11。
- 支持 Direct3D 11 的 GPU。
- 如需真正 HDR 亮度输出，需要 HDR 显示器，并在 Windows 显示设置中开启 HDR。
- FrameworkDependent 小包需要安装 Microsoft Windows App Runtime 1.8。

SelfContained 大包会携带 Windows App SDK / WinUI 3 运行时文件，不需要额外安装 Windows App Runtime。

FrameworkDependent 小包体积更小，但仍需要：

- 目标机器已安装 Windows App Runtime 1.8。
- `Microsoft.WindowsAppRuntime.Bootstrap.dll` 与 `DisplayFill_Settings.exe` 位于同一目录。

`quick-build.cmd` 会自动把这个 bootstrap DLL 放入 FrameworkDependent 运行目录和 ZIP 包。

## 构建

需要 Visual Studio C++ / WinUI 工具链和 Windows App SDK 依赖。当前本机使用的 MSBuild 路径是：

```text
C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe
```

在项目根目录运行：

```cmd
quick-build.cmd x64
```

或：

```cmd
quick-build.cmd arm64
```

脚本会依次构建并打包：

- SelfContained Release。
- FrameworkDependent Release。

生成目录和 ZIP 文件：

```text
run\Release-x64-SelfContained
run\Release-x64-FrameworkDependent
DisplayFill_Windows_x64_SelfContained_Release.zip
DisplayFill_Windows_x64_FrameworkDependent_Release.zip

run\Release-ARM64-SelfContained
run\Release-ARM64-FrameworkDependent
DisplayFill_Windows_ARM64_SelfContained_Release.zip
DisplayFill_Windows_ARM64_FrameworkDependent_Release.zip
```

`run/`、`x64/`、`ARM64/`、ZIP 和二进制输出都已被 `.gitignore` 忽略，不会提交到 GitHub。

## 运行

解压 Release ZIP 后运行：

```text
DisplayFill_Settings.exe
```

也可以运行包内的：

```text
Start-DisplayFill.cmd
```

配置文件会自动生成在 EXE 同目录：

```text
DisplayFill_Windows.ini
```

常用快捷键：

```text
Ctrl+F6    切换鼠标穿透模式
ESC        退出 HDR 引擎
```

## 项目结构

```text
DisplayFill_Windows.slnx
quick-build.cmd

DisplayFill_Settings/
  App.xaml
  MainWindow.xaml
  MainWindow.xaml.cpp
  EngineHost.h
  EngineHost.cpp
  DisplayFill_Settings.vcxproj
  Assets/

DisplayFill_Windows/
  HDRDriver.h
  WindowManager.cpp
  Renderer.cpp
  SettingsStore.cpp
  IpcServer.cpp
```

`DisplayFill_Settings` 是唯一的 WinUI 3 EXE 项目。`DisplayFill_Windows` 目录保留 HDR 引擎源码，但这些源码会被编译进 `DisplayFill_Settings.exe`。

## 配置和控制

设置窗口可以实时调整：

- 目标 HDR 亮度。
- 相框宽度。
- 圆角和羽化。
- 屏幕边距。
- 中心亮度增强。
- 色温和色调。
- 阴影强度和范围。
- 鼠标穿透。
- 悬停透明度和过渡时间。
- Acrylic / Mica / Solid 背景。
- 浅色 / 深色 / 跟随系统主题。
- 界面语言。

内部引擎还保留 Named Pipe 控制接口：

```text
\\.\pipe\DisplayFill_Windows_Control
```

常用消息示例：

```json
{"type":"getState"}
```

```json
{"type":"set","key":"targetNits","value":800}
```

```json
{"type":"command","name":"togglePassThrough"}
```

```json
{"type":"getConfig"}
```

```json
{"type":"setConfig","content":"[DisplayFill]\r\ntargetNits=800\r\n"}
```

## HDR 说明

要获得真正 HDR 亮度，请确认：

- 显示器支持 HDR。
- Windows 显示设置中已经开启 HDR。
- 应用运行在 HDR 显示器上。

如果不满足这些条件，程序会进入 SDR 回退模式。此时应用仍可使用，但不会输出超过 SDR 范围的亮度。

## English Summary

DisplayFill Windows is an HDR screen fill-light app for Windows. It renders a bright HDR frame around the desktop while leaving the center area open.

The current architecture uses one app entry point:

```text
DisplayFill_Settings.exe
```

The WinUI 3 settings window, Win32/D3D11 HDR renderer, tray icon, INI persistence, and optional Named Pipe control server are compiled into the same process.

Build release packages with:

```cmd
quick-build.cmd x64
```

or:

```cmd
quick-build.cmd arm64
```

SelfContained packages include the Windows App SDK / WinUI runtime files. FrameworkDependent packages are smaller but require Microsoft Windows App Runtime 1.8 on the target machine.

## 致谢

项目思路参考了 [dashhuang/DisplayFill](https://github.com/dashhuang/DisplayFill)。本项目在此基础上扩展了 Windows HDR、Win32/D3D11 渲染和 WinUI 3 设置界面。
