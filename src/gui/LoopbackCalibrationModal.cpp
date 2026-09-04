#include "LoopbackCalibrationModal.h"
#include "SoundIdTheme.h"
#include <cmath>

namespace abdaudiolab::gui
{

LoopbackCalibrationModal::LoopbackCalibrationModal(audio::LabAudioEngine& engine)
    : audioEngine(engine), progressBar(progressValue)
{
    setAlwaysOnTop(true);
    setWantsKeyboardFocus(true);
    setVisible(false);

    addAndMakeVisible(panel);

    btnClose.setButtonText(juce::String::fromUTF8(u8"✕"));
    btnClose.setTooltip("Close calibration window");
    btnClose.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClose.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnClose.onClick = [this] { dismissDialog(); };
    panel.addAndMakeVisible(btnClose);

    btnStartMeasure.setTooltip("Start Loopback Calibration - Fire Farina log sweep across DAC->ADC to compute latency and compensation curve");
    btnStartMeasure.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillBlackBg);
    btnStartMeasure.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnStartMeasure.onClick = [this] { startCalibrationSweep(); };
    panel.addAndMakeVisible(btnStartMeasure);

    btnApplyAndClose.setTooltip("Apply & Close - Save calibration curve and recommended input trim gain");
    btnApplyAndClose.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnApplyAndClose.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnApplyAndClose.onClick = [this] {
        if (calibrationData.isCalibrated)
        {
            audioEngine.setInputAutoTrim(calibrationData.recommendedTrimGain);
            if (onCalibrationApplied)
                onCalibrationApplied(calibrationData);
        }
        dismissDialog();
    };
    panel.addChildComponent(btnApplyAndClose);

    btnCancel.setTooltip("Cancel - Discard calibration results and close");
    btnCancel.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnCancel.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnCancel.onClick = [this] { dismissDialog(); };
    panel.addAndMakeVisible(btnCancel);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xffe5e7eb));
    panel.addChildComponent(progressBar);
}

void LoopbackCalibrationModal::showDialog(juce::Component* parent)
{
    if (parent != nullptr)
    {
        setBounds(parent->getLocalBounds());
        parent->addAndMakeVisible(this);
    }
    currentState = State::ReadyToMeasure;
    measurementStep = 0;
    progressValue = 0.0;
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnApplyAndClose.setVisible(false);
    progressBar.setVisible(false);
    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
    resized();
    repaint();
}

void LoopbackCalibrationModal::dismissDialog()
{
    stopTimer();
    audioEngine.getResponseReceiver().reset();
    setVisible(false);
}

bool LoopbackCalibrationModal::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        dismissDialog();
        return true;
    }
    return false;
}

void LoopbackCalibrationModal::mouseDown(const juce::MouseEvent& e)
{
    if (!panel.getBounds().contains(e.getPosition()))
    {
        dismissDialog();
    }
}

void LoopbackCalibrationModal::resized()
{
    auto area = getLocalBounds();
    int panelW = juce::jmin(580, area.getWidth() - 32);
    int panelH = juce::jmin(440, area.getHeight() - 32);
    panel.setBounds((area.getWidth() - panelW) / 2, (area.getHeight() - panelH) / 2, panelW, panelH);

    btnClose.setBounds(panelW - 36, 12, 24, 24);

    int bottomY = panelH - 46;
    btnCancel.setBounds(24, bottomY, 110, 32);
    btnStartMeasure.setBounds(panelW - 240, bottomY, 216, 32);
    btnApplyAndClose.setBounds(panelW - 240, bottomY, 216, 32);
    progressBar.setBounds(24, bottomY - 32, panelW - 48, 18);
}

void LoopbackCalibrationModal::startCalibrationSweep()
{
    currentState = State::Measuring;
    measurementStep = 0;
    progressValue = 0.0;
    btnStartMeasure.setEnabled(false);
    progressBar.setVisible(true);
    panel.repaint();

    // 1. Arm receiver for 1.25s capture
    double sr = audioEngine.getSampleRate();
    int captureSamples = static_cast<int>(sr * 1.25);
    audioEngine.getResponseReceiver().armCapture(captureSamples, 0.005f);

    // 2. Play 1.0s Farina sweep at full band
    audioEngine.getStimulusGenerator().setStimulus(audio::StimulusType::LogFarinaSweep, 1.0, 20.0f, 40000.0f);

    startTimer(50); // 50ms tick
}

void LoopbackCalibrationModal::timerCallback()
{
    measurementStep++;
    progressValue = std::min(1.0, measurementStep * 0.05 / 1.25);

    if (measurementStep > 28) // ~1.4s
    {
        stopTimer();
        processCalibrationResult();
    }
    panel.repaint();
}

void LoopbackCalibrationModal::processCalibrationResult()
{
    progressBar.setVisible(false);
    double sr = audioEngine.getSampleRate();

    std::vector<float> captured;
    audioEngine.getResponseReceiver().retrieveRecordedData(captured);
    calibrationData = math::LoopbackCalibrator::analyzeLoopback(captured, sr, 1.0, 20.0f, 40000.0f, -3.0f);

    if (calibrationData.isCalibrated)
    {
        currentState = State::Success;
        btnStartMeasure.setVisible(false);
        btnApplyAndClose.setVisible(true);
        btnApplyAndClose.setEnabled(true);
    }
    else
    {
        currentState = State::Failed;
        btnStartMeasure.setVisible(true);
        btnStartMeasure.setEnabled(true);
        btnStartMeasure.setButtonText("Retry Calibration");
        btnApplyAndClose.setVisible(false);
    }
    panel.repaint();
}

