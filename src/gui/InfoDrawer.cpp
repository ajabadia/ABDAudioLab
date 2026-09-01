#include "InfoDrawer.h"
#include <cmath>

namespace abdaudiolab::gui
{

void InfoDrawer::PanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Solid white panel
    g.setColour(juce::Colours::white);
    g.fillRect(bounds);

    auto content = bounds.reduced(24.0f);

    // Title
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("System & Telemetry Info", content.removeFromTop(28.0f), juce::Justification::centredLeft, true);

    g.setFont(juce::FontOptions(11.5f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("Active Hardware Routing, Audio Engine & Session Metrics", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    content.removeFromTop(12.0f);

    // Section 1: Audio I/O Interface
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText("AUDIO SUBSYSTEM", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    drawTelemetryRow(g, "Audio Device", telemetry.audioDeviceName, content);
    drawTelemetryRow(g, "Driver Architecture", telemetry.audioDeviceType, content);
    drawTelemetryRow(g, "Sample Rate", juce::String(static_cast<int>(telemetry.sampleRate)) + " Hz", content);
    drawTelemetryRow(g, "Buffer Size & Latency", juce::String(telemetry.bufferSize) + " samples (" + juce::String(telemetry.latencyMs, 2) + " ms)", content);
    drawTelemetryRow(g, "Input Channels", juce::String(telemetry.inputChannels) + (telemetry.inputChannels >= 2 ? " ch (Stereo L/R)" : " ch (Mono)"), content);
    drawTelemetryRow(g, "Output Channels", juce::String(telemetry.outputChannels) + (telemetry.outputChannels >= 2 ? " ch (Stereo L/R)" : " ch (Mono)"), content);

    content.removeFromTop(10.0f);

    // Section 2: MIDI Controller
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentPurple);
    g.drawText("MIDI AUTOMATION & CONTROL", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    drawTelemetryRow(g, "Active MIDI Input", telemetry.midiInputName.isNotEmpty() ? telemetry.midiInputName : "None / Unassigned", content);
    drawTelemetryRow(g, "Active MIDI Output", telemetry.midiOutputName.isNotEmpty() ? telemetry.midiOutputName : "None / Unassigned", content);

    content.removeFromTop(10.0f);

    // Section 3: Session & Storage
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentAmber);
    g.drawText("SESSION METRICS & STORAGE", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    drawTelemetryRow(g, "Auto-Trim Headroom", juce::String(telemetry.autoTrimGainDb, 1) + " dB (Target: -3.0 dBfs)", content);
    drawTelemetryRow(g, "Completed LUT Points", juce::String(telemetry.totalMeasuredPoints) + " measured", content);
    drawTelemetryRow(g, "Export Directory", telemetry.exportDirectoryPath, content);
    drawTelemetryRow(g, "Software Release", telemetry.appVersion + " (Build #" + juce::String(telemetry.buildNumber) + ")", content);
}

void InfoDrawer::PanelComponent::drawTelemetryRow(juce::Graphics& g, const juce::String& label, const juce::String& value, juce::Rectangle<float>& area)
{
    auto row = area.removeFromTop(19.0f);

    g.setFont(juce::FontOptions(11.5f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(label, row.removeFromLeft(145.0f), juce::Justification::centredLeft, true);

    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(value, row, juce::Justification::centredLeft, true);
}

InfoDrawer::InfoDrawer()
{
    addAndMakeVisible(panel);

    btnClose.setButtonText(juce::String::fromUTF8(u8"✕"));
    btnClose.setTooltip("Close telemetry info drawer");
    btnClose.onClick = [this] { closeDrawer(); };
    panel.addAndMakeVisible(btnClose);

    btnChangeFolder.setTooltip("Select custom output destination for generated C++ tables and reports");
    btnChangeFolder.onClick = [this] {
        if (onChangeExportFolderClicked) onChangeExportFolderClicked();
    };
    panel.addAndMakeVisible(btnChangeFolder);

    btnOpenAudioSetup.setTooltip("Configure audio I/O buffer sizes, sample rates and MIDI interfaces");
    btnOpenAudioSetup.onClick = [this] {
        if (onOpenAudioSettingsClicked) onOpenAudioSettingsClicked();
    };
    panel.addAndMakeVisible(btnOpenAudioSetup);

    btnAbout.setTooltip("Display software version, architecture notes and credits");
    btnAbout.onClick = [this] {
        if (onAboutClicked) onAboutClicked();
    };
    panel.addAndMakeVisible(btnAbout);

    setAlwaysOnTop(true);
    setVisible(false);
}

void InfoDrawer::openDrawer()
{
    isOpen = true;
    targetAnimationPos = 1.0f;
    setVisible(true);
    repositionPanel();
    startTimerHz(60);
}

void InfoDrawer::closeDrawer()
{
    isOpen = false;
    targetAnimationPos = 0.0f;
    startTimerHz(60);
}

void InfoDrawer::updateTelemetry(const TelemetryInfo& info)
{
    telemetry = info;
    panel.telemetry = info;
    panel.repaint();
}

void InfoDrawer::timerCallback()
{
    float diff = targetAnimationPos - currentAnimationPos;
    if (std::abs(diff) < 0.01f)
    {
        currentAnimationPos = targetAnimationPos;
        stopTimer();
        if (!isOpen)
        {
            setVisible(false);
        }
    }
    else
    {
        currentAnimationPos += diff * 0.25f;
    }
    repositionPanel();
    repaint();
}

float InfoDrawer::getResponsivePanelWidth() const noexcept
{
    float idealWidth = static_cast<float>(getWidth()) * 0.50f;
    float panelWidth = juce::jlimit(480.0f, 750.0f, idealWidth);
    if (panelWidth > static_cast<float>(getWidth()) * 0.95f)
        panelWidth = static_cast<float>(getWidth()) * 0.95f;
    return panelWidth;
}

void InfoDrawer::repositionPanel()
{
    float panelWidth = getResponsivePanelWidth();
    float drawerX = (currentAnimationPos - 1.0f) * panelWidth;
    panel.setBounds(static_cast<int>(drawerX), 0, static_cast<int>(panelWidth), getHeight());

    auto bounds = panel.getLocalBounds();
    btnClose.setBounds(bounds.getRight() - 40, 14, 28, 28);

    auto botArea = bounds.reduced(24).removeFromBottom(126);
    btnChangeFolder.setBounds(botArea.removeFromTop(34));
    botArea.removeFromTop(6);
    btnOpenAudioSetup.setBounds(botArea.removeFromTop(34));
    botArea.removeFromTop(6);
    btnAbout.setBounds(botArea.removeFromTop(34));
}

void InfoDrawer::mouseDown(const juce::MouseEvent& e)
{
    float panelWidth = getResponsivePanelWidth();
    float drawerX = (currentAnimationPos - 1.0f) * panelWidth;

    if (e.position.x > drawerX + panelWidth)
    {
        closeDrawer();
    }
}

void InfoDrawer::resized()
{
    repositionPanel();
}

void InfoDrawer::paint(juce::Graphics& g)
{
    if (currentAnimationPos <= 0.001f) return;

    // Semi-transparent backdrop
    g.setColour(juce::Colours::black.withAlpha(0.35f * currentAnimationPos));
    g.fillRect(getLocalBounds());

    // Drop shadow
    float panelWidth = getResponsivePanelWidth();
    float drawerX = (currentAnimationPos - 1.0f) * panelWidth;
    g.setColour(juce::Colours::black.withAlpha(0.15f * currentAnimationPos));
    g.fillRoundedRectangle(drawerX + panelWidth, 0.0f, 4.0f, static_cast<float>(getHeight()), 0.0f);
}

} // namespace abdaudiolab::gui
