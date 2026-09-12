/**
 * @file DrawerSetupTab.cpp
 * @brief Implementation of DrawerSetupTab.
 * @author ABDSynths
 * @date 2026
 */

#include "DrawerSetupTab.h"
#include "AssetLocator.h"

namespace abdaudiolab::gui
{

// =============================================================================
// TargetDeviceHeroComponent
// =============================================================================

DrawerSetupTab::TargetDeviceHeroComponent::TargetDeviceHeroComponent(DrawerSetupTab& ownerRef)
    : owner(ownerRef)
{
}

void DrawerSetupTab::TargetDeviceHeroComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float corner = 8.0f;

    // Card background & subtle border
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(b, corner);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b.reduced(0.5f), corner, 1.0f);

    // Header badge
    auto headerArea = b.removeFromTop(28.0f).reduced(12.0f, 4.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentAmber);
    g.drawText("TARGET DEVICE UNDER TEST", headerArea, juce::Justification::centredLeft, true);

    // Category / Status Tag on the right of header (Dynamic: SELECTED PROFILE vs CONNECTED HARDWARE)
    auto tagArea = headerArea.removeFromRight(150.0f);
    g.setColour(owner.targetStatusBadgeColour.withAlpha(0.12f));
    g.fillRoundedRectangle(tagArea, 3.0f);
    g.setColour(owner.targetStatusBadgeColour);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(owner.targetStatusBadgeText, tagArea, juce::Justification::centred, true);

    // Hero image area in the center (shorter height for balanced proportion)
    auto imgArea = b.removeFromTop(95.0f).reduced(12.0f, 2.0f);
    if (owner.targetSvgDrawable != nullptr)
    {
        owner.targetSvgDrawable->drawWithin(g, imgArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
    else if (owner.targetRasterImage.isValid())
    {
        g.drawImage(owner.targetRasterImage, imgArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

    // Name & Submodule
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(owner.targetHwName, b.removeFromTop(20.0f).reduced(12.0f, 0.0f), juce::Justification::centred, true);

    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText(owner.targetSubmoduleName, b.removeFromTop(18.0f).reduced(12.0f, 0.0f), juce::Justification::centred, true);

    g.setFont(juce::FontOptions(9.5f));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText(owner.targetRoutingText, b.removeFromTop(16.0f).reduced(12.0f, 0.0f), juce::Justification::centred, true);
}

// =============================================================================
// RealConnectionsSummaryComponent
// =============================================================================

void DrawerSetupTab::RealConnectionsSummaryComponent::RefreshIconButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    
    // Background pill/circle on hover/down
    if (shouldDrawButtonAsDown)
    {
        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.25f));
        g.fillRoundedRectangle(b, 4.0f);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        g.setColour(SoundIdTheme::surfaceSubtle);
        g.fillRoundedRectangle(b, 4.0f);
    }

    // Determine icon color
    juce::Colour iconCol = shouldDrawButtonAsDown ? SoundIdTheme::accentGreen
                         : shouldDrawButtonAsHighlighted ? SoundIdTheme::accentGreen
                         : SoundIdTheme::textSecondary;

    // Draw circular refresh arrow vector
    auto iconArea = b.reduced(3.0f);
    float cx = iconArea.getCentreX();
    float cy = iconArea.getCentreY();
    float r = std::min(iconArea.getWidth(), iconArea.getHeight()) * 0.42f;

    g.setColour(iconCol);
    
    // Arc from ~45 deg to ~315 deg
    juce::Path arc;
    arc.addCentredArc(cx, cy, r, r, 0.0f, juce::MathConstants<float>::pi * 0.35f, juce::MathConstants<float>::pi * 1.85f, true);
    g.strokePath(arc, juce::PathStrokeType(1.75f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Arrow head at arc end
    float endAngle = juce::MathConstants<float>::pi * 1.85f;
    float arrowX = cx + r * std::sin(endAngle);
    float arrowY = cy - r * std::cos(endAngle);

    juce::Path head;
    head.startNewSubPath(arrowX - 2.5f, arrowY - 4.0f);
    head.lineTo(arrowX + 2.0f, arrowY);
    head.lineTo(arrowX - 4.0f, arrowY + 2.5f);
    g.strokePath(head, juce::PathStrokeType(1.75f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}

DrawerSetupTab::RealConnectionsSummaryComponent::RealConnectionsSummaryComponent(DrawerSetupTab& ownerRef)
    : owner(ownerRef)
{
    btnRefresh.setTooltip(juce::String::fromUTF8(u8"Refrescar estado de interfaces y conexiones"));
    btnRefresh.onClick = [this] {
        if (owner.onRefreshRequested)
            owner.onRefreshRequested();
    };

    addAndMakeVisible(btnRefresh);
}

void DrawerSetupTab::RealConnectionsSummaryComponent::updateTheme()
{
    btnRefresh.repaint();
}

void DrawerSetupTab::RealConnectionsSummaryComponent::resized()
{
    // Position refresh icon button at the top-right header area with a comfortable click size (26x24)
    btnRefresh.setBounds(getWidth() - 36, 2, 26, 24);
}

void DrawerSetupTab::RealConnectionsSummaryComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float corner = 8.0f;

    // Card background
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(b, corner);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b.reduced(0.5f), corner, 1.0f);

    auto headerArea = b.removeFromTop(26.0f).reduced(12.0f, 4.0f);
    // Reserve space on the right for the refresh button
    headerArea.removeFromRight(30.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("ACTIVE REAL CONNECTIONS", headerArea, juce::Justification::centredLeft, true);

    // Divider
    g.setColour(SoundIdTheme::borderSubtle.withAlpha(0.6f));
    g.drawHorizontalLine(static_cast<int>(b.getY()), b.getX() + 10.0f, b.getRight() - 10.0f);
    b.removeFromTop(8.0f);

    auto drawConnectionRow = [&](const juce::String& portType,
                                 const juce::String& description,
                                 juce::Colour statusCol,
                                 bool isConnected)
    {
        auto row = b.removeFromTop(24.0f).reduced(12.0f, 1.0f);

        // Status pill/dot
        auto dotArea = row.removeFromLeft(12.0f);
        g.setColour(isConnected ? statusCol : SoundIdTheme::textMuted);
        g.fillEllipse(dotArea.getX() + 2.0f, dotArea.getY() + 6.0f, 8.0f, 8.0f);

        // Port Type Badge
        auto badgeArea = row.removeFromLeft(85.0f);
        g.setColour(isConnected ? statusCol.withAlpha(0.15f) : SoundIdTheme::surfaceSubtle);
        g.fillRoundedRectangle(badgeArea, 3.0f);
        g.setColour(isConnected ? statusCol : SoundIdTheme::textMuted);
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(portType, badgeArea, juce::Justification::centred, true);

        row.removeFromLeft(8.0f);

        // Description text
        g.setFont(juce::FontOptions(9.5f));
        g.setColour(isConnected ? SoundIdTheme::textPrimary : SoundIdTheme::textMuted);
        g.drawText(description, row, juce::Justification::centredLeft, true);
    };

    // 1. Audio Output (Probe / Excitation)
    juce::String audioOutStr;
    if (owner.hasAudioOutConnected)
        audioOutStr = owner.telemetryInfo.audioDeviceName + " -> Main Output (DAC)";
    else
        audioOutStr = "No output assigned";
    drawConnectionRow("AUDIO OUT", audioOutStr, SoundIdTheme::accentGreen, owner.hasAudioOutConnected);

    // 2. Audio Input (Return / Measurement)
    juce::String audioInStr;
    if (owner.hasAudioInConnected)
        audioInStr = owner.telemetryInfo.audioDeviceName + " <- Recording Input (ADC)";
    else
        audioInStr = "No input assigned";
    drawConnectionRow("AUDIO IN", audioInStr, SoundIdTheme::accentAmber, owner.hasAudioInConnected);

    // 3. MIDI Out (Control / SysEx / Clock)
    juce::String midiOutStr;
    if (owner.hasMidiOutConnected)
        midiOutStr = owner.telemetryInfo.midiOutputName + " (Control / SysEx)";
    else
        midiOutStr = "No MIDI Out port assigned";
    drawConnectionRow("MIDI OUT", midiOutStr, SoundIdTheme::accentBlue, owner.hasMidiOutConnected);

    // 4. MIDI In (Telemetry / Feedback)
    juce::String midiInStr;
    if (owner.hasMidiInConnected)
        midiInStr = owner.telemetryInfo.midiInputName + " (Telemetry / Feedback)";
    else
        midiInStr = "No MIDI In port assigned";
    drawConnectionRow("MIDI IN", midiInStr, juce::Colours::cyan, owner.hasMidiInConnected);
}

// =============================================================================
// DrawerSetupTab Implementation
// =============================================================================

DrawerSetupTab::DrawerSetupTab()
{
    lblStudioTitle.setText("STUDIO ENVIRONMENT, INTERFACES & ROUTING", juce::dontSendNotification);
    lblStudioTitle.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    lblStudioTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblStudioTitle);

    lblStudioSubtitle.setText("Active device under test and real-time audio/MIDI connections.", juce::dontSendNotification);
    lblStudioSubtitle.setFont(juce::FontOptions(10.0f));
    lblStudioSubtitle.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblStudioSubtitle);

    // Centered Hero & Real Connections
    heroTargetDevice = std::make_unique<TargetDeviceHeroComponent>(*this);
    addAndMakeVisible(heroTargetDevice.get());

    connectionsSummary = std::make_unique<RealConnectionsSummaryComponent>(*this);
    addAndMakeVisible(connectionsSummary.get());

    // Telemetry display labels
    auto setupValLbl = [this](juce::Label& lbl) {
        lbl.setFont(juce::FontOptions(10.0f));
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };
    setupValLbl(lblAudioDeviceVal);
    setupValLbl(lblSampleRateVal);
    setupValLbl(lblLatencyVal);
    setupValLbl(lblMidiInputVal);
    setupValLbl(lblMidiOutputVal);

    btnOpenTopology.setButtonText("Open Studio Connection Map...");
    btnOpenTopology.setTooltip("View and interact with the real-time studio cabling and device topology map");
    btnOpenTopology.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.18f));
    btnOpenTopology.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnOpenTopology.onClick = [this] {
        if (onOpenTopologyModalClicked)
            onOpenTopologyModalClicked();
    };
    addAndMakeVisible(btnOpenTopology);

    btnSetupAudioMidi.setButtonText("Configure Audio & MIDI Ports...");
    btnSetupAudioMidi.setTooltip("Configure audio interface device, sample rate, buffer size, and MIDI ports");
    btnSetupAudioMidi.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnSetupAudioMidi.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnSetupAudioMidi.onClick = [this] {
        if (onOpenAudioSettingsClicked)
            onOpenAudioSettingsClicked();
    };
    addAndMakeVisible(btnSetupAudioMidi);

    btnSetupAbout.setButtonText("About ABDAudioLab & Research Architecture");
    btnSetupAbout.setTooltip("About ABDAudioLab - Software version, modeling engine, and research credits");
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

    // Inline About
    lblAboutVersion.setText("ABDAudioLab v1.1.0 (DSP Validation Platform)", juce::dontSendNotification);
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

    lblAboutCredits.setText(juce::String::fromUTF8(u8"© 2026 ABD Synths"), juce::dontSendNotification);
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
    lblAudioDeviceVal.setText("Audio: " + (info.audioDeviceName.isNotEmpty() ? info.audioDeviceName : "None"), juce::dontSendNotification);
    lblSampleRateVal.setText("Sample Rate: " + juce::String(info.sampleRate, 0) + " Hz", juce::dontSendNotification);
    lblLatencyVal.setText("Buffer: " + juce::String(info.bufferSize) + " samples (" + juce::String(info.latencyMs, 2) + " ms)", juce::dontSendNotification);
    lblMidiInputVal.setText("MIDI In: " + (info.midiInputName.isNotEmpty() ? info.midiInputName : "None"), juce::dontSendNotification);
    lblMidiOutputVal.setText("MIDI Out: " + (info.midiOutputName.isNotEmpty() ? info.midiOutputName : "None"), juce::dontSendNotification);

    if (!hasDetectedInterface)
    {
        hasAudioInConnected = info.audioDeviceName.isNotEmpty() && !info.audioDeviceName.containsIgnoreCase("None");
        hasAudioOutConnected = hasAudioInConnected;
        hasMidiInConnected = info.midiInputName.isNotEmpty() && !info.midiInputName.containsIgnoreCase("None");
        hasMidiOutConnected = info.midiOutputName.isNotEmpty() && !info.midiOutputName.containsIgnoreCase("None");
    }

    repaint();
}

