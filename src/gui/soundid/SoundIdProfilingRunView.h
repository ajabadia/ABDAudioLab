#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../session/ProfilingSessionContracts.h"

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

    juce::TextButton pauseButton_;
    juce::TextButton cancelButton_;

    double currentProgress_ { 0.0 };
    bool isProfilingActive_ { false };
    bool isPaused_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdProfilingRunView)
};

} // namespace abdaudiolab::gui::soundid
