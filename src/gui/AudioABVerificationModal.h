/**
 * @file AudioABVerificationModal.h
 * @brief Interactive A/B Model Verification & Spectral Correlation Dialog.
 * @details Compares physical hardware audio captures against emulated grey-box
 *          DSP models using ABDSharedCode::AudioABComparator and AudioABVerdictEngine.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../math/AudioABComparator.h"
#include "../math/AudioABVerdictEngine.h"
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

/**
 * @class AudioABVerificationModal
 * @brief Modal dialog providing side-by-side A/B cross-correlation and spectral verification.
 */
class AudioABVerificationModal : public juce::Component,
                                 public juce::KeyListener
{
public:
    AudioABVerificationModal();
    ~AudioABVerificationModal() override;

    void showDialog(juce::Component* parent);
    void dismissDialog();

    void setReferenceSignal(const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& label);
    void setCandidateSignal(const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& label);

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;

    std::function<void(const math::AudioABComparatorResult&, const math::AudioABVerdictResult&)> onVerificationCompleted;

private:
    void runComparison();
    void loadAudioFile(bool isReference);
    void synthesizeCandidateFromModel();
    void exportReportJson();
    void updateVerdictUi();

    juce::Component panel;
    juce::TextButton btnClose { "X" };

    // Reference (A) controls
    juce::Label lblRefTitle;
    juce::Label lblRefInfo;
    juce::TextButton btnLoadRef { "Browse Reference WAV (Hardware)..." };

    // Candidate (B) controls
    juce::Label lblCandTitle;
    juce::Label lblCandInfo;
    juce::TextButton btnLoadCand { "Browse Candidate WAV (Model)..." };
    juce::ComboBox comboLutModel;
    juce::TextButton btnSynthCandidate { juce::String::fromUTF8(u8"\u26a1 Synthesize Candidate from Model") };

    // Tolerance Preset
    juce::Label lblToleranceTitle;
    juce::ComboBox comboTolerance;

    // Action button
    juce::TextButton btnRunVerify { juce::String::fromUTF8(u8"\u26a1 Run Verification (A/B)") };
    juce::TextButton btnExportReport { "Export Report (JSON)..." };

    // Results Display
    juce::Label lblVerdictBadge;
    juce::Label lblVerdictSummary;

    // Metrics Cards
    juce::String metricsTimeText;
    juce::String metricsDynamicsText;
    juce::String metricsSpectralText;

    // Internal State
    math::AudioABSignal refSignal;
    math::AudioABSignal candSignal;
    juce::String refName { "No signal loaded" };
    juce::String candName { "No signal loaded" };

    math::AudioABComparatorResult lastResult;
    math::AudioABVerdictResult lastVerdict;
    bool hasResult { false };

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioABVerificationModal)
};

} // namespace abdaudiolab::gui
