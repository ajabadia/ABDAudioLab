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

    juce::TextButton btnStartMeasure { juce::String::fromUTF8(u8"Iniciar Calibración Loopback") };
    juce::TextButton btnSkip { juce::String::fromUTF8(u8"Omitir Calibración (Bypass Nominal 0 dB)") };
    juce::TextButton btnContinue { juce::String::fromUTF8(u8"Continuar a Ejecutar Sesión ➔") };
    juce::TextButton btnRetry { juce::String::fromUTF8(u8"Reintentar Calibración") };

    juce::ProgressBar progressBar;
    double progressValue { 0.0 };
    int measurementStep { 0 };
    float liveInputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeCalibrationPanel)
};

} // namespace abdaudiolab::gui
