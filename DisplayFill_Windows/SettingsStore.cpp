#include "HDRDriver.h"

#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>

namespace hdr_driver
{
namespace
{
    constexpr wchar_t kIniFileName[] = L"DisplayFill_Windows.ini";
    constexpr wchar_t kSection[] = L"DisplayFill";

    std::wstring GetIniPath()
    {
        wchar_t modulePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

        wchar_t* lastSlash = wcsrchr(modulePath, L'\\');
        if (!lastSlash)
        {
            return kIniFileName;
        }

        *(lastSlash + 1) = L'\0';
        return std::wstring(modulePath) + kIniFileName;
    }

    float ReadFloat(const std::wstring& path, const wchar_t* key, float fallback, float minValue, float maxValue)
    {
        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"%.6f", fallback);
        GetPrivateProfileStringW(kSection, key, buffer, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
        wchar_t* end = nullptr;
        const float value = wcstof(buffer, &end);
        if (end == buffer)
        {
            return fallback;
        }
        return AppState::Clamp(value, minValue, maxValue);
    }

    int ReadInt(const std::wstring& path, const wchar_t* key, int fallback, int minValue, int maxValue)
    {
        const int value = GetPrivateProfileIntW(kSection, key, fallback, path.c_str());
        return AppState::ClampInt(value, minValue, maxValue);
    }

    bool ReadBool(const std::wstring& path, const wchar_t* key, bool fallback)
    {
        return GetPrivateProfileIntW(kSection, key, fallback ? 1 : 0, path.c_str()) != 0;
    }

    void WriteFloat(const std::wstring& path, const wchar_t* key, float value)
    {
        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"%.6f", value);
        WritePrivateProfileStringW(kSection, key, buffer, path.c_str());
    }

    void WriteInt(const std::wstring& path, const wchar_t* key, int value)
    {
        wchar_t buffer[32] = {};
        swprintf_s(buffer, L"%d", value);
        WritePrivateProfileStringW(kSection, key, buffer, path.c_str());
    }

    void WriteBool(const std::wstring& path, const wchar_t* key, bool value)
    {
        WritePrivateProfileStringW(kSection, key, value ? L"1" : L"0", path.c_str());
    }

    const wchar_t* LanguageToIni(AppLanguage language)
    {
        switch (language)
        {
        case AppLanguage::ZhHant:
            return L"zh-Hant";
        case AppLanguage::EnUs:
            return L"en-US";
        case AppLanguage::ZhHans:
        default:
            return L"zh-Hans";
        }
    }

