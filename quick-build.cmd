@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
set "CONFIG=Release"

if /I "%~1"=="x64" goto :platform_x64
if /I "%~1"=="arm64" goto :platform_arm64
if /I "%~1"=="ARM64" goto :platform_arm64

echo Usage:
echo   %~nx0 x64
echo   %~nx0 arm64
echo.
echo This script builds two release packages for the selected platform:
echo   1. SelfContained       - large, unzip and run immediately
echo   2. FrameworkDependent  - small, requires Windows App Runtime 1.8
echo.
echo ERROR: You must specify target platform: x64 or arm64.
exit /b 2

:platform_x64
set "PLATFORM=x64"
set "PLATFORM_LABEL=x64"
set "OUT_ROOT=x64"
goto :main

:platform_arm64
set "PLATFORM=ARM64"
set "PLATFORM_LABEL=ARM64"
set "OUT_ROOT=ARM64"
goto :main

:main
set "SETTINGS_OUT=%ROOT%%OUT_ROOT%\Release\DisplayFill_Settings"

set "RUN_SELF=%ROOT%run\Release-%PLATFORM_LABEL%-SelfContained"
set "RUN_FD=%ROOT%run\Release-%PLATFORM_LABEL%-FrameworkDependent"

set "ZIP_SELF=%ROOT%DisplayFill_Windows_%PLATFORM_LABEL%_SelfContained_Release.zip"
set "ZIP_FD=%ROOT%DisplayFill_Windows_%PLATFORM_LABEL%_FrameworkDependent_Release.zip"

set "LEGACY_ZIP=%ROOT%DisplayFill_Windows_0.1.3_ARM64_Release.zip"
set "OLD_ZIP_ARM64=%ROOT%DisplayFill_Windows_ARM64_Release.zip"
set "OLD_ZIP_X64=%ROOT%DisplayFill_Windows_x64_Release.zip"

cd /d "%ROOT%"

echo [1/9] Stopping running DisplayFill processes...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Process DisplayFill_Settings,DisplayFill_Windows -ErrorAction SilentlyContinue | Stop-Process -Force"

if not exist "%MSBUILD%" (
    echo ERROR: MSBuild not found: %MSBUILD%
    exit /b 1
)

echo [2/9] Cleaning old package outputs...
if exist "%RUN_SELF%" rmdir /s /q "%RUN_SELF%"
if exist "%RUN_FD%" rmdir /s /q "%RUN_FD%"

if exist "%ZIP_SELF%" del /f /q "%ZIP_SELF%"
if exist "%ZIP_FD%" del /f /q "%ZIP_FD%"

if exist "%OLD_ZIP_ARM64%" del /f /q "%OLD_ZIP_ARM64%"
if exist "%OLD_ZIP_X64%" del /f /q "%OLD_ZIP_X64%"
if exist "%LEGACY_ZIP%" del /f /q "%LEGACY_ZIP%"

mkdir "%RUN_SELF%"
if errorlevel 1 exit /b %errorlevel%

mkdir "%RUN_FD%"
if errorlevel 1 exit /b %errorlevel%

echo [3/9] Building %CONFIG% %PLATFORM_LABEL% SelfContained...
"%MSBUILD%" "%ROOT%DisplayFill_Windows.slnx" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:WindowsAppSDKSelfContained=true
if errorlevel 1 exit /b %errorlevel%

if not exist "%SETTINGS_OUT%" (
    echo ERROR: Settings output folder not found: %SETTINGS_OUT%
    exit /b 1
)

echo [4/9] Creating SelfContained runtime folder...
robocopy "%SETTINGS_OUT%" "%RUN_SELF%" /E /XF *.pdb *.ilk *.exp *.lib *.iobj *.ipdb *.recipe *.appxrecipe *.log *.tlog *.lastbuildstate
if %errorlevel% GTR 7 exit /b %errorlevel%

call :write_launcher "%RUN_SELF%"
call :write_readme_self "%RUN_SELF%"

call :clean_build_artifacts "%RUN_SELF%"
if errorlevel 1 exit /b %errorlevel%

echo [5/9] Packaging SelfContained zip...
call :zip_folder "%RUN_SELF%" "%ZIP_SELF%"
if errorlevel 1 exit /b %errorlevel%

echo [6/9] Building %CONFIG% %PLATFORM_LABEL% FrameworkDependent...
"%MSBUILD%" "%ROOT%DisplayFill_Windows.slnx" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:WindowsAppSDKSelfContained=false
if errorlevel 1 exit /b %errorlevel%

if not exist "%SETTINGS_OUT%" (
    echo ERROR: Settings output folder not found after FrameworkDependent build: %SETTINGS_OUT%
    exit /b 1
)

echo [7/9] Creating FrameworkDependent runtime folder...
call :copy_framework_dependent_files
if errorlevel 1 exit /b %errorlevel%

call :write_launcher "%RUN_FD%"
call :write_readme_fd "%RUN_FD%"
call :clean_build_artifacts "%RUN_FD%"
if errorlevel 1 exit /b %errorlevel%

echo [8/9] Packaging FrameworkDependent zip...
call :zip_folder "%RUN_FD%" "%ZIP_FD%"
if errorlevel 1 exit /b %errorlevel%

