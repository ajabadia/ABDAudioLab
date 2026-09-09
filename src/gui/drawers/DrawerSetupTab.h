/**
 * @file DrawerSetupTab.h
 * @brief Drawer tab for active hardware target summary, audio/MIDI telemetry, and about card.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include "../SoundIdTheme.h"
#include "../InfoDrawer.h"

namespace abdaudiolab::gui
{

class DrawerSetupTab : public juce::Component
{
public:
    DrawerSetupTab();
    ~DrawerSetupTab() override = default;

    void setTelemetryInfo(const TelemetryInfo& info);
    void setTargetHardwareInfo(const juce::String& hwName,
                               const juce::String& submoduleName,
                               const juce::String& routingText,
                               const juce::Image& rasterImg,
                               const juce::Drawable* svgDrawable = nullptr);

    void updateTheme();
    [[nodiscard]] int getPreferredHeight() const noexcept;

    // Callbacks
    std::function<void()> onOpenAudioSettingsClicked;
    std::function<void()> onAboutClicked;

    void paint(juce::Graphics& g) override;
    void resized() override;

    class ImageDisplayComponent : public juce::Component
    {
    public:
        explicit ImageDisplayComponent(DrawerSetupTab& ownerRef) : owner(ownerRef) {}
        void paint(juce::Graphics& g) override;
    private:
        DrawerSetupTab& owner;
    };

private:
    void updateAboutVisibility();

    TelemetryInfo telemetryInfo;

    // Target Hardware & Routing Summary
    juce::Label lblSetupTargetSection;
    ImageDisplayComponent setupImgDisplay;
    juce::Label lblSetupTargetHwName;
    juce::Label lblSetupTargetSubmodule;
    juce::Label lblSetupTargetRouting;

    // Audio Interface & MIDI Telemetry
    juce::Label lblSetupAudioSection;
    juce::Label lblSetupAudioDevice;
    juce::Label lblSetupAudioDeviceVal;
    juce::Label lblSetupSampleRate;
    juce::Label lblSetupSampleRateVal;
    juce::Label lblSetupLatency;
    juce::Label lblSetupLatencyVal;
    juce::Label lblSetupMidiInput;
    juce::Label lblSetupMidiInputVal;
    juce::Label lblSetupMidiOutput;
    juce::Label lblSetupMidiOutputVal;

    // Action Buttons
    juce::TextButton btnSetupAudioMidi { "Configure Audio & MIDI Settings..." };
    juce::TextButton btnSetupAbout { "About ABDAudioLab & Research Architecture" };

    // Expandable About Card
    bool aboutSectionExpanded { false };
    juce::Label lblAboutVersion;
    juce::Label lblAboutTagline;
    juce::Label lblAboutArchitecture;
    juce::Label lblAboutCredits;

    // Model graphics
    juce::Image modelRasterImage;
    std::unique_ptr<juce::Drawable> modelSvgDrawable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawerSetupTab)
};

} // namespace abdaudiolab::gui