    AppLanguage ReadLanguage(const std::wstring& path, AppLanguage fallback)
    {
        wchar_t buffer[32] = {};
        wcscpy_s(buffer, LanguageToIni(fallback));
        GetPrivateProfileStringW(kSection, L"language", buffer, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
        if (wcscmp(buffer, L"zh-Hant") == 0)
        {
            return AppLanguage::ZhHant;
        }
        if (wcscmp(buffer, L"en-US") == 0)
        {
            return AppLanguage::EnUs;
        }
        return AppLanguage::ZhHans;
    }

    const wchar_t* ThemeModeToIni(AppThemeMode mode)
    {
        switch (mode)
        {
        case AppThemeMode::Light:
            return L"light";
        case AppThemeMode::Dark:
            return L"dark";
        case AppThemeMode::System:
        default:
            return L"system";
        }
    }

    AppThemeMode ReadThemeMode(const std::wstring& path, AppThemeMode fallback)
    {
        wchar_t buffer[32] = {};
        wcscpy_s(buffer, ThemeModeToIni(fallback));
        GetPrivateProfileStringW(kSection, L"themeMode", buffer, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
        if (wcscmp(buffer, L"light") == 0)
        {
            return AppThemeMode::Light;
        }
        if (wcscmp(buffer, L"dark") == 0)
        {
            return AppThemeMode::Dark;
        }
        return AppThemeMode::System;
    }

    const wchar_t* BackdropKindToIni(AppBackdropKind kind)
    {
        switch (kind)
        {
        case AppBackdropKind::Mica:
            return L"mica";
        case AppBackdropKind::Solid:
            return L"solid";
        case AppBackdropKind::Acrylic:
        default:
            return L"acrylic";
        }
    }

    AppBackdropKind ReadBackdropKind(const std::wstring& path, AppBackdropKind fallback)
    {
        wchar_t buffer[32] = {};
        wcscpy_s(buffer, BackdropKindToIni(fallback));
        GetPrivateProfileStringW(kSection, L"backdropKind", buffer, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
        if (wcscmp(buffer, L"mica") == 0)
        {
            return AppBackdropKind::Mica;
        }
        if (wcscmp(buffer, L"solid") == 0)
        {
            return AppBackdropKind::Solid;
        }
        return AppBackdropKind::Acrylic;
    }
}

void LoadSettingsFromIni(AppSettings& settings)
{
    const std::wstring path = GetIniPath();
    if (!std::filesystem::exists(path))
    {
        SaveSettingsToIni(settings);
    }

    settings.targetNits = ReadFloat(path, L"targetNits", settings.targetNits, 80.0f, 4000.0f);
    settings.frameMarginXRatio = ReadFloat(path, L"frameMarginXRatio", settings.frameMarginXRatio, 0.01f, 0.45f);
    settings.frameMarginYRatio = ReadFloat(path, L"frameMarginYRatio", settings.frameMarginYRatio, 0.01f, 0.45f);
    settings.holeCornerRadius = ReadInt(path, L"cornerRadius", settings.holeCornerRadius, 0, 960);
    settings.visualCornerFeatherPixels = ReadFloat(path, L"visualCornerFeatherPixels", settings.visualCornerFeatherPixels, 0.0f, 256.0f);
    settings.outerCornerRadius = ReadInt(path, L"outerCornerRadius", settings.outerCornerRadius, 0, 960);
    settings.screenInsetPixels = ReadInt(path, L"screenInsetPixels", settings.screenInsetPixels, 0, 2000);
    settings.centerBrightnessBoost = ReadFloat(path, L"centerBrightnessBoost", settings.centerBrightnessBoost, 0.0f, 2.0f);
    settings.colorTemperatureShift = ReadFloat(path, L"colorTemperatureShift", settings.colorTemperatureShift, -1.0f, 1.0f);
    settings.colorTintShift = ReadFloat(path, L"colorTintShift", settings.colorTintShift, -1.0f, 1.0f);
    settings.shadowStrength = ReadFloat(path, L"shadowStrength", settings.shadowStrength, 0.0f, 1.0f);
    settings.shadowSizePixels = ReadFloat(path, L"shadowSizePixels", settings.shadowSizePixels, 0.0f, 160.0f);
    settings.normalWindowAlpha = static_cast<BYTE>(ReadInt(path, L"normalAlpha", settings.normalWindowAlpha, 0, 255));
    settings.mouseHoverWindowAlpha = static_cast<BYTE>(ReadInt(path, L"hoverAlpha", settings.mouseHoverWindowAlpha, 0, 255));
    settings.hoverOpacityTransitionSeconds = ReadFloat(path, L"hoverTransitionSeconds", settings.hoverOpacityTransitionSeconds, 0.01f, 5.0f);
    settings.startupBreathMinBrightness = ReadFloat(path, L"startupBreathMinBrightness", settings.startupBreathMinBrightness, 0.0f, 4.0f);
    settings.startupBreathMaxBrightness = ReadFloat(path, L"startupBreathMaxBrightness", settings.startupBreathMaxBrightness, 0.0f, 4.0f);
    settings.startupBreathDurationSeconds = ReadFloat(path, L"startupBreathDurationSeconds", settings.startupBreathDurationSeconds, 0.0f, 30.0f);
    settings.passThroughMode = ReadBool(path, L"passThroughMode", settings.passThroughMode);
    settings.language = ReadLanguage(path, settings.language);
    settings.themeMode = ReadThemeMode(path, settings.themeMode);
    settings.backdropKind = ReadBackdropKind(path, settings.backdropKind);
    SaveSettingsToIni(settings);
}

void SaveSettingsToIni(const AppSettings& settings)
{
    const std::wstring path = GetIniPath();
    WriteFloat(path, L"targetNits", settings.targetNits);
    WriteFloat(path, L"frameMarginXRatio", settings.frameMarginXRatio);
    WriteFloat(path, L"frameMarginYRatio", settings.frameMarginYRatio);
    WriteInt(path, L"cornerRadius", settings.holeCornerRadius);
    WriteFloat(path, L"visualCornerFeatherPixels", settings.visualCornerFeatherPixels);
    WriteInt(path, L"outerCornerRadius", settings.outerCornerRadius);
    WriteInt(path, L"screenInsetPixels", settings.screenInsetPixels);
    WriteFloat(path, L"centerBrightnessBoost", settings.centerBrightnessBoost);
    WriteFloat(path, L"colorTemperatureShift", settings.colorTemperatureShift);
    WriteFloat(path, L"colorTintShift", settings.colorTintShift);
    WriteFloat(path, L"shadowStrength", settings.shadowStrength);
    WriteFloat(path, L"shadowSizePixels", settings.shadowSizePixels);
    WriteInt(path, L"normalAlpha", settings.normalWindowAlpha);
    WriteInt(path, L"hoverAlpha", settings.mouseHoverWindowAlpha);
    WriteFloat(path, L"hoverTransitionSeconds", settings.hoverOpacityTransitionSeconds);
    WriteFloat(path, L"startupBreathMinBrightness", settings.startupBreathMinBrightness);
    WriteFloat(path, L"startupBreathMaxBrightness", settings.startupBreathMaxBrightness);
    WriteFloat(path, L"startupBreathDurationSeconds", settings.startupBreathDurationSeconds);
    WriteBool(path, L"passThroughMode", settings.passThroughMode);
    WritePrivateProfileStringW(kSection, L"language", LanguageToIni(settings.language), path.c_str());
    WritePrivateProfileStringW(kSection, L"themeMode", ThemeModeToIni(settings.themeMode), path.c_str());
    WritePrivateProfileStringW(kSection, L"backdropKind", BackdropKindToIni(settings.backdropKind), path.c_str());
}

std::string ReadSettingsIniUtf8()
{
    const std::wstring path = GetIniPath();
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool WriteSettingsIniUtf8(const std::string& content, AppSettings& settings)
{
    const std::wstring path = GetIniPath();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return false;
    }

    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!stream)
    {
        return false;
    }
    stream.close();
    LoadSettingsFromIni(settings);
    return true;
}

} // namespace hdr_driver