echo [9/9] Verifying outputs...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$selfZip = Get-Item -LiteralPath '%ZIP_SELF%';" ^
  "$fdZip = Get-Item -LiteralPath '%ZIP_FD%';" ^
  "$selfFiles = @(Get-ChildItem -LiteralPath '%RUN_SELF%' -Recurse -File -Force).Count;" ^
  "$fdFiles = @(Get-ChildItem -LiteralPath '%RUN_FD%' -Recurse -File -Force).Count;" ^
  "if ($selfFiles -le 0) { throw 'SelfContained runtime folder is empty.' };" ^
  "if ($fdFiles -le 0) { throw 'FrameworkDependent runtime folder is empty.' };" ^
  "if ($selfZip.Length -le 1024) { throw 'SelfContained zip is unexpectedly small.' };" ^
  "if ($fdZip.Length -le 1024) { throw 'FrameworkDependent zip is unexpectedly small.' };" ^
  "[pscustomobject]@{ SelfContainedFiles=$selfFiles; FrameworkDependentFiles=$fdFiles; SelfContainedZipBytes=$selfZip.Length; FrameworkDependentZipBytes=$fdZip.Length }"
if errorlevel 1 exit /b %errorlevel%

echo.
echo Done.
echo.
echo SelfContained runtime:
echo   %RUN_SELF%
echo SelfContained zip:
echo   %ZIP_SELF%
echo.
echo FrameworkDependent runtime:
echo   %RUN_FD%
echo FrameworkDependent zip:
echo   %ZIP_FD%
echo.
echo Note:
echo   SelfContained is large but unzip-and-run.
echo   FrameworkDependent is small but requires Windows App Runtime 1.8.
echo.

endlocal
exit /b 0


:copy_framework_dependent_files
if not exist "%RUN_FD%" mkdir "%RUN_FD%"

robocopy "%SETTINGS_OUT%" "%RUN_FD%" /E /XF *.pdb *.ilk *.exp *.lib *.iobj *.ipdb *.recipe *.appxrecipe *.log *.tlog *.lastbuildstate
if %errorlevel% GTR 7 exit /b %errorlevel%

if not exist "%RUN_FD%\Microsoft.WindowsAppRuntime.Bootstrap.dll" (
    echo ERROR: FrameworkDependent package is missing Microsoft.WindowsAppRuntime.Bootstrap.dll.
    exit /b 1
)

exit /b 0


:write_launcher
set "TARGET_DIR=%~1"

> "%TARGET_DIR%\Start-DisplayFill.cmd" (
    echo @echo off
    echo cd /d "%%~dp0"
    echo start "" "DisplayFill_Settings.exe"
)

exit /b 0


:write_readme_self
set "TARGET_DIR=%~1"

> "%TARGET_DIR%\README.txt" (
    echo DisplayFill %CONFIG% %PLATFORM_LABEL% SelfContained Release
    echo.
    echo This is the large unzip-and-run package.
    echo.
    echo How to run:
    echo   Start-DisplayFill.cmd
    echo.
    echo Or run directly:
    echo   DisplayFill_Settings.exe
    echo.
    echo This folder includes the Windows App SDK / WinUI 3 runtime files.
    echo No separate Windows App Runtime installation is required.
    echo.
    echo DisplayFill_Settings.exe contains the WinUI settings window and the HDR rendering engine in one process.
)

exit /b 0


:write_readme_fd
set "TARGET_DIR=%~1"

> "%TARGET_DIR%\README.txt" (
    echo DisplayFill %CONFIG% %PLATFORM_LABEL% FrameworkDependent Release
    echo.
    echo This is the small package.
    echo.
    echo Requirement:
    echo   You must install Microsoft Windows App Runtime 1.8 before running this version.
    echo.
    echo Download:
    echo   https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads
    echo.
    echo How to run:
    echo   Start-DisplayFill.cmd
    echo.
    echo Or run directly:
    echo   DisplayFill_Settings.exe
    echo.
    echo If DisplayFill_Settings.exe does not start, install Windows App Runtime 1.8 for your CPU architecture.
    echo.
    echo DisplayFill_Settings.exe contains the WinUI settings window and the HDR rendering engine in one process.
)

exit /b 0


:clean_build_artifacts
set "TARGET_DIR=%~1"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$removeExtensions = @('.pdb','.ilk','.exp','.lib','.iobj','.ipdb','.recipe','.appxrecipe','.log','.tlog','.lastbuildstate');" ^
  "Get-ChildItem -LiteralPath '%TARGET_DIR%' -Recurse -File -Force | Where-Object { $removeExtensions -contains $_.Extension.ToLowerInvariant() } | Remove-Item -Force -ErrorAction SilentlyContinue;" ^
  "$fileCount = @(Get-ChildItem -LiteralPath '%TARGET_DIR%' -Recurse -File -Force).Count;" ^
  "if ($fileCount -le 0) { throw 'Runtime folder has no files after cleanup: %TARGET_DIR%' }"

if errorlevel 1 exit /b %errorlevel%
exit /b 0


:zip_folder
set "SOURCE_DIR=%~1"
set "DEST_ZIP=%~2"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$sourceDir = [System.IO.Path]::GetFullPath('%SOURCE_DIR%');" ^
  "$zipPath = [System.IO.Path]::GetFullPath('%DEST_ZIP%');" ^
  "if (-not (Test-Path -LiteralPath $sourceDir -PathType Container)) { throw 'Runtime folder not found: ' + $sourceDir };" ^
  "$fileCount = @(Get-ChildItem -LiteralPath $sourceDir -Recurse -File -Force).Count;" ^
  "if ($fileCount -le 0) { throw 'Runtime folder has no files: ' + $sourceDir };" ^
  "if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force };" ^
  "Add-Type -AssemblyName System.IO.Compression.FileSystem;" ^
  "[System.IO.Compression.ZipFile]::CreateFromDirectory($sourceDir, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false);" ^
  "$zip = Get-Item -LiteralPath $zipPath;" ^
  "if ($zip.Length -le 1024) { throw 'Created zip is unexpectedly small: ' + $zip.FullName + ' (' + $zip.Length + ' bytes)' }"

if errorlevel 1 exit /b %errorlevel%
exit /b 0
