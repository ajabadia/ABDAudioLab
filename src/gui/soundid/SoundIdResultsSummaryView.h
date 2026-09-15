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

    // Métodos de consulta y auditoría para testing e integración
    [[nodiscard]] bool isExportEnabled() const noexcept { return canExport_; }
    [[nodiscard]] synth::SelectionStatus getCurrentVerdict() const noexcept { return currentVerdict_; }
    [[nodiscard]] const std::string& getFullCanonicalHash() const noexcept { return fullCanonicalHash_; }
    [[nodiscard]] bool isHashVerified() const noexcept { return hashVerified_; }
    [[nodiscard]] juce::String getWarningsText() const { return warningsLabel_.getText(); }
    [[nodiscard]] juce::String getHashAuditText() const { return hashAuditLabel_.getText(); }

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
    juce::Label provenanceLabel_;
    juce::Label esrMetricLabel_;
    juce::Label correlationMetricLabel_;
    juce::Label criteriaComplianceLabel_;
    juce::Label validatedDomainLabel_;
    juce::Label cpuFactorLabel_;
    juce::Label warningsLabel_;

    // Auditoría de Nivel 2: Procedencia y Hash Canónico
    juce::Label hashAuditLabel_;
    juce::Label hashVerifiedBadgeLabel_;
    juce::TextButton copyHashButton_;

    // Acciones principales
    juce::TextButton exportButton_;
    juce::TextButton loadEvaluationButton_;
    juce::TextButton viewAuditDetailsButton_;
    juce::TextButton restartSessionButton_;

    void showAuditReportDialog();

    std::shared_ptr<juce::FileChooser> fileChooser_;

    bool canExport_ { false };
    bool hashVerified_ { false };
    std::string fullCanonicalHash_;
    synth::SelectionStatus currentVerdict_ { synth::SelectionStatus::Inconclusive };
    session::ProfilingSessionSnapshot lastSnapshot_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdResultsSummaryView)
};

} // namespace abdaudiolab::gui::soundid
