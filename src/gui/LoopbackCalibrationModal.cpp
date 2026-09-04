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
    btnStartMeasure.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
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
    btnCancel.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnCancel.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnCancel.onClick = [this] { dismissDialog(); };
    panel.addAndMakeVisible(btnCancel);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::borderSubtle);
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
    liveInputPeak = 0.0f;
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnApplyAndClose.setVisible(false);
    progressBar.setVisible(false);
    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
    startTimerHz(30); // 30Hz for live signal level validation
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
    int panelW = juce::jmin(550, area.getWidth() - 32);
    int panelH = juce::jmin(380, area.getHeight() - 32);
    panel.setBounds((area.getWidth() - panelW) / 2, (area.getHeight() - panelH) / 2, panelW, panelH);

    btnClose.setBounds(panelW - 36, 12, 24, 24);

    int bottomY = panelH - 46;
    btnCancel.setBounds(24, bottomY, 90, 34);
    btnStartMeasure.setBounds(panelW - 230, bottomY, 206, 34);
    btnApplyAndClose.setBounds(panelW - 230, bottomY, 206, 34);
    progressBar.setBounds(24, bottomY - 26, panelW - 48, 14);
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

    startTimer(50); // 50ms tick during sweep
}

void LoopbackCalibrationModal::timerCallback()
{
    if (currentState == State::Measuring)
    {
        measurementStep++;
        progressValue = std::min(1.0, measurementStep * 0.05 / 1.25);

        if (measurementStep > 28) // ~1.4s
        {
            stopTimer();
            processCalibrationResult();
        }
    }
    else
    {
        // Live loopback signal detection
        float inL = audioEngine.getInputPeakL();
        float inR = audioEngine.getInputPeakR();
        liveInputPeak = std::max(liveInputPeak * 0.88f, std::max(inL, inR));
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
        startTimerHz(30);
    }
    panel.repaint();
}

