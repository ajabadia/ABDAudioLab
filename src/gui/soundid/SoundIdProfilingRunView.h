#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../session/ProfilingSessionContracts.h"
#include <string>

namespace abdaudiolab::gui::soundid
{

/**
 * @brief Vista limpia del Paso 2: Revisión pre-ejecución, botón gigante de inicio y monitor de perfilado activo.
 */
class SoundIdProfilingRunView : public juce::Component
{
public:
    explicit SoundIdProfilingRunView(session::IProfilingSessionCommands& commands);
    ~SoundIdProfilingRunView() override;

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onStartClicked;
    std::function<void()> onPauseClicked;
    std::function<void()> onCancelClicked;

#ifdef ABD_TESTING
    void resetTestCounters() { testUpdateExecutedCount_ = testSetTextCount_ = testRepaintCount_ = 0; }
    int getTestUpdateExecutedCount() const { return testUpdateExecutedCount_; }
    int getTestSetTextCount() const { return testSetTextCount_; }
    int getTestRepaintCount() const { return testRepaintCount_; }
#endif

private:
    session::IProfilingSessionCommands& commands_;

    juce::Label headerTitle_;
    juce::Label headerSubtitle_;
    juce::Label modeBadgeLabel_;

    // Resumen Pre-Vuelo
    juce::GroupComponent preflightCard_;
    juce::Label preflightRecipeLabel_;
    juce::Label preflightTimeLabel_;
    juce::Label preflightWarningsLabel_;

    // Botón gigante principal de inicio y accesos directos
    juce::TextButton startButton_;
    juce::TextButton loadEvaluationButton_;
    juce::TextButton viewResultsButton_;
    juce::TextButton advancedSettingsLink_;

    std::shared_ptr<juce::FileChooser> fileChooser_;

    // Monitor activo de perfilado
    juce::GroupComponent activeMonitorCard_;
    juce::ProgressBar progressBar_;
    juce::Label trialCounterLabel_;
    juce::Label timeRemainingLabel_;
    juce::Label stimulusLabel_;
    juce::Label signalHealthLabel_;
    juce::Label trialStageBadge_;
    juce::Label midiTrialDetailsLabel_;

    // Tarjeta de interacción manual del operador (WaitingForOperator)
    juce::GroupComponent operatorStepCard_;
    juce::Label operatorPromptLabel_;
    juce::Label expectedSettingLabel_;
    juce::TextButton btnConfirmManual_ { "✓  LISTO / CAPTURAR [Espacio]" };
    juce::TextButton btnRepeatStep_ { "Repetir [R]" };
    juce::TextButton btnStepBack_ { "Paso Atrás" };

    juce::TextButton pauseButton_;
    juce::TextButton cancelButton_;

    // -------------------------------------------------------------------------
    // Dirty-check: lightweight presentation state to avoid redundant setText/repaint
    // -------------------------------------------------------------------------
    struct ProfilingRunPresentationState
    {
        int pointIndex { -1 };
        int pointCount { -1 };
        session::ProfilingSessionStatus sessionStatus { session::ProfilingSessionStatus::Idle };
        session::TrialLifecycleStage stage { session::TrialLifecycleStage::Armed };
        float rmsDb { -999.0f };
        float peakDb { -999.0f };
        bool clippingDetected { false };
        std::string warning;
        std::string stimulusDescription;
        std::string operatorPromptText;
        session::ExcitationMode excitationMode { session::ExcitationMode::AutomatedMidi };
        int activeNoteNumber { -1 };
        int activeVelocity { -1 };
        bool isPaused { false };
    };

    ProfilingRunPresentationState lastPresentationState_;
    bool hasPresentationState_ { false }; ///< False until first updateFromSnapshot call; guarantees first snapshot is never skipped

    double currentProgress_ { 0.0 };
    bool isProfilingActive_ { false };
    bool isPaused_ { false };
    bool isWaitingForOperator_ { false };

    bool keyPressed(const juce::KeyPress& key) override;

#ifdef ABD_TESTING
    mutable int testUpdateExecutedCount_ { 0 };
    mutable int testSetTextCount_        { 0 };
    mutable int testRepaintCount_        { 0 };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdProfilingRunView)
};

} // namespace abdaudiolab::gui::soundid
