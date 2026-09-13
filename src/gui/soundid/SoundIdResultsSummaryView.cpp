#include "SoundIdResultsSummaryView.h"
#include "../SoundIdTheme.h"
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::soundid
{

SoundIdResultsSummaryView::SoundIdResultsSummaryView(session::IProfilingSessionCommands& commands)
    : commands_(commands)
{
    headerTitle_.setText("Paso 3: Resultados y Validación de Modelo", juce::dontSendNotification);
    headerTitle_.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText("Modelo representativo evaluado contra el conjunto holdout reservado con métricas físicas reproducibles.", juce::dontSendNotification);
    headerSubtitle_.setFont(juce::Font(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Tarjeta del Modelo
    modelCard_.setText("Modelo Acústico Recomendado");
    modelCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    modelCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(modelCard_);

    auto setupInfo = [this](juce::Label& lbl, const std::string& text, bool bold) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(14.0f, bold ? juce::Font::bold : juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, bold ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfo(modelTitleLabel_, "Modelo: En evaluación", true);
    setupInfo(verdictBadgeLabel_, "Dictamen: Inconclusive", true);
    setupInfo(esrMetricLabel_, "ESR de validación: -- dB", false);
    setupInfo(correlationMetricLabel_, "Correlación espectral rho: --", false);
    setupInfo(criteriaComplianceLabel_, "Cumplimiento del criterio: --%", false);
    setupInfo(validatedDomainLabel_, "Dominio validado: --", false);
    setupInfo(cpuFactorLabel_, "Coste relativo de CPU: 1.0x", false);
    setupInfo(warningsLabel_, "", true);

    // Botones
    exportButton_.setButtonText("EXPORTAR PAQUETE DE PRODUCCIÓN (1-CLIC)");
    exportButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    exportButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportButton_.onClick = [this]() {
        commands_.exportModel("cpp", "build/export/ModelPackage.cpp");
    };
    addAndMakeVisible(exportButton_);

    viewAuditDetailsButton_.setButtonText("Ver Informe de Auditoría y Metrología...");
    viewAuditDetailsButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    viewAuditDetailsButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    viewAuditDetailsButton_.onClick = [this]() {
        commands_.navigateToStage(session::ProfilingWorkflowStage::AdvancedSettings);
    };
    addAndMakeVisible(viewAuditDetailsButton_);

    restartSessionButton_.setButtonText("<- Nuevo Perfilado");
    restartSessionButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    restartSessionButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    restartSessionButton_.onClick = [this]() {
        commands_.navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
    };
    addAndMakeVisible(restartSessionButton_);
}

void SoundIdResultsSummaryView::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    const auto& eval = snapshot.evaluation;
    currentVerdict_ = eval.selectionStatus;
    canExport_ = snapshot.exportOptions.canExportCpp;

    if (eval.hasEvaluation)
    {
        modelTitleLabel_.setText("Modelo: " + eval.recommendedModelType, juce::dontSendNotification);
        verdictBadgeLabel_.setText("Dictamen: " + synth::selectionStatusToString(eval.selectionStatus), juce::dontSendNotification);

        std::ostringstream ssEsr;
        ssEsr << "ESR de validación (Holdout): " << std::fixed << std::setprecision(1) << eval.validationEsrDb << " dB";
        esrMetricLabel_.setText(ssEsr.str(), juce::dontSendNotification);

        std::ostringstream ssRho;
        ssRho << "Correlación espectral rho: " << std::fixed << std::setprecision(4) << eval.validationCorrelation;
        correlationMetricLabel_.setText(ssRho.str(), juce::dontSendNotification);

        std::ostringstream ssComp;
        ssComp << "Estímulos dentro del criterio: " << static_cast<int>(eval.stimuliMeetingCriterionPercent) << "%";
        criteriaComplianceLabel_.setText(ssComp.str(), juce::dontSendNotification);

        validatedDomainLabel_.setText("Dominio validado: " + eval.validatedDomain, juce::dontSendNotification);

        std::ostringstream ssCpu;
        ssCpu << "Coste relativo de CPU: " << std::fixed << std::setprecision(2) << eval.relativeCpuCostFactor << "x";
        cpuFactorLabel_.setText(ssCpu.str(), juce::dontSendNotification);
    }
    else
    {
        modelTitleLabel_.setText("Modelo: Ninguno evaluado", juce::dontSendNotification);
        verdictBadgeLabel_.setText("Dictamen: Pendiente", juce::dontSendNotification);
    }

    exportButton_.setEnabled(canExport_);

    repaint();
}

void SoundIdResultsSummaryView::paint(juce::Graphics& g)
{
    g.fillAll(SoundIdTheme::bgLight);
}

void SoundIdResultsSummaryView::resized()
{
    auto area = getLocalBounds().reduced(24);

    headerTitle_.setBounds(area.removeFromTop(28));
    headerSubtitle_.setBounds(area.removeFromTop(22));
    area.removeFromTop(16);

    // Tarjeta del modelo evaluado
    auto cardArea = area.removeFromTop(220);
    modelCard_.setBounds(cardArea);

    auto cardInner = cardArea.reduced(16, 24);
    modelTitleLabel_.setBounds(cardInner.removeFromTop(24));
    verdictBadgeLabel_.setBounds(cardInner.removeFromTop(20));
    esrMetricLabel_.setBounds(cardInner.removeFromTop(20));
    correlationMetricLabel_.setBounds(cardInner.removeFromTop(20));
    criteriaComplianceLabel_.setBounds(cardInner.removeFromTop(20));
    validatedDomainLabel_.setBounds(cardInner.removeFromTop(20));
    cpuFactorLabel_.setBounds(cardInner.removeFromTop(20));

    area.removeFromTop(24);

    // Botones
    exportButton_.setBounds(area.removeFromTop(48));
    area.removeFromTop(12);

    auto btnRow = area.removeFromTop(36);
    restartSessionButton_.setBounds(btnRow.removeFromLeft(160));
    viewAuditDetailsButton_.setBounds(btnRow.removeFromRight(260));
}

} // namespace abdaudiolab::gui::soundid
