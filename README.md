# DisplayFill Windows

Windows HDR screen fill-light desktop application built with Win32, Direct3D 11, and DXGI.

这是一个 Windows 桌面 HDR 屏幕补光灯应用。程序会创建一个置顶无边框窗口，在屏幕边缘渲染高亮度 HDR 白色补光区域，并在中间保留可点击穿透的挖空区域，方便同时使用桌面、终端、Visual Studio 或其他应用。

## 功能特性

- 使用 Direct3D 11 渲染，交换链格式为 `DXGI_FORMAT_R16G16B16A16_FLOAT` FP16。
- 使用 scRGB HDR 色彩空间：`DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709`。
- 支持 HDR 检测；当系统或显示器未启用 HDR 时自动回退到 SDR 白色。
- 使用工作区尺寸创建窗口，默认不会遮挡任务栏。
- 按当前主显示器分辨率计算相框边距，例如上下/左右各取 8%。
- 使用 Win32 Region 实现中间区域物理挖空，穿透模式下可点击后方窗口。
- 使用 Direct3D shader 做视觉圆角亮度羽化，让圆角边缘比 `CreateRoundRectRgn` 更平滑。
- 启动时支持亮度呼吸动画，默认 3 秒内从 20% 平滑提升到 100%。
- 鼠标悬停在补光相框区域时，窗口透明度会按非线性曲线降低。
- 支持穿透模式和非穿透模式切换。

## Controls / 操作方式

- `Ctrl+F6`: Toggle pass-through mode / 切换穿透模式。
- `ESC`: Exit / 退出。
- Double-click the window frame: Exit / 双击补光相框退出。

默认是穿透模式，鼠标可以点击到补光窗口后面的应用。按 `Ctrl+F6` 后可切换为非穿透模式，此时相框区域会像普通置顶窗口一样接收鼠标事件。

## Requirements / 运行要求

- Windows 10/11。
- 支持 DXGI 1.6 的 Windows SDK/runtime。
- Visual Studio 2026 或较新的 Visual Studio C++ 工具链。
- Windows SDK 中的 Direct3D 11 与 D3DCompiler 头文件/库。
- 如果需要真正 HDR 高亮度输出，需要 HDR 显示器，并在 Windows 设置中开启 HDR。

## Build / 构建方式

使用 Visual Studio 打开 `DisplayFill_Windows.slnx` 后直接构建，或在项目根目录运行：

```powershell
msbuild "DisplayFill_Windows\DisplayFill_Windows.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

## Tunable Parameters / 可调参数

主要参数集中在 `DisplayFill_Windows/HDRDriver.h`：

- `kDefaultTargetNits`: HDR 目标亮度，单位 nits。常见设置为 600、800、1000、1200。
- `kFrameMarginXRatio`: 左右补光相框宽度比例，例如 `0.08f` 表示左右各取屏幕宽度 8%。
- `kFrameMarginYRatio`: 上下补光相框高度比例，例如 `0.08f` 表示上下各取屏幕高度 8%。
- `kHoleCornerRadius`: 中间挖空区域圆角半径，单位像素。
- `kVisualCornerFeatherPixels`: 视觉圆角羽化宽度，数值越大边缘越柔和。
- `kNormalWindowAlpha`: 正常状态窗口透明度，`255` 为完全不透明。
- `kMouseHoverWindowAlpha`: 鼠标悬停时目标透明度，数值越小越透明。
- `kHoverOpacityTransitionSeconds`: 鼠标悬停透明度非线性过渡时长。
- `kStartupBreathMinBrightness`: 启动呼吸动画的最低亮度比例。
- `kStartupBreathMaxBrightness`: 启动呼吸动画的最高亮度比例。
- `kStartupBreathDurationSeconds`: 启动呼吸动画持续时间。

## Files That Should Be Ignored / 需要屏蔽的内容

仓库不应该提交构建产物、本机 IDE 配置、临时文件或任何密钥。当前 `.gitignore` 已屏蔽：

- Visual Studio 本机状态：`.vs/`、`.vscode/`、`*.user`、`*.suo`。
- 构建输出目录：`x64/`、`ARM64/`、`Debug/`、`Release/`、项目局部 build 文件夹。
- 编译/链接中间文件：`*.obj`、`*.pdb`、`*.ilk`、`*.idb`、`*.tlog`、`*.log`。
- 生成的二进制文件：`*.exe`、`*.dll`、`*.lib`、`*.exp`。
- 本地环境和敏感文件：`.env`、`.env.*`、`*.pem`、`*.key`、`*.pfx`、`*.p12`、`credentials.json`、`secrets.json`。

不要提交 GitHub token、私有证书、本机凭据、个人环境变量或编译生成的可执行文件。发布二进制文件时建议使用 GitHub Releases，而不是直接提交到源码仓库。

## Notes / 注意事项

此应用会尝试激发 HDR 显示器的高亮度输出。长时间使用高亮度补光可能造成眼部不适，请根据显示器能力和环境亮度合理调整 `kDefaultTargetNits`。

## Acknowledgements / 致谢

本项目的设计思路参考了 [dashhuang/DisplayFill](https://github.com/dashhuang/DisplayFill)。感谢原项目提供的启发。