void DrawerSetupTab::setTargetHardwareInfo(const juce::String& hwName,
                                          const juce::String& submoduleName,
                                          const juce::String& routingText,
                                          const juce::Image& rasterImg,
                                          const juce::Drawable* svgDrawable,
                                          const juce::String& category,
                                          const juce::String& statusText,
                                          std::optional<juce::Colour> statusColour)
{
    targetHwName = hwName.isNotEmpty() ? hwName : "Direct Loopback (No Target Selected)";
    targetSubmoduleName = submoduleName.isNotEmpty() ? ("Submodule: " + submoduleName)
                                                     : "Profile: Default Factory Setup";
    targetRoutingText = routingText.isNotEmpty() ? routingText : "Direct Loopback / Audio Routing";

    if (statusText.isNotEmpty())
    {
        targetStatusBadgeText = statusText;
        targetStatusBadgeColour = statusColour.value_or(SoundIdTheme::accentAmber);
    }
    else if (category == "PLUGIN_VIRTUAL")
    {
        targetStatusBadgeText = "VIRTUAL PLUGIN";
        targetStatusBadgeColour = SoundIdTheme::accentBlue;
    }
    else if (hwName.isEmpty() || hwName.containsIgnoreCase("Loopback") || hwName.containsIgnoreCase("Sin Target"))
    {
        targetStatusBadgeText = "DIRECT LOOPBACK";
        targetStatusBadgeColour = SoundIdTheme::accentAmber;
    }
    else
    {
        targetStatusBadgeText = "SELECTED PROFILE";
        targetStatusBadgeColour = SoundIdTheme::accentAmber;
    }

    targetRasterImage = rasterImg;
    if (svgDrawable != nullptr)
        targetSvgDrawable = svgDrawable->createCopy();
    else
        targetSvgDrawable.reset();

    if (!targetRasterImage.isValid() && targetSvgDrawable == nullptr)
    {
        juce::String cat = (category.isNotEmpty() ? category : hwName).toLowerCase();
        juce::File fallbackFile;
        if (cat.contains("pedal") || cat.contains("stompbox") || cat.contains("guitar"))
            fallbackFile = locateAssetFile("models/generic-guitar-pedal.png");
        else if (cat.contains("eurorack") || cat.contains("modular"))
            fallbackFile = locateAssetFile("models/generic-eurorack.png");
        else if (cat.contains("rack") || cat.contains("studio") || cat.contains("efecto") || cat.contains("effect"))
            fallbackFile = locateAssetFile("models/generic-audio-rack.png");
        else if (cat.contains("anal") || cat.contains("analog"))
            fallbackFile = locateAssetFile("models/generic-analog-keyboard.png");
        else
            fallbackFile = locateAssetFile("models/generic-digital-keyboard.png");

        if (fallbackFile.existsAsFile())
            targetRasterImage = juce::ImageFileFormat::loadFrom(fallbackFile);
    }

    repaint();
}

