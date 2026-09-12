#include "NativeCalibrationPanel.h"
#include "SoundIdTheme.h"
#include <cmath>

namespace abdaudiolab::gui
{

NativeCalibrationPanel::NativeCalibrationPanel(audio::LabAudioEngine& engine)
    : audioEngine(engine), progressBar(progressValue)
{
    btnStartMeasure.setTooltip("Fires a 1.0s Farina logarithmic sweep to measure round-trip gain, latency and transfer function H(f)");
    btnStartMeasure.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnStartMeasure.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnStartMeasure.onClick = [this] { startCalibrationSweep(); };
    addAndMakeVisible(btnStartMeasure);

    btnSkip.setTooltip("Proceed to Step 2 with nominal unity gain (0 dB) and 0 latency samples. Disables interface latency compensation.");
    btnSkip.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnSkip.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
    btnSkip.onClick = [this] { skipCalibration(); };
    addAndMakeVisible(btnSkip);

    btnContinue.setButtonText(juce::String::fromUTF8(u8"Continuar a Hardware & Routing (Paso 2) ➔"));
    btnContinue.setTooltip("Advance to Step 2: hardware selection and routing");
    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnContinue.onClick = [this] {
        if (onContinueToSession)
            onContinueToSession();
    };
    addChildComponent(btnContinue);

    btnRetry.setTooltip("Repeat the calibration sweep after adjusting input levels");
    btnRetry.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentAmber);
    btnRetry.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    btnRetry.onClick = [this] { startCalibrationSweep(); };
    addChildComponent(btnRetry);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::borderSubtle);
    addChildComponent(progressBar);

    startTimerHz(30);
}

NativeCalibrationPanel::~NativeCalibrationPanel()
{
    stopTimer();
}

void NativeCalibrationPanel::resetToInitialState()
{
    currentState = State::ReadyToMeasure;
    measurementStep = 0;
    progressValue = 0.0;
    liveInputPeak = 0.0f;
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnSkip.setVisible(true);
    btnSkip.setEnabled(true);
    btnContinue.setVisible(false);
    btnRetry.setVisible(false);
    progressBar.setVisible(false);
    startTimerHz(30);
    repaint();
}

void NativeCalibrationPanel::startCalibrationSweep()
{
    currentState = State::Measuring;
    measurementStep = 0;
    progressValue = 0.0;
    btnStartMeasure.setEnabled(false);
    btnSkip.setEnabled(false);
    btnContinue.setVisible(false);
    btnRetry.setVisible(false);
    progressBar.setVisible(true);
    repaint();

    // 1. Arm receiver for 1.25s capture
    double sr = audioEngine.getSampleRate();
    int captureSamples = static_cast<int>(sr * 1.25);
    audioEngine.getResponseReceiver().armCapture(captureSamples, 0.005f);

    // 2. Play 1.0s Farina sweep at full band
    audioEngine.getStimulusGenerator().setStimulus(audio::StimulusType::LogFarinaSweep, 1.0, 20.0f, 40000.0f);

    startTimer(50); // 50ms tick during sweep
}

void NativeCalibrationPanel::timerCallback()
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
    repaint();
}

void NativeCalibrationPanel::processCalibrationResult()
{
    progressBar.setVisible(false);
    double sr = audioEngine.getSampleRate();

    std::vector<float> captured;
    audioEngine.getResponseReceiver().retrieveRecordedData(captured);
    calibrationData = math::LoopbackCalibrator::analyzeLoopback(captured, sr, 1.0, 20.0f, 40000.0f, -3.0f);

    if (calibrationData.isCalibrated)
    {
        currentState = State::Success;
        audioEngine.setInputAutoTrim(calibrationData.recommendedTrimGain);
        btnStartMeasure.setVisible(false);
        btnSkip.setVisible(false);
        btnRetry.setVisible(true);
        btnContinue.setVisible(true);
        btnContinue.setEnabled(true);

        if (onCalibrationApplied)
            onCalibrationApplied(calibrationData);
    }
    else
    {
        currentState = State::Failed;
        btnStartMeasure.setVisible(false);
        btnSkip.setVisible(true);
        btnSkip.setEnabled(true);
        btnRetry.setVisible(true);
        btnContinue.setVisible(false);
        startTimerHz(30);
    }
    repaint();
}

