#pragma once

#include "../math/LoopbackCalibrator.h"
#include "../audio/LabAudioEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief Light-themed interactive Loopback Line Calibration Dialog.
 * Measures sound card transfer function H(f), latency, THD+N and computes -3.0 dBfs auto-trim.
 */
class LoopbackCalibrationModal : public juce::Component,
                                 public juce::Timer
{
public:
    enum class State
    {
        ReadyToMeasure,
        Measuring,
        Success,
        Failed
    };

    explicit LoopbackCalibrationModal(audio::LabAudioEngine& engine);
    ~LoopbackCalibrationModal() override = default;

    void showDialog(juce::Component* parent);
    void dismissDialog();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;

    std::function<void(const math::LoopbackCalibrationData&)> onCalibrationApplied;

    [[nodiscard]] const math::LoopbackCalibrationData& getCalibrationData() const noexcept { return calibrationData; }

private:
    void startCalibrationSweep();
    void processCalibrationResult();

    audio::LabAudioEngine& audioEngine;
    State currentState { State::ReadyToMeasure };
    math::LoopbackCalibrationData calibrationData;

    juce::Component panel;
    juce::TextButton btnClose { "X" };
    juce::TextButton btnStartMeasure { "Start Loopback Measurement" };
    juce::TextButton btnApplyAndClose { "Apply Trim & Save Profile" };
    juce::TextButton btnCancel { "Cancel" };

    juce::ProgressBar progressBar;
    double progressValue { 0.0 };
    int measurementStep { 0 };
    float liveInputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopbackCalibrationModal)
};

} // namespace abdaudiolab::gui
