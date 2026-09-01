#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

struct TelemetryInfo
{
    juce::String audioDeviceName;
    juce::String audioDeviceType;
    double sampleRate { 96000.0 };
    int bufferSize { 256 };
    double latencyMs { 2.67 };
    int inputChannels { 2 };
    int outputChannels { 2 };
    juce::String midiInputName;
    juce::String midiOutputName;
    juce::String exportDirectoryPath;
    float autoTrimGainDb { 0.0f };
    float lastMeasuredSnrDb { 0.0f };
    int totalMeasuredPoints { 0 };
    juce::String appVersion;
    int buildNumber { 107 };
};

/**
 * @brief Slide-in drawer displaying live system, audio/MIDI, and telemetry information.
 */
class InfoDrawer : public juce::Component,
                   public juce::Timer
{
public:
    InfoDrawer();
    ~InfoDrawer() override = default;

    void openDrawer();
    void closeDrawer();
    [[nodiscard]] bool isDrawerOpen() const noexcept { return isOpen; }

    void updateTelemetry(const TelemetryInfo& info);

    std::function<void()> onChangeExportFolderClicked;
    std::function<void()> onOpenAudioSettingsClicked;
    std::function<void()> onAboutClicked;

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void repositionPanel();
    [[nodiscard]] float getResponsivePanelWidth() const noexcept;

    bool isOpen { false };
    float currentAnimationPos { 0.0f };
    float targetAnimationPos { 0.0f };

    TelemetryInfo telemetry;

    class PanelComponent : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        TelemetryInfo telemetry;
        void drawTelemetryRow(juce::Graphics& g, const juce::String& label, const juce::String& value, juce::Rectangle<float>& area);
    };

    PanelComponent panel;

    juce::TextButton btnClose { juce::String::fromUTF8(u8"✕") };
    juce::TextButton btnChangeFolder { "Change Export Destination..." };
    juce::TextButton btnOpenAudioSetup { "Open Audio & MIDI Setup..." };
    juce::TextButton btnAbout { "About ABDAudioLab..." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InfoDrawer)
};

} // namespace abdaudiolab::gui
