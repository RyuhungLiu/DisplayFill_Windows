#include "pch.h"
#include "SettingsWindow.xaml.h"
#if __has_include("SettingsWindow.g.cpp")
#include "SettingsWindow.g.cpp"
#endif

#include <format>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::DisplayFillWindows::implementation
{
    SettingsWindow::SettingsWindow()
    {
        InitializeComponent();
        ConnectEvents();
        UpdateValueTexts();
    }

    void SettingsWindow::LoadSettings(SettingsSnapshot const& settings)
    {
        TargetNitsSlider().Value(settings.targetNits);
        FrameMarginSlider().Value(settings.frameMarginRatio);
        CornerRadiusSlider().Value(settings.cornerRadius);
        CornerFeatherSlider().Value(settings.cornerFeatherPixels);
        PassThroughToggle().IsOn(settings.passThroughMode);
        HoverAlphaSlider().Value(settings.hoverAlpha);
        HoverTransitionSlider().Value(settings.hoverTransitionSeconds);
        BreathMinSlider().Value(settings.startupBreathMinBrightness);
        BreathDurationSlider().Value(settings.startupBreathDurationSeconds);
        UpdateValueTexts();
    }

    void SettingsWindow::SetSettingsChangedCallback(SettingsChangedCallback callback)
    {
        m_callback = callback;
    }

    void SettingsWindow::ConnectEvents()
    {
        auto updateOnly = [this](auto const&, auto const&)
        {
            UpdateValueTexts();
        };

        TargetNitsSlider().ValueChanged(updateOnly);
        FrameMarginSlider().ValueChanged(updateOnly);
        CornerRadiusSlider().ValueChanged(updateOnly);
        CornerFeatherSlider().ValueChanged(updateOnly);
        HoverAlphaSlider().ValueChanged(updateOnly);
        HoverTransitionSlider().ValueChanged(updateOnly);
        BreathMinSlider().ValueChanged(updateOnly);
        BreathDurationSlider().ValueChanged(updateOnly);

        ApplyButton().Click([this](auto const&, auto const&)
        {
            ApplyCurrentSettings();
        });

        ResetButton().Click([this](auto const&, auto const&)
        {
            ResetDefaults();
        });
    }

    void SettingsWindow::UpdateValueTexts()
    {
        TargetNitsValueText().Text(to_hstring(static_cast<int>(TargetNitsSlider().Value())) + L" nits");
        FrameMarginValueText().Text(to_hstring(static_cast<int>(FrameMarginSlider().Value() * 100.0)) + L"%");
        CornerRadiusValueText().Text(to_hstring(static_cast<int>(CornerRadiusSlider().Value())) + L" px");
        CornerFeatherValueText().Text(to_hstring(static_cast<int>(CornerFeatherSlider().Value())) + L" px");
        HoverAlphaValueText().Text(to_hstring(static_cast<int>(HoverAlphaSlider().Value())));
        HoverTransitionValueText().Text(to_hstring(HoverTransitionSlider().Value()) + L" s");
        BreathMinValueText().Text(to_hstring(static_cast<int>(BreathMinSlider().Value() * 100.0)) + L"%");
        BreathDurationValueText().Text(to_hstring(BreathDurationSlider().Value()) + L" s");
    }

    SettingsSnapshot SettingsWindow::BuildSnapshot() const
    {
        SettingsSnapshot settings;
        settings.targetNits = static_cast<float>(TargetNitsSlider().Value());
        settings.frameMarginRatio = static_cast<float>(FrameMarginSlider().Value());
        settings.cornerRadius = static_cast<int>(CornerRadiusSlider().Value());
        settings.cornerFeatherPixels = static_cast<float>(CornerFeatherSlider().Value());
        settings.passThroughMode = PassThroughToggle().IsOn();
        settings.hoverAlpha = static_cast<uint8_t>(HoverAlphaSlider().Value());
        settings.hoverTransitionSeconds = static_cast<float>(HoverTransitionSlider().Value());
        settings.startupBreathMinBrightness = static_cast<float>(BreathMinSlider().Value());
        settings.startupBreathDurationSeconds = static_cast<float>(BreathDurationSlider().Value());
        return settings;
    }

    void SettingsWindow::ApplyCurrentSettings()
    {
        if (m_callback)
        {
            m_callback(BuildSnapshot());
        }
    }

    void SettingsWindow::ResetDefaults()
    {
        SettingsSnapshot defaults;
        LoadSettings(defaults);
    }
}
