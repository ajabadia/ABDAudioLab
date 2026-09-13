#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../session/ProfilingSessionContracts.h"
#include "../session/ProfilingSessionController.h"
#include "SoundIdTopHeaderStrip.h"
#include "SoundIdTargetView.h"
#include "SoundIdProfilingRunView.h"
#include "SoundIdResultsSummaryView.h"

namespace abdaudiolab::gui::soundid
{

/**
 * @brief Contenedor autónomo del flujo guiado de 3 pasos (Fase 16 / UX_SIMPLIFICATION_PLAN).
 * Aloja la banda superior persistente y conmuta dinámicamente entre TargetView, ProfilingRunView y ResultsSummaryView.
 */
class SoundIdGuidedWorkflowContainer : public juce::Component,
                                       public session::IProfilingSessionEventListener
{
public:
    explicit SoundIdGuidedWorkflowContainer(session::ProfilingSessionController& controller);
    ~SoundIdGuidedWorkflowContainer() override;

    // Implementación de IProfilingSessionEventListener
    void onSessionSnapshotUpdated(const session::ProfilingSessionSnapshot& snapshot) override;
    void onAlertRaised(const session::UiAlert& alert) override;
    void onWorkflowStageChanged(session::ProfilingWorkflowStage newStage) override;
    void onSessionStatusChanged(session::ProfilingSessionStatus newStatus) override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateTelemetry(double sampleRate, int blockSize, double cpuPercent);

private:
    session::ProfilingSessionController& controller_;
    uint64_t lastObservedSequence_ { 0 };
    uint64_t activeGeneration_ { 0 };

    SoundIdTopHeaderStrip topHeaderStrip_;
    SoundIdTargetView targetView_;
    SoundIdProfilingRunView runView_;
    SoundIdResultsSummaryView resultsView_;

    session::ProfilingWorkflowStage currentStage_ { session::ProfilingWorkflowStage::TargetSelection };

    void showStageComponent(session::ProfilingWorkflowStage stage);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdGuidedWorkflowContainer)
};

} // namespace abdaudiolab::gui::soundid
