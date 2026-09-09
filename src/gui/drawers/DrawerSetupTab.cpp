/**
 * @file DrawerSetupTab.cpp
 * @brief Implementation of DrawerSetupTab.
 * @author ABDSynths
 * @date 2026
 */

#include "DrawerSetupTab.h"

namespace abdaudiolab::gui
{

DrawerSetupTab::DrawerSetupTab()
    : setupImgDisplay(*this)
{
    auto setupLbl = [this](juce::Label& lbl, const juce::String& text, bool bold) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::FontOptions(10.5f, bold ? juce::Font::bold : juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, bold ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    // 1. Target Hardware & Routing Summary
    lblSetupTargetSection.setText("1. ACTIVE SESSION TARGET & HARDWARE", juce::dontSendNotification);
    lblSetupTargetSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSetupTargetSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblSetupTargetSection);

    addAndMakeVisible(setupImgDisplay);

    lblSetupTargetHwName.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    lblSetupTargetHwName.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupTargetHwName.setText("No Hardware Selected", juce::dontSendNotification);
    addAndMakeVisible(lblSetupTargetHwName);

    lblSetupTargetSubmodule.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSetupTargetSubmodule.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    lblSetupTargetSubmodule.setText("Active Submodule: Default Profile", juce::dontSendNotification);
    addAndMakeVisible(lblSetupTargetSubmodule);

    lblSetupTargetRouting.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    lblSetupTargetRouting.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblSetupTargetRouting.setText("Routing: Self-Contained / Direct Loopback", juce::dontSendNotification);
    addAndMakeVisible(lblSetupTargetRouting);

    // 2. Audio Interface & MIDI Telemetry
    lblSetupAudioSection.setText("2. AUDIO INTERFACE & MIDI TELEMETRY", juce::dontSendNotification);
    lblSetupAudioSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSetupAudioSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblSetupAudioSection);

    setupLbl(lblSetupAudioDevice, "Active Audio Interface:", true);
    setupLbl(lblSetupAudioDeviceVal, "Windows Audio (Default)", false);
    setupLbl(lblSetupSampleRate, "Sample Rate:", true);
    setupLbl(lblSetupSampleRateVal, "96,000 Hz", false);
    setupLbl(lblSetupLatency, "Buffer Size / Latency:", true);
    setupLbl(lblSetupLatencyVal, "256 samples (2.67 ms)", false);
    setupLbl(lblSetupMidiInput, "MIDI Input Device:", true);
    setupLbl(lblSetupMidiInputVal, "None", false);
    setupLbl(lblSetupMidiOutput, "MIDI Output Device:", true);
    setupLbl(lblSetupMidiOutputVal, "None", false);

    btnSetupAudioMidi.setTooltip("Configure Audio & MIDI Settings - Select audio interface, sample rate, buffer size, and MIDI ports");
    btnSetupAudioMidi.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnSetupAudioMidi.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnSetupAudioMidi.onClick = [this] {
        if (onOpenAudioSettingsClicked)
            onOpenAudioSettingsClicked();
    };
    addAndMakeVisible(btnSetupAudioMidi);

    btnSetupAbout.setTooltip("About ABDAudioLab - View software version, research architecture, and credits");
    btnSetupAbout.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnSetupAbout.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnSetupAbout.onClick = [this] {
        aboutSectionExpanded = !aboutSectionExpanded;
        updateAboutVisibility();
        if (onAboutClicked)
            onAboutClicked();
        if (auto* parent = getParentComponent())
            parent->resized();
    };
    addAndMakeVisible(btnSetupAbout);

    // Inline About & Architecture Labels
    lblAboutVersion.setText("ABDAudioLab v1.0.0 (DSP Validation Platform)", juce::dontSendNotification);
    lblAboutVersion.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblAboutVersion.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addChildComponent(lblAboutVersion);

    lblAboutTagline.setText("Nonlinear Analog Profiling & Emulation Research", juce::dontSendNotification);
    lblAboutTagline.setFont(juce::FontOptions(10.0f, juce::Font::italic));
    lblAboutTagline.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addChildComponent(lblAboutTagline);

    lblAboutArchitecture.setText("Hybrid C++20 / JUCE 8 / ABDSharedCode Architecture\nReal-time DSP, TPT ZDF Filters, PolyBLEP Oscillators, SysEx MIDI Engine.", juce::dontSendNotification);
    lblAboutArchitecture.setFont(juce::FontOptions(9.5f));
    lblAboutArchitecture.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addChildComponent(lblAboutArchitecture);

    lblAboutCredits.setText(juce::String::fromUTF8(u8"© 2026 ABDSynths - Alberto Abadía"), juce::dontSendNotification);
    lblAboutCredits.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    lblAboutCredits.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addChildComponent(lblAboutCredits);
}

void DrawerSetupTab::updateAboutVisibility()
{
    lblAboutVersion.setVisible(aboutSectionExpanded);
    lblAboutTagline.setVisible(aboutSectionExpanded);
    lblAboutArchitecture.setVisible(aboutSectionExpanded);
    lblAboutCredits.setVisible(aboutSectionExpanded);
    resized();
}

