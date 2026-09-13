#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../session/ProfilingSessionContracts.h"

namespace abdaudiolab::gui::soundid
{

/**
 * @brief Vista limpia del Paso 3: Resultados consolidados, métricas objetivas (ESR dB, dominio validado) y exportación 1-clic.
 */
class SoundIdResultsSummaryView : public juce::Component
{
public:
    explicit SoundIdResultsSummaryView(session::IProfilingSessionCommands& commands);
    ~SoundIdResultsSummaryView() override = default;

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    session::IProfilingSessionCommands& commands_;

    juce::Label headerTitle_;
    juce::Label headerSubtitle_;

    // Tarjeta del Modelo Recomendado
    juce::GroupComponent modelCard_;
    juce::Label modelTitleLabel_;
    juce::Label verdictBadgeLabel_;
    juce::Label esrMetricLabel_;
    juce::Label correlationMetricLabel_;
    juce::Label criteriaComplianceLabel_;
    juce::Label validatedDomainLabel_;
    juce::Label cpuFactorLabel_;
    juce::Label warningsLabel_;

    // Acciones principales
    juce::TextButton exportButton_;
    juce::TextButton viewAuditDetailsButton_;
    juce::TextButton restartSessionButton_;

    bool canExport_ { false };
    synth::SelectionStatus currentVerdict_ { synth::SelectionStatus::Inconclusive };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdResultsSummaryView)
};

} // namespace abdaudiolab::gui::soundid