void NativeCalibrationPanel::skipCalibration()
{
    stopTimer();
    audioEngine.getResponseReceiver().reset();

    calibrationData = {};
    calibrationData.isCalibrated = false;
    calibrationData.sampleRate = audioEngine.getSampleRate();
    calibrationData.recommendedTrimGain = 1.0f; // 0 dB unity gain
    calibrationData.targetHeadroomDbfs = -3.0f;
    calibrationData.roundTripLatencyMs = 0.0f;
    calibrationData.latencySamples = 0;
    calibrationData.frequencyFlatnessDb = 0.0f;
    calibrationData.deviceName = "Bypassed / Nominal (0 dB)";

    audioEngine.setInputAutoTrim(1.0f);
    currentState = State::Skipped;

    btnStartMeasure.setVisible(false);
    btnSkip.setVisible(false);
    btnRetry.setVisible(true);
    btnContinue.setVisible(true);
    btnContinue.setEnabled(true);

    if (onCalibrationSkipped)
        onCalibrationSkipped();
    else if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);

    repaint();
}

void NativeCalibrationPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Fondo del panel central
    g.fillAll(SoundIdTheme::bgLight);

    // Tarjeta central principal
    float maxCardW = juce::jmin(840.0f, area.getWidth() - 40.0f);
    float maxCardH = juce::jmin(540.0f, area.getHeight() - 30.0f);
    auto cardBounds = juce::Rectangle<float>((area.getWidth() - maxCardW) * 0.5f,
                                            (area.getHeight() - maxCardH) * 0.5f,
                                            maxCardW, maxCardH);

    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(cardBounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(cardBounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = cardBounds.reduced(28.0f, 24.0f);

    // 1. Header Row
    auto headerRow = content.removeFromTop(32.0f);
    g.setFont(juce::FontOptions("Inter", 18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Step 1: Closed-Loop Loopback Calibration", headerRow.removeFromLeft(420.0f), juce::Justification::centredLeft, true);

    // Estado Badge
    auto badgeRect = headerRow.removeFromRight(150.0f).reduced(0.0f, 3.0f);
    if (currentState == State::Success)
    {
        g.setColour(juce::Colour(0xffd1fae5));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff065f46));
        g.drawText("● CALIBRATION COMPLETE", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Skipped)
    {
        g.setColour(juce::Colour(0xfffef3c7));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff92400e));
        g.drawText("⏭ CALIBRATION BYPASSED", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Measuring)
    {
        g.setColour(juce::Colour(0xffe0e7ff));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff3730a3));
        g.drawText("⟳ MEASURING...", badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Failed)
    {
        g.setColour(juce::Colour(0xfffee2e2));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentRed);
        g.drawText("✕ LEVEL LOW / CLIP", badgeRect, juce::Justification::centred, true);
    }
    else
    {
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("CALIBRATION PENDING", badgeRect, juce::Justification::centred, true);
    }

    content.removeFromTop(12.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(content.removeFromTop(1.0f));
    content.removeFromTop(16.0f);

    // Subtítulo explicativo
    g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("Closed-loop calibration compensates DAC/ADC round-trip latency and auto-trims input gain to the target headroom for stable, reproducible measurements.",
               content.removeFromTop(32.0f), juce::Justification::topLeft, true);

    content.removeFromTop(12.0f);

    // 2 Columnas de contenido
    float leftW = content.getWidth() * 0.54f;
    auto leftCol = content.removeFromLeft(leftW);
    content.removeFromLeft(20.0f);
    auto rightCol = content;

    // --- Columna Izquierda: 3 Pasos del protocolo ---
    auto drawStepItem = [&](int num, const juce::String& title, const juce::String& desc) {
        auto stepRow = leftCol.removeFromTop(60.0f);
        auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillEllipse(circleBounds);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(circleBounds, 1.5f);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.drawText(juce::String(num), circleBounds, juce::Justification::centred, false);

        stepRow.removeFromLeft(10.0f);
        g.setFont(juce::FontOptions("Inter", 12.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(title, stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(desc, stepRow, juce::Justification::topLeft, true);

        leftCol.removeFromTop(8.0f);
    };

    drawStepItem(1, "1. Connect Loopback Cable",
                 "Connect a physical jack cable from Audio Output 1 (DAC) to Audio Input 1 (ADC) of your interface.");
    drawStepItem(2, "2. Verify Gain Levels",
                 "Adjust your interface preamp until the real-time signal meter reads -3.0 dBFS (\u00b10.5 dBFS).");
    drawStepItem(3, "3. Trigger Farina Sweep",
                 "Click the button below to fire a 1.0s logarithmic sweep. Extracts round-trip latency and transfer function H(f).");

    // --- Columna Derecha: Monitor Balístico de Nivel & Métricas ---
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(rightCol.withHeight(200.0f), 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(rightCol.withHeight(200.0f).reduced(0.5f), 8.0f, 1.0f);

    auto meterArea = rightCol.withHeight(200.0f).reduced(14.0f, 12.0f);

    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("REAL-TIME SIGNAL MONITOR", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
    meterArea.removeFromTop(8.0f);

    // Vúmetro horizontal
    auto vumeterBar = meterArea.removeFromTop(16.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.fillRoundedRectangle(vumeterBar, 4.0f);

    float peakNorm = juce::jlimit(0.0f, 1.0f, liveInputPeak);
    auto fillBar = vumeterBar.withWidth(vumeterBar.getWidth() * peakNorm);

    if (peakNorm > 0.98f)
        g.setColour(SoundIdTheme::accentRed);
    else if (peakNorm > 0.60f)
        g.setColour(SoundIdTheme::accentGreen);
    else
        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.6f));

    g.fillRoundedRectangle(fillBar, 4.0f);

    meterArea.removeFromTop(6.0f);
    float liveDb = 20.0f * std::log10(std::max(liveInputPeak, 1e-4f));
    juce::String dbText = (liveDb < -70.0f) ? "-inf dBFS" : juce::String(liveDb, 1) + " dBFS";
    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("Input level: " + dbText, meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);

    meterArea.removeFromTop(10.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(meterArea.removeFromTop(1.0f));
    meterArea.removeFromTop(8.0f);

    // Métricas de calibración
    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    if (currentState == State::Success)
    {
        float trimDb = 20.0f * std::log10(std::max(calibrationData.recommendedTrimGain, 1e-4f));
        g.drawText("Round-trip latency: " + juce::String(calibrationData.roundTripLatencyMs, 2) + " ms (" +
                   juce::String(calibrationData.latencySamples) + " samples)",
                   meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        juce::String sign = (trimDb >= 0.0f) ? "+" : "";
        g.drawText("Auto-Trim gain: " + sign + juce::String(trimDb, 2) + " dB",
                   meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        g.drawText("H(f) flatness: \u00b1" + juce::String(calibrationData.frequencyFlatnessDb, 2) + " dB",
                   meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
    }
    else if (currentState == State::Skipped)
    {
        g.drawText("Mode: Bypassed (no latency compensation)", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        g.drawText("Gain: 1.0x (0.0 dB nominal)", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        g.drawText("Assumed latency: 0 samples", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour(SoundIdTheme::textMuted);
        g.drawText("Awaiting sweep execution...", meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
    }
}

void NativeCalibrationPanel::resized()
{
    auto area = getLocalBounds();
    int maxCardW = juce::jmin(840, area.getWidth() - 40);
    int maxCardH = juce::jmin(540, area.getHeight() - 30);
    auto cardBounds = juce::Rectangle<int>((area.getWidth() - maxCardW) / 2,
                                          (area.getHeight() - maxCardH) / 2,
                                          maxCardW, maxCardH);

    int bottomY = cardBounds.getBottom() - 56;
    int leftX = cardBounds.getX() + 28;
    int cardRight = cardBounds.getRight() - 28;

    progressBar.setBounds(leftX, bottomY - 24, cardRight - leftX, 12);

    // Botones de acción inferiores
    btnSkip.setBounds(leftX, bottomY, 240, 36);
    btnRetry.setBounds(leftX, bottomY, 180, 36);

    btnStartMeasure.setBounds(cardRight - 260, bottomY, 260, 36);
    btnContinue.setBounds(cardRight - 260, bottomY, 260, 36);
}

} // namespace abdaudiolab::gui