void DrawerSetupTab::setDetectedInterfaceInfo(const juce::String& ifaceName,
                                              const juce::String& detailsText,
                                              const juce::Image& ifaceImg,
                                              bool isConnectedToSoftware,
                                              bool isAudioIn,
                                              bool isAudioOut,
                                              bool isMidiIn,
                                              bool isMidiOut)
{
    hasDetectedInterface = ifaceName.isNotEmpty();
    interfaceName = ifaceName;
    interfaceDetails = detailsText;
    interfaceRasterImage = ifaceImg;
    interfaceConnectedToSoftware = isConnectedToSoftware;

    hasAudioInConnected = isAudioIn;
    hasAudioOutConnected = isAudioOut;
    hasMidiInConnected = isMidiIn;
    hasMidiOutConnected = isMidiOut;

    repaint();
    resized();
}

void DrawerSetupTab::updateTheme()
{
    btnOpenTopology.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.18f));
    btnOpenTopology.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);

    btnSetupAudioMidi.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnSetupAudioMidi.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnSetupAbout.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnSetupAbout.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);

    if (connectionsSummary != nullptr)
        connectionsSummary->updateTheme();

    lblStudioTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblStudioSubtitle.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);

    lblAboutVersion.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblAboutTagline.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblAboutArchitecture.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblAboutCredits.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);

    repaint();
}