void LoopbackCalibrationModal::paint(juce::Graphics& g)
{
    // Semi-transparent backdrop
    g.fillAll(juce::Colours::black.withAlpha(0.40f));

    // Paint Panel Card
    auto panelBounds = panel.getBounds().toFloat();
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(panelBounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(panelBounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = panelBounds.reduced(24.0f, 20.0f);

    // Header
    auto headerRow = content.removeFromTop(28.0f);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Sound Card Loopback Calibration", headerRow.removeFromLeft(340.0f), juce::Justification::centredLeft, true);

    auto badgeRect = headerRow.removeFromLeft(120.0f).reduced(0.0f, 4.0f);
    if (currentState == State::Success)
    {
        g.setColour(juce::Colour(0xffd1fae5));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff065f46));
        g.drawText("CALIBRATED", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Measuring)
    {
        g.setColour(juce::Colour(0xffe0e7ff));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff3730a3));
        g.drawText("MEASURING...", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Failed)
    {
        g.setColour(juce::Colour(0xfffee2e2));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff991b1b));
        g.drawText("LOW LEVEL / CLIP", badgeRect, juce::Justification::centred, true);
    }

    content.removeFromTop(12.0f);
    g.setColour(juce::Colour(0xffe5e7eb));
    g.fillRect(content.removeFromTop(1.0f));
    content.removeFromTop(14.0f);

    if (currentState == State::ReadyToMeasure || currentState == State::Measuring)
    {
        // Step-by-Step Instructions
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("PREPARATION GUIDE (DAC -> ADC LOOPBACK):", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        content.removeFromTop(6.0f);

        auto drawStep = [&](const juce::String& num, const juce::String& text) {
            auto row = content.removeFromTop(22.0f);
            g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText(num, row.removeFromLeft(24.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions(11.5f));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(text, row, juce::Justification::centredLeft, true);
        };

        drawStep("1.", "Connect Audio Out 1 (Left) directly to Audio In 1 (Left) with a patch cable.");
        drawStep("2.", "Set physical input preamp gain to ~12 o'clock (moderate line level).");
        drawStep("3.", "Click [Start Loopback Measurement] to inject a 1.0s Farina sweep.");
        drawStep("4.", "The system will extract soundcard H(f), latency, THD+N and align to -3.0 dBfs.");
    }
    else if (currentState == State::Success)
    {
        // Measurement Results Dashboard
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText("CALIBRATION SUCCESSFUL - SOUND CARD CHARACTERIZED", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);
        content.removeFromTop(8.0f);

        auto drawMetric = [&](const juce::String& label, const juce::String& val, const juce::String& note) {
            auto row = content.removeFromTop(24.0f);
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(label, row.removeFromLeft(160.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText(val, row.removeFromLeft(120.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions(10.5f));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(note, row, juce::Justification::centredLeft, true);
        };

        juce::String peakStr = juce::String(calibrationData.peakInDbfs, 1) + " dBfs";
        float gainDb = 20.0f * std::log10(std::max(calibrationData.recommendedTrimGain, 1e-4f));
        juce::String trimStr = (gainDb >= 0.0f ? "+" : "") + juce::String(gainDb, 1) + " dB";

        drawMetric("Input Peak Headroom:", peakStr, "(Target: -3.0 dBfs)");
        drawMetric("Recommended Auto-Trim:", trimStr, "Gain multiplier applied automatically");
        drawMetric("Round-Trip Latency:", juce::String(calibrationData.roundTripLatencyMs, 2) + " ms", "(" + juce::String(calibrationData.latencySamples) + " samples @ " + juce::String(static_cast<int>(calibrationData.sampleRate / 1000.0)) + " kHz)");
        drawMetric("Frequency Flatness:", juce::String(calibrationData.frequencyFlatnessDb, 2) + " dB", "Max variance across 20 Hz - 20 kHz");
        drawMetric("Signal-to-Noise (SNR):", juce::String(calibrationData.snrDb, 1) + " dB", "THD+N: " + juce::String(calibrationData.thdPlusNoisePercent * 100.0f, 4) + "%");

        content.removeFromTop(10.0f);
        g.setFont(juce::FontOptions(10.5f, juce::Font::italic));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Inverse filter H_cal^-1(f) is ready to de-color all subsequent hardware measurements.", content.removeFromTop(16.0f), juce::Justification::centredLeft, true);
    }
    else if (currentState == State::Failed)
    {
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentRed);
        g.drawText("CALIBRATION FAILED: INSUFFICIENT SIGNAL OR CLIPPING", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);
        content.removeFromTop(8.0f);

        g.setFont(juce::FontOptions(11.0f));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"• Check that Audio Out 1 is connected directly to Audio In 1 with a patch cable.\n• Check that the soundcard input volume is turned up.\n• Ensure Audio Settings are set to the correct physical audio interface."), content.removeFromTop(60.0f), juce::Justification::centredLeft, true);
    }
}

} // namespace abdaudiolab::gui