void LoopbackCalibrationModal::paint(juce::Graphics& g)
{
    // Dimmed background overlay
    g.fillAll(juce::Colours::black.withAlpha(0.40f));

    // Paint Panel Card
    auto panelBounds = panel.getBounds().toFloat();
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(panelBounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(panelBounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = panelBounds.reduced(24.0f, 20.0f);

    // Header
    auto headerRow = content.removeFromTop(28.0f);
    g.setFont(juce::FontOptions("Inter", 18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Sound Card Loopback Calibration", headerRow.removeFromLeft(330.0f), juce::Justification::centredLeft, true);

    auto badgeRect = headerRow.removeFromLeft(110.0f).reduced(0.0f, 4.0f);
    if (currentState == State::Success)
    {
        g.setColour(juce::Colour(0xffd1fae5));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff065f46));
        g.drawText("CALIBRATED", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Measuring)
    {
        g.setColour(juce::Colour(0xffe0e7ff));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff3730a3));
        g.drawText("MEASURING...", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Failed)
    {
        g.setColour(juce::Colour(0xfffee2e2));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentRed);
        g.drawText("LOW LEVEL / CLIP", badgeRect, juce::Justification::centred, true);
    }

    content.removeFromTop(10.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(content.removeFromTop(1.0f));
    content.removeFromTop(12.0f);

    if (currentState == State::ReadyToMeasure || currentState == State::Measuring)
    {
        // 2-Column layout: 60% left (steps), 40% right (live signal meter validation)
        float leftW = content.getWidth() * 0.58f;
        auto leftCol = content.removeFromLeft(leftW);
        content.removeFromLeft(16.0f);
        auto rightCol = content;

        // --- Left Column: Numbered circular badges ---
        auto drawStepBadge = [&](int stepNum, const juce::String& boldLead, const juce::String& desc) {
            auto row = leftCol.removeFromTop(44.0f);
            auto badgeArea = row.removeFromLeft(22.0f).withSizeKeepingCentre(20.0f, 20.0f);

            // Circular badge
            g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
            g.fillEllipse(badgeArea);
            g.setColour(SoundIdTheme::accentGreen);
            g.drawEllipse(badgeArea, 1.2f);

            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
            g.drawText(juce::String(stepNum), badgeArea, juce::Justification::centred, false);

            row.removeFromLeft(8.0f);
            g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(boldLead, row.removeFromTop(16.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(desc, row, juce::Justification::centredLeft, true);

            leftCol.removeFromTop(4.0f);
        };

        drawStepBadge(1, "Connect Patch Cable", "DAC Out 1 -> ADC In 1 with physical 1/4\" jack.");
        drawStepBadge(2, "Set Line Level Gain", "Preamp knob at ~12 o'clock (nominal line level).");
        drawStepBadge(3, "Inject Farina Sweep", "1.0s log sweep computes H(f) and -3.0 dBfs trim.");

        // --- Right Column: Live loopback signal meter validation ---
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(rightCol.withHeight(150.0f), 8.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(rightCol.withHeight(150.0f).reduced(0.5f), 8.0f, 1.0f);

        auto meterBox = rightCol.withHeight(150.0f).reduced(12.0f, 10.0f);
        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("SIGNAL VALIDATION", meterBox.removeFromTop(14.0f), juce::Justification::centredLeft, true);

        meterBox.removeFromTop(8.0f);

        // Live horizontal meter bar
        float liveDb = liveInputPeak > 1e-4f ? 20.0f * std::log10(liveInputPeak) : -96.0f;
        float normLevel = juce::jlimit(0.0f, 1.0f, (liveDb + 60.0f) / 60.0f);

        auto barArea = meterBox.removeFromTop(10.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.fillRoundedRectangle(barArea, 5.0f);

        if (normLevel > 0.02f)
        {
            auto fillArea = barArea.withWidth(barArea.getWidth() * normLevel);
            g.setColour(liveDb >= -0.5f ? SoundIdTheme::accentRed : (liveDb >= -3.0f ? SoundIdTheme::accentAmber : SoundIdTheme::accentGreen));
            g.fillRoundedRectangle(fillArea, 5.0f);
        }

        meterBox.removeFromTop(8.0f);

        // Status text & dB reading
        bool hasSignal = (liveDb > -60.0f);
        if (hasSignal)
        {
            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText(juce::String::fromUTF8(u8"✓ Live signal detected"), meterBox.removeFromTop(16.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Roboto Mono", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText("Level: " + juce::String(liveDb, 1) + " dBFS", meterBox.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        }
        else
        {
            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentRed);
            g.drawText(juce::String::fromUTF8(u8"⚠ No loopback signal"), meterBox.removeFromTop(16.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textMuted);
            g.drawText("Check patch cable & gain", meterBox.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        }
    }
    else if (currentState == State::Success)
    {
        // Measurement Results Dashboard
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText("CALIBRATION SUCCESSFUL - SOUND CARD CHARACTERIZED", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);
        content.removeFromTop(8.0f);

        auto drawMetric = [&](const juce::String& label, const juce::String& val, const juce::String& note) {
            auto row = content.removeFromTop(22.0f);
            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(label, row.removeFromLeft(160.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Roboto Mono", 11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText(val, row.removeFromLeft(110.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(note, row, juce::Justification::centredLeft, true);
        };

        juce::String peakStr = juce::String(calibrationData.peakInDbfs, 1) + " dBFS";
        float gainDb = 20.0f * std::log10(std::max(calibrationData.recommendedTrimGain, 1e-4f));
        juce::String trimStr = (gainDb >= 0.0f ? "+" : "") + juce::String(gainDb, 1) + " dB";

        drawMetric("Input Peak Headroom:", peakStr, "(Target: -3.0 dBFS)");
        drawMetric("Recommended Auto-Trim:", trimStr, "Gain multiplier applied automatically");
        drawMetric("Round-Trip Latency:", juce::String(calibrationData.roundTripLatencyMs, 2) + " ms", "(" + juce::String(calibrationData.latencySamples) + " samples)");
        drawMetric("Frequency Flatness:", juce::String(calibrationData.frequencyFlatnessDb, 2) + " dB", "Max variance across 20 Hz - 20 kHz");
        drawMetric("Signal-to-Noise (SNR):", juce::String(calibrationData.snrDb, 1) + " dB", "THD+N: " + juce::String(calibrationData.thdPlusNoisePercent * 100.0f, 3) + "%");
    }
    else if (currentState == State::Failed)
    {
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentRed);
        g.drawText("CALIBRATION FAILED: INSUFFICIENT SIGNAL OR CLIPPING", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);
        content.removeFromTop(8.0f);

        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"• Check that Audio Out 1 is connected directly to Audio In 1 with a patch cable.\n• Check that the soundcard input volume is turned up to moderate line level.\n• Ensure Audio Settings are set to the correct physical audio interface."), content.removeFromTop(60.0f), juce::Justification::centredLeft, true);
    }
}

} // namespace abdaudiolab::gui
