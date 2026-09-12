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
                               const juce::Drawable* svgDrawable = nullptr,
                               const juce::String& category = {},
                               const juce::String& statusText = {},
                               std::optional<juce::Colour> statusColour = std::nullopt);
    void setDetectedInterfaceInfo(const juce::String& interfaceName,
                                  const juce::String& detailsText,
                                  const juce::Image& interfaceImg,
                                  bool isConnectedToSoftware = true,
                                  bool isAudioIn = false,
                                  bool isAudioOut = false,
                                  bool isMidiIn = false,
                                  bool isMidiOut = false);

    void updateTheme();
    [[nodiscard]] int getPreferredHeight() const noexcept;

    // Callbacks
    std::function<void()> onOpenAudioSettingsClicked;
    std::function<void()> onOpenTopologyModalClicked;
    std::function<void()> onAboutClicked;
    std::function<void()> onRefreshRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

    class TargetDeviceHeroComponent : public juce::Component
    {
    public:
        explicit TargetDeviceHeroComponent(DrawerSetupTab& ownerRef);
        void paint(juce::Graphics& g) override;

    private:
        DrawerSetupTab& owner;
    };

    class RealConnectionsSummaryComponent : public juce::Component
    {
    public:
        explicit RealConnectionsSummaryComponent(DrawerSetupTab& ownerRef);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void updateTheme();

        class RefreshIconButton : public juce::Button
        {
        public:
            RefreshIconButton() : juce::Button("RefreshConnections") {}
            void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        };

        RefreshIconButton btnRefresh;

    private:
        DrawerSetupTab& owner;
    };

private:
    void updateAboutVisibility();

    TelemetryInfo telemetryInfo;

    // Centered Hero & Real Connections
    std::unique_ptr<TargetDeviceHeroComponent> heroTargetDevice;
    std::unique_ptr<RealConnectionsSummaryComponent> connectionsSummary;

    // Header & Section Labels
    juce::Label lblStudioTitle;
    juce::Label lblStudioSubtitle;

    // Telemetry and Status Summary
    juce::Label lblAudioDeviceVal;
    juce::Label lblSampleRateVal;
    juce::Label lblLatencyVal;
    juce::Label lblMidiInputVal;
    juce::Label lblMidiOutputVal;

    // Action Buttons
    juce::TextButton btnOpenTopology;
    juce::TextButton btnSetupAudioMidi;
    juce::TextButton btnSetupAbout;

    // Expandable About Card
    bool aboutSectionExpanded { false };
    juce::Label lblAboutVersion;
    juce::Label lblAboutTagline;
    juce::Label lblAboutArchitecture;
    juce::Label lblAboutCredits;

    // Target Hardware Data
    juce::String targetHwName { "Direct Loopback (No Target Selected)" };
    juce::String targetSubmoduleName { "DAC/ADC Calibration" };
    juce::String targetRoutingText { "Direct Loopback Cable" };
    juce::Image targetRasterImage;
    std::unique_ptr<juce::Drawable> targetSvgDrawable;
    juce::String targetStatusBadgeText { "DIRECT LOOPBACK" };
    juce::Colour targetStatusBadgeColour { SoundIdTheme::accentAmber };

    // Detected Host Interface Data
    juce::String interfaceName;
    juce::String interfaceDetails;
    juce::Image interfaceRasterImage;
    juce::Image interfaceGrayscaleImage;
    bool hasDetectedInterface { false };
    bool interfaceConnectedToSoftware { false };
    bool hasAudioInConnected { false };
    bool hasAudioOutConnected { false };
    bool hasMidiInConnected { false };
    bool hasMidiOutConnected { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawerSetupTab)
};

} // namespace abdaudiolab::gui
