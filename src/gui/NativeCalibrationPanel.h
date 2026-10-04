#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../math/LoopbackCalibrator.h"
#include "../audio/LabAudioEngine.h"
#include "session/ProfilingSessionContracts.h"

#include "../calibration/CalibrationProfileStore.h"

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

    void refreshSavedProfiles();
    void saveCurrentCalibrationProfile();
    void deleteSelectedProfile();

    [[nodiscard]] const math::LoopbackCalibrationData& getCalibrationData() const noexcept { return calibrationData; }
    [[nodiscard]] State getState() const noexcept { return currentState; }

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);
    void setCalibrationChannels(const juce::String& outCh, const juce::String& inCh)
    {
        calibrationOutputChannelName = outCh;
        calibrationInputChannelName = inCh;
        repaint();
    }
    [[nodiscard]] juce::String getCalibrationOutputChannel() const noexcept { return calibrationOutputChannelName; }
    [[nodiscard]] juce::String getCalibrationInputChannel() const noexcept { return calibrationInputChannelName; }

    std::function<void(const math::LoopbackCalibrationData&)> onCalibrationApplied;
    std::function<void()> onCalibrationSkipped;
    std::function<void()> onContinueToSession;
    std::function<void()> onVerifyDigitalRequested;

private:
    void processCalibrationResult();

    audio::LabAudioEngine& audioEngine;
    State currentState { State::ReadyToMeasure };
    math::LoopbackCalibrationData calibrationData;

    calibration::CalibrationProfileStore profileStore;
    std::vector<calibration::CalibrationRecord> savedProfiles;
    bool showSavedProfilesSection_ { false };
    bool showProfileDetails_ { false };
    int selectedProfileIndex_ { 0 };
    juce::String saveFeedbackText_;

    juce::String calibrationOutputChannelName { "Output 1" };
    juce::String calibrationInputChannelName { "Input 1" };

    juce::TextButton btnStartMeasure { juce::String::fromUTF8(u8"Iniciar Calibración Loopback") };
    juce::TextButton btnSkip { juce::String::fromUTF8(u8"Omitir Calibración (0 dB)") };
    juce::TextButton btnContinue { juce::String::fromUTF8(u8"Continuar a Run Session (Paso 3) ➔") };
    juce::TextButton btnRetry { juce::String::fromUTF8(u8"Repetir Calibración") };
    juce::TextButton btnVerifyDigital { juce::String::fromUTF8(u8"Verificar Latencia Digital") };

    juce::TextButton btnSaveCalibration { juce::String::fromUTF8(u8"Guardar Calibración") };
    juce::TextButton btnToggleSavedProfiles { juce::String::fromUTF8(u8"Calibraciones Guardadas") };
    juce::TextButton btnDeleteProfile { juce::String::fromUTF8(u8"Eliminar") };
    juce::TextButton btnViewProfileDetails { juce::String::fromUTF8(u8"Ver Detalles") };

    bool isDigitalMode_ { false };
    bool isDigitalVerified_ { false };
    juce::String digitalStatusText_;

    juce::ProgressBar progressBar;
    double progressValue { 0.0 };
    int measurementStep { 0 };
    float liveInputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeCalibrationPanel)
};

} // namespace abdaudiolab::gui
