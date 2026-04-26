# DisplayFill Windows

A Windows desktop HDR screen fill-light application built with Win32, Direct3D 11, and DXGI. It creates a topmost borderless frame around the current work area and renders high-brightness white in scRGB HDR space.

## Features

- Direct3D 11 rendering with `DXGI_FORMAT_R16G16B16A16_FLOAT` FP16 swap chain.
- scRGB HDR output via `DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709`.
- Automatic SDR fallback when HDR/scRGB presentation is unavailable.
- Work-area aware window sizing so the taskbar is not covered.
- Center cut-out frame using Win32 region APIs for desktop/app click-through.
- Smooth visual rounded-corner brightness falloff implemented with a D3D shader.
- Startup brightness breathing animation from 20% to 100% over 3 seconds.
- Nonlinear mouse-hover opacity transition.
- Pass-through mode toggle with `Ctrl+F6`.

## Controls

- `Ctrl+F6`: Toggle pass-through mode.
- `ESC`: Exit.
- Double-click the window frame: Exit.

Default mode is pass-through mode, so mouse clicks can reach windows behind the fill-light frame.

## Requirements

- Windows 10/11 with DXGI 1.6 capable SDK/runtime.
- Visual Studio 2026 or a recent Visual Studio C++ toolchain.
- Windows SDK with Direct3D 11 and D3DCompiler headers/libraries.
- HDR-capable display with Windows HDR enabled for full HDR brightness.

## Build

Open `DisplayFill_Windows.slnx` in Visual Studio and build the project, or run:

```powershell
msbuild "DisplayFill_Windows\DisplayFill_Windows.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

## Tunable Parameters

Most user-facing parameters are in `DisplayFill_Windows/HDRDriver.h`:

- `kDefaultTargetNits`: target HDR brightness in nits.
- `kFrameMarginXRatio`: left/right frame thickness ratio.
- `kFrameMarginYRatio`: top/bottom frame thickness ratio.
- `kHoleCornerRadius`: physical/visual hole corner radius.
- `kVisualCornerFeatherPixels`: shader-based visual smoothing width.
- `kMouseHoverWindowAlpha`: target alpha during hover transparency.
- `kHoverOpacityTransitionSeconds`: nonlinear opacity transition duration.
- `kStartupBreathMinBrightness`: startup breathing minimum brightness.
- `kStartupBreathMaxBrightness`: startup breathing maximum brightness.
- `kStartupBreathDurationSeconds`: startup breathing duration.

## Files That Should Be Ignored

The repository should not commit generated build artifacts or machine-local files. The `.gitignore` excludes:

- Visual Studio local settings: `.vs/`, `.vscode/`, `*.user`, `*.suo`.
- Build output directories: `x64/`, `ARM64/`, `Debug/`, `Release/`, project-local build folders.
- Intermediate compiler/linker files: `*.obj`, `*.pdb`, `*.ilk`, `*.idb`, `*.tlog`, `*.log`.
- Generated binaries: `*.exe`, `*.dll`, `*.lib`, `*.exp`.
- Local environment/secrets: `.env`, `.env.*`, credential/key/certificate files.

Do not commit access tokens, private certificates, local credentials, or generated binaries unless there is a specific release process for them.

## Notes

This app intentionally drives high brightness on HDR displays. Use appropriate brightness values and avoid prolonged use at uncomfortable luminance levels.