int DrawerSetupTab::getPreferredHeight() const noexcept
{
    int h = 560;
    if (aboutSectionExpanded)
        h += 110;
    return h;
}

void DrawerSetupTab::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);
}

void DrawerSetupTab::resized()
{
    int padX = 16;
    int contentW = std::max(60, getWidth() - padX * 2);
    int y = 14;

    lblStudioTitle.setBounds(padX, y, contentW, 18);
    y += 20;
    lblStudioSubtitle.setBounds(padX, y, contentW, 16);
    y += 22;

    // 1. Centered Hero Target Card
    int heroH = 205;
    if (heroTargetDevice != nullptr)
        heroTargetDevice->setBounds(padX, y, contentW, heroH);

    y += heroH + 12;

    // 2. Real Active Connections Summary Box
    int connH = 135;
    if (connectionsSummary != nullptr)
        connectionsSummary->setBounds(padX, y, contentW, connH);

    y += connH + 14;

    // Telemetry pills / status bar
    int infoColW = contentW / 3;
    lblAudioDeviceVal.setBounds(padX, y, infoColW, 16);
    lblSampleRateVal.setBounds(padX + infoColW, y, infoColW, 16);
    lblLatencyVal.setBounds(padX + infoColW * 2, y, infoColW, 16);
    y += 18;

    lblMidiInputVal.setBounds(padX, y, infoColW, 16);
    lblMidiOutputVal.setBounds(padX + infoColW, y, infoColW, 16);
    y += 22;

    btnOpenTopology.setBounds(padX, y, contentW, 30);
    y += 36;
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

} // namespace abdaudiolab::gui