void DrawerSetupTab::setTelemetryInfo(const TelemetryInfo& info)
{
    telemetryInfo = info;
    lblSetupAudioDeviceVal.setText(info.audioDeviceName.isNotEmpty() ? info.audioDeviceName : "None", juce::dontSendNotification);
    lblSetupSampleRateVal.setText(juce::String(info.sampleRate, 0) + " Hz", juce::dontSendNotification);
    lblSetupLatencyVal.setText(juce::String(info.bufferSize) + " samples (" + juce::String(info.latencyMs, 2) + " ms)", juce::dontSendNotification);
    lblSetupMidiInputVal.setText(info.midiInputName.isNotEmpty() ? info.midiInputName : "None (Manual / Mock)", juce::dontSendNotification);
    lblSetupMidiOutputVal.setText(info.midiOutputName.isNotEmpty() ? info.midiOutputName : "None (Manual / Mock)", juce::dontSendNotification);
}

void DrawerSetupTab::setTargetHardwareInfo(const juce::String& hwName,
                                          const juce::String& submoduleName,
                                          const juce::String& routingText,
                                          const juce::Image& rasterImg,
                                          const juce::Drawable* svgDrawable)
{
    lblSetupTargetHwName.setText(hwName.isNotEmpty() ? hwName : "No Hardware Selected", juce::dontSendNotification);
    lblSetupTargetSubmodule.setText(submoduleName.isNotEmpty() ? ("Active Submodule: " + submoduleName) : "Active Submodule: Default Profile", juce::dontSendNotification);
    lblSetupTargetRouting.setText(routingText.isNotEmpty() ? routingText : "Routing: Self-Contained / Direct Loopback", juce::dontSendNotification);

    modelRasterImage = rasterImg;
    if (svgDrawable != nullptr)
        modelSvgDrawable = svgDrawable->createCopy();
    else
        modelSvgDrawable.reset();

    setupImgDisplay.repaint();
}

void DrawerSetupTab::updateTheme()
{
    auto updateBtn = [](juce::TextButton& btn) {
        btn.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
        btn.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    };

    updateBtn(btnSetupAudioMidi);
    updateBtn(btnSetupAbout);

    lblSetupTargetSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupTargetHwName.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupTargetSubmodule.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    lblSetupTargetRouting.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);

    lblSetupAudioSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupAudioDevice.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupAudioDeviceVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblSetupSampleRate.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupSampleRateVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblSetupLatency.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupLatencyVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblSetupMidiInput.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupMidiInputVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblSetupMidiOutput.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblSetupMidiOutputVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);

    lblAboutVersion.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblAboutTagline.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblAboutArchitecture.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblAboutCredits.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);

    setupImgDisplay.repaint();
    repaint();
}

int DrawerSetupTab::getPreferredHeight() const noexcept
{
    int h = 530;
    if (aboutSectionExpanded)
        h += 110;
    return h;
}

void DrawerSetupTab::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void DrawerSetupTab::resized()
{
    int padX = 0;
    int contentW = getWidth();
    int y = 0;

    lblSetupTargetSection.setBounds(padX, y, contentW, 16);
    y += 22;

    setupImgDisplay.setBounds(padX, y, contentW, 130);
    y += 136;

    lblSetupTargetHwName.setBounds(padX, y, contentW, 20);
    y += 22;

    lblSetupTargetSubmodule.setBounds(padX, y, contentW, 18);
    y += 20;

    lblSetupTargetRouting.setBounds(padX, y, contentW, 16);
    y += 28;

    lblSetupAudioSection.setBounds(padX, y, contentW, 16);
    y += 24;

    auto addRow = [&](juce::Label& lbl, juce::Label& val) {
        lbl.setBounds(padX, y, contentW, 15);
        val.setBounds(padX + 4, y + 16, contentW - 4, 18);
        y += 38;
    };

    addRow(lblSetupAudioDevice, lblSetupAudioDeviceVal);
    addRow(lblSetupSampleRate, lblSetupSampleRateVal);
    addRow(lblSetupLatency, lblSetupLatencyVal);
    addRow(lblSetupMidiInput, lblSetupMidiInputVal);
    addRow(lblSetupMidiOutput, lblSetupMidiOutputVal);

    y += 6;
    btnSetupAudioMidi.setBounds(padX, y, contentW, 28);
    y += 34;
    btnSetupAbout.setBounds(padX, y, contentW, 28);
    y += 34;

    if (aboutSectionExpanded)
    {
        lblAboutVersion.setVisible(true);
        lblAboutVersion.setBounds(padX + 6, y, contentW - 12, 18);
        y += 20;

        lblAboutTagline.setVisible(true);
        lblAboutTagline.setBounds(padX + 6, y, contentW - 12, 16);
        y += 18;

        lblAboutArchitecture.setVisible(true);
        lblAboutArchitecture.setBounds(padX + 6, y, contentW - 12, 32);
        y += 34;

        lblAboutCredits.setVisible(true);
        lblAboutCredits.setBounds(padX + 6, y, contentW - 12, 16);
        y += 24;
    }
    else
    {
        lblAboutVersion.setVisible(false);
        lblAboutTagline.setVisible(false);
        lblAboutArchitecture.setVisible(false);
        lblAboutCredits.setVisible(false);
    }
}

void DrawerSetupTab::ImageDisplayComponent::paint(juce::Graphics& g)
{
    auto renderArea = getLocalBounds().toFloat();
    if (owner.modelSvgDrawable != nullptr)
    {
        owner.modelSvgDrawable->drawWithin(g, renderArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
    else if (owner.modelRasterImage.isValid())
    {
        g.drawImage(owner.modelRasterImage, renderArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
}

} // namespace abdaudiolab::gui
