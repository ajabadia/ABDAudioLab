#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../math/LoopbackCalibrator.h"
#include "../audio/LabAudioEngine.h"

namespace abdaudiolab::gui
{

class NativeCalibrationPanel : public juce::Component,
                               public juce::Timer
{
public:
    enum class State
    {
        ReadyToMeasure,
        Measuring,
        Success,
        Failed,
        Skipped
    };

    explicit NativeCalibrationPanel(audio::LabAudioEngine& engine);
    ~NativeCalibrationPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void resetToInitialState();
    void startCalibrationSweep();
    void skipCalibration();

    [[nodiscard]] const math::LoopbackCalibrationData& getCalibrationData() const noexcept { return calibrationData; }
    [[nodiscard]] State getState() const noexcept { return currentState; }

    std::function<void(const math::LoopbackCalibrationData&)> onCalibrationApplied;
    std::function<void()> onCalibrationSkipped;
    std::function<void()> onContinueToSession;

private:
    void processCalibrationResult();

    audio::LabAudioEngine& audioEngine;
    State currentState { State::ReadyToMeasure };
    math::LoopbackCalibrationData calibrationData;

    juce::TextButton btnStartMeasure { "Start Loopback Calibration" };
    juce::TextButton btnSkip { "Bypass Calibration (0 dB Nominal Gain)" };
    juce::TextButton btnContinue { "Proceed to Hardware & Routing (Step 2) \u2192" };
    juce::TextButton btnRetry { "Retry Calibration" };

    juce::ProgressBar progressBar;
    double progressValue { 0.0 };
    int measurementStep { 0 };
    float liveInputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeCalibrationPanel)
};

} // namespace abdaudiolab::gui
