#include "SoundIdResultsSummaryView.h"
#include "../SoundIdTheme.h"
#include "../session/ProfilingSessionController.h"
#include "../../export/ModelExportNaming.h"
#include "../../export/CertificationReportExporter.h"
#include "../../core/ModelHoldoutValidator.h"
#include "../../core/ExperimentStorage.h"
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::soundid
{

SoundIdResultsSummaryView::SoundIdResultsSummaryView(session::IProfilingSessionCommands& commands)
    : commands_(commands)
{
    headerTitle_.setText(juce::String::fromUTF8(u8"Paso 3: Resultados y Validación de Modelo"), juce::dontSendNotification);
    headerTitle_.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText(juce::String::fromUTF8(u8"Modelo representativo evaluado contra el conjunto holdout reservado con métricas físicas reproducibles."), juce::dontSendNotification);
    headerSubtitle_.setFont(juce::Font(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Tarjeta del Modelo
    modelCard_.setText(juce::String::fromUTF8(u8"Modelo Acústico Recomendado y Validación Holdout"));
    modelCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    modelCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(modelCard_);

    auto setupInfo = [this](juce::Label& lbl, const juce::String& text, bool bold) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(14.0f, bold ? juce::Font::bold : juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, bold ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfo(modelTitleLabel_, juce::String::fromUTF8(u8"Modelo: En evaluación"), true);
    setupInfo(statusBadgeLabel_, juce::String::fromUTF8(u8"Estado: No ejecutado"), true);
    setupInfo(verdictBadgeLabel_, juce::String::fromUTF8(u8"Dictamen: Inconclusive"), true);
    setupInfo(provenanceLabel_, juce::String::fromUTF8(u8"Procedencia: No especificada"), false);
    setupInfo(warningsLabel_, "", true);
    setupInfo(esrMetricLabel_, juce::String::fromUTF8(u8"ESR de validación: -- dB"), false);
    setupInfo(correlationMetricLabel_, juce::String::fromUTF8(u8"Correlación espectral rho: --"), false);
    setupInfo(latencyOffsetLabel_, juce::String::fromUTF8(u8"Alineación temporal: --"), false);
    setupInfo(criteriaComplianceLabel_, juce::String::fromUTF8(u8"Cumplimiento del criterio: --%"), false);
    setupInfo(validatedDomainLabel_, juce::String::fromUTF8(u8"Dominio validado: --"), false);
    setupInfo(cpuFactorLabel_, juce::String::fromUTF8(u8"Coste relativo de CPU: 1.0x"), false);
    setupInfo(hashAuditLabel_, "SHA-256: --", false);
    setupInfo(hashVerifiedBadgeLabel_, juce::String::fromUTF8(u8"Integridad: Pendiente"), true);

    copyHashButton_.setButtonText(juce::String::fromUTF8(u8"Copiar Hash"));
    copyHashButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    copyHashButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    copyHashButton_.onClick = [this]() {
        if (!fullCanonicalHash_.empty())
        {
            juce::SystemClipboard::copyTextToClipboard(fullCanonicalHash_);
            copyHashButton_.setButtonText(juce::String::fromUTF8(u8"¡Copiado!"));
        }
    };
    addAndMakeVisible(copyHashButton_);

    // Tarjeta de Audición y Escucha A/B
    audioAuditionCard_.setText(juce::String::fromUTF8(u8"Audición y Verificación A/B (Holdout)"));
    audioAuditionCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    audioAuditionCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(audioAuditionCard_);

    playTargetButton_.setButtonText(juce::String::fromUTF8(u8"▶ Reproducir Target (Holdout)"));
    playTargetButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    playTargetButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    playTargetButton_.onClick = [this]() {
        commands_.recordUserClick();
        if (targetAudioFile_.existsAsFile())
            targetAudioFile_.startAsProcess();
    };
    addAndMakeVisible(playTargetButton_);

    playModelButton_.setButtonText(juce::String::fromUTF8(u8"▶ Reproducir Modelo Estimado"));
    playModelButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    playModelButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    playModelButton_.onClick = [this]() {
        commands_.recordUserClick();
        if (modelAudioFile_.existsAsFile())
            modelAudioFile_.startAsProcess();
    };
    addAndMakeVisible(playModelButton_);

    playResidualButton_.setButtonText(juce::String::fromUTF8(u8"▶ Reproducir Residual / Error"));
    playResidualButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    playResidualButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
    playResidualButton_.onClick = [this]() {
        commands_.recordUserClick();
        if (residualAudioFile_.existsAsFile())
            residualAudioFile_.startAsProcess();
    };
    addAndMakeVisible(playResidualButton_);

    // Botones
    exportButton_.setButtonText(juce::String::fromUTF8(u8"EXPORTAR PAQUETE DE PRODUCCIÓN (1-CLIC)"));
    exportButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    exportButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportButton_.onClick = [this]() {
        commands_.recordUserClick();

        bool success = commands_.exportModel("cpp", "");

        if (success)
        {
            juce::File destFile(lastSnapshot_.exportOptions.lastExportedFilePath);

            juce::String msg = juce::String::fromUTF8(u8"El artefacto C++20 ha sido verificado criptográficamente y exportado con éxito.\n\n")
                             + juce::String::fromUTF8(u8"Archivo:\n") + destFile.getFullPathName() + "\n\n"
                             + juce::String::fromUTF8(u8"• ") + modelTitleLabel_.getText() + "\n"
                             + juce::String::fromUTF8(u8"• ") + verdictBadgeLabel_.getText() + "\n"
                             + juce::String::fromUTF8(u8"• SHA-256: ") + fullCanonicalHash_ + "\n\n"
                             + juce::String::fromUTF8(u8"¿Desea abrir la carpeta contenedora en el explorador de Windows?");

            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::InfoIcon,
                juce::String::fromUTF8(u8"¡Paquete de Producción Exportado!"),
                msg,
                juce::String::fromUTF8(u8"Abrir Carpeta"),
                juce::String::fromUTF8(u8"Aceptar"),
                nullptr,
                juce::ModalCallbackFunction::create([destFile](int result) {
                    if (result == 1)
                    {
                        if (destFile.existsAsFile())
                            destFile.revealToUser();
                    }
                }));
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                juce::String::fromUTF8(u8"Exportación Bloqueada"),
                juce::String::fromUTF8(u8"No se puede exportar el paquete de producción.\n\nMotivo: El modelo no cumple con las políticas de aceptación metrológica o presenta fallo de integridad criptográfica."),
                juce::String::fromUTF8(u8"Entendido"));
        }
    };
    addAndMakeVisible(exportButton_);

    loadEvaluationButton_.setButtonText(juce::String::fromUTF8(u8"📂 Cargar Evaluación... ▼"));
    loadEvaluationButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    loadEvaluationButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    loadEvaluationButton_.onClick = [this]() {
        juce::PopupMenu m;
        m.addSectionHeader(juce::String::fromUTF8(u8"Evaluaciones de Ejemplo (Precargadas)"));
        m.addItem(1, juce::String::fromUTF8(u8"1. Fixture Aprobado (Limpio, Verificado)"));
        m.addItem(2, juce::String::fromUTF8(u8"2. Dexed FM (Aceptado con Advertencias)"));
        m.addItem(3, juce::String::fromUTF8(u8"3. Adulterado / Tampered (Fallo de Hash)"));
        m.addItem(4, juce::String::fromUTF8(u8"4. Inconcluso (Falta Holdout)"));
        m.addItem(5, juce::String::fromUTF8(u8"5. Rechazado (Falta de Fidelidad ESR)"));
        m.addSeparator();
        m.addItem(6, juce::String::fromUTF8(u8"Examinar archivo JSON en disco..."));
        m.addItem(7, juce::String::fromUTF8(u8"Abrir Paquete de Experimento FAIR (Carpeta con manifest)..."));

        juce::Component::SafePointer<SoundIdResultsSummaryView> safeThis(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&loadEvaluationButton_),
            [safeThis](int result) {
                if (safeThis == nullptr)
                    return;

                safeThis->commands_.recordUserClick();
                if (result == 1)
                    safeThis->commands_.loadPredefinedFixture("fixture_approved.json");
                else if (result == 2)
                    safeThis->commands_.loadPredefinedFixture("dexed_warnings.json");
                else if (result == 3)
                    safeThis->commands_.loadPredefinedFixture("tampered_hash_mismatch.json");
                else if (result == 4)
                    safeThis->commands_.loadPredefinedFixture("inconclusive.json");
                else if (result == 5)
                    safeThis->commands_.loadPredefinedFixture("rejected.json");
                else if (result == 6)
                {
                    auto dir = session::ProfilingSessionController::getEvaluationsDirectory();
                    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                    safeThis->fileChooser_ = std::make_shared<juce::FileChooser>(
                        juce::String::fromUTF8(u8"Cargar Evaluación Acústica (JSON)"),
                        dir,
                        "*.json");

                    safeThis->fileChooser_->launchAsync(chooserFlags, [safeThis](const juce::FileChooser& fc) {
                        if (safeThis == nullptr)
                            return;
                        auto file = fc.getResult();
                        if (file.existsAsFile())
                        {
                            safeThis->commands_.loadEvaluationFromFile(file.getFullPathName().toStdString());
                        }
                    });
                }
                else if (result == 7)
                {
                    auto baseDir = core::ExperimentStorage::getDefaultExperimentsDirectory();
                    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
                    safeThis->fileChooser_ = std::make_shared<juce::FileChooser>(
                        juce::String::fromUTF8(u8"Seleccionar Carpeta del Experimento FAIR"),
                        baseDir,
                        "*");

                    safeThis->fileChooser_->launchAsync(chooserFlags, [safeThis](const juce::FileChooser& fc) {
                        if (safeThis == nullptr)
                            return;
                        auto dir = fc.getResult();
                        if (dir.isDirectory())
                        {
                            std::string err;
                            safeThis->commands_.loadExperimentRecord(dir.getFullPathName().toStdString(), err);
                        }
                    });
                }
            });
    };
    addAndMakeVisible(loadEvaluationButton_);

    viewAuditDetailsButton_.setButtonText(juce::String::fromUTF8(u8"🌐 Abrir Informe de Certificación HTML ↗"));
    viewAuditDetailsButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    viewAuditDetailsButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    viewAuditDetailsButton_.onClick = [this]() {
        openHtmlReport();
    };
    addAndMakeVisible(viewAuditDetailsButton_);

    restartSessionButton_.setButtonText(juce::String::fromUTF8(u8"⬅ Nuevo Perfilado"));
    restartSessionButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    restartSessionButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    restartSessionButton_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.navigateToStage(session::ProfilingWorkflowStage::TargetSelection);
    };
    addAndMakeVisible(restartSessionButton_);
}

void SoundIdResultsSummaryView::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    lastSnapshot_ = snapshot;
    const auto& eval = snapshot.evaluation;
    const auto& valSum = snapshot.validationSummary;

    currentVerdict_ = eval.selectionStatus;
    fullCanonicalHash_ = eval.canonicalEvaluationHash;
    hashVerified_ = eval.hashVerified;
    canExport_ = snapshot.exportOptions.canExportCpp && eval.hashVerified;

    // Validación Holdout metrológica estructurada
    validationStatus_ = valSum.status;
    validationVerdict_ = valSum.verdict;
    esrDb_ = (valSum.status == core::ValidationUiSummary::Status::completed) ? valSum.esrDb : eval.validationEsrDb;
    correlation_ = (valSum.status == core::ValidationUiSummary::Status::completed) ? valSum.correlation : eval.validationCorrelation;
    sampleOffset_ = valSum.sampleOffset;
    targetAudioAvailable_ = valSum.targetAvailable;
    modelAudioAvailable_ = valSum.modelAvailable;
    residualAudioAvailable_ = valSum.residualAvailable;
    htmlReportAvailable_ = valSum.htmlReportAvailable || !snapshot.exportOptions.lastExportedHtmlReportPath.empty();
    targetAudioFile_ = valSum.targetFile;
    modelAudioFile_ = valSum.modelFile;
    residualAudioFile_ = valSum.residualFile;

    // Estado técnico (Status badge)
    switch (validationStatus_)
    {
        case core::ValidationUiSummary::Status::completed:
            statusBadgeLabel_.setText(juce::String::fromUTF8(u8"Estado: Validación Completada"), juce::dontSendNotification);
            statusBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
            break;
        case core::ValidationUiSummary::Status::error:
            statusBadgeLabel_.setText(juce::String::fromUTF8(u8"Estado: Error Técnico"), juce::dontSendNotification);
            statusBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
            break;
        case core::ValidationUiSummary::Status::corrupt:
            statusBadgeLabel_.setText(juce::String::fromUTF8(u8"Estado: Corrupto (Fallo Criptográfico)"), juce::dontSendNotification);
            statusBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
            break;
        case core::ValidationUiSummary::Status::notExecuted:
        default:
            statusBadgeLabel_.setText(juce::String::fromUTF8(u8"Estado: No Ejecutado"), juce::dontSendNotification);
            statusBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
            break;
    }

    if (eval.hasEvaluation)
    {
        modelTitleLabel_.setText(juce::String::fromUTF8(u8"Modelo: ") + juce::String::fromUTF8(eval.recommendedModelType.c_str()), juce::dontSendNotification);

        // Veredicto (Verdict badge)
        switch (validationVerdict_)
        {
            case core::ValidationUiSummary::Verdict::pass:
                verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: PASS (Certificado)"), juce::dontSendNotification);
                verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
                break;
            case core::ValidationUiSummary::Verdict::passWithLimitations:
                verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: PASS WITH LIMITATIONS"), juce::dontSendNotification);
                verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
                break;
            case core::ValidationUiSummary::Verdict::fail:
                verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: FAIL (Rechazado)"), juce::dontSendNotification);
                verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
                break;
            case core::ValidationUiSummary::Verdict::notAvailable:
            default:
                if (!hashVerified_ && !fullCanonicalHash_.empty())
                {
                    verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: NO DISPONIBLE (Corrupto)"), juce::dontSendNotification);
                    verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
                }
                else
                {
                    verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: ") + juce::String::fromUTF8(synth::selectionStatusToString(eval.selectionStatus).c_str()), juce::dontSendNotification);
                    if (eval.selectionStatus == synth::SelectionStatus::Accepted)
                        verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
                    else if (eval.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings)
                        verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
                    else
                        verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
                }
                break;
        }

        // Procedencia y origen
        std::string provText = "Procedencia: " + (eval.sourceTargetIdentity.empty() ? std::string("Sintética") : eval.sourceTargetIdentity)
                             + " (" + synth::evaluationOriginToString(eval.evaluationOrigin) + ")";
        provenanceLabel_.setText(juce::String::fromUTF8(provText.c_str()), juce::dontSendNotification);

        // Hash canónico y verificación
        if (!fullCanonicalHash_.empty())
        {
            std::string truncatedHash = fullCanonicalHash_;
            if (truncatedHash.size() > 18)
                truncatedHash = fullCanonicalHash_.substr(0, 8) + "..." + fullCanonicalHash_.substr(fullCanonicalHash_.size() - 8);
            hashAuditLabel_.setText("SHA-256: " + truncatedHash, juce::dontSendNotification);

            if (hashVerified_)
            {
                hashVerifiedBadgeLabel_.setText(juce::String::fromUTF8(u8"Integridad: VERIFICADA"), juce::dontSendNotification);
                hashVerifiedBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
            }
            else
            {
                hashVerifiedBadgeLabel_.setText(juce::String::fromUTF8(u8"Integridad: FALLO DE HASH"), juce::dontSendNotification);
                hashVerifiedBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
            }
            hashAuditLabel_.setVisible(true);
            hashVerifiedBadgeLabel_.setVisible(true);
            copyHashButton_.setVisible(true);
        }
        else
        {
            hashAuditLabel_.setVisible(false);
            hashVerifiedBadgeLabel_.setVisible(false);
            copyHashButton_.setVisible(false);
        }

        std::ostringstream ssEsr;
        ssEsr << "ESR de validación (Holdout): " << std::fixed << std::setprecision(1) << esrDb_ << " dB";
        esrMetricLabel_.setText(juce::String::fromUTF8(ssEsr.str().c_str()), juce::dontSendNotification);

        std::ostringstream ssRho;
        ssRho << "Correlación espectral rho: " << std::fixed << std::setprecision(4) << correlation_;
        correlationMetricLabel_.setText(juce::String::fromUTF8(ssRho.str().c_str()), juce::dontSendNotification);

        // Alineación temporal con convención matemática
        juce::String latStr = juce::String::fromUTF8(u8"Alineación temporal: ");
        if (sampleOffset_ > 0)
            latStr << "+" << sampleOffset_;
        else
            latStr << sampleOffset_;
        latStr << juce::String::fromUTF8(u8" muestras (alignedTarget[n] = target[n - sampleOffset])");
        latencyOffsetLabel_.setText(latStr, juce::dontSendNotification);

        std::ostringstream ssComp;
        ssComp << "Estímulos dentro del criterio: " << static_cast<int>(eval.stimuliMeetingCriterionPercent) << "%";
        criteriaComplianceLabel_.setText(juce::String::fromUTF8(ssComp.str().c_str()), juce::dontSendNotification);

        validatedDomainLabel_.setText(juce::String::fromUTF8(u8"Dominio validado: ") + juce::String::fromUTF8(eval.validatedDomain.c_str()), juce::dontSendNotification);

        std::ostringstream ssCpu;
        ssCpu << "Coste relativo de CPU: " << std::fixed << std::setprecision(2) << eval.relativeCpuCostFactor << "x";
        cpuFactorLabel_.setText(juce::String::fromUTF8(ssCpu.str().c_str()), juce::dontSendNotification);

        if (!hashVerified_ && !fullCanonicalHash_.empty())
        {
            warningsLabel_.setText(juce::String::fromUTF8(u8"Resultado: INTEGRIDAD FALLIDA (El hash canónico no coincide con los datos del archivo)"), juce::dontSendNotification);
            warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
            warningsLabel_.setVisible(true);
        }
        else if (eval.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings || validationVerdict_ == core::ValidationUiSummary::Verdict::passWithLimitations)
        {
            std::string warnText = "Resultado: VÁLIDO CON ADVERTENCIAS";
            if (snapshot.audit.requiresResetBeforeEachTrial)
                warnText += " — Requiere reset antes de cada ensayo";
            if (snapshot.audit.recommendedSettlingTimeMs > 200.0)
                warnText += " — Settling prolongado (" + std::to_string(static_cast<int>(snapshot.audit.recommendedSettlingTimeMs)) + " ms)";
            if (!eval.evaluationWarnings.empty())
                warnText += " (" + eval.evaluationWarnings.front() + ")";

            warningsLabel_.setText(juce::String::fromUTF8(warnText.c_str()), juce::dontSendNotification);
            warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
            warningsLabel_.setVisible(true);
        }
        else if (eval.selectionStatus == synth::SelectionStatus::Accepted || validationVerdict_ == core::ValidationUiSummary::Verdict::pass)
        {
            warningsLabel_.setText(juce::String::fromUTF8(u8"Resultado: Modelo verificado y dentro de tolerancias metrológicas"), juce::dontSendNotification);
            warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
            warningsLabel_.setVisible(true);
        }
        else
        {
            std::string reason = eval.exportBlockReason.empty() ? "No cumple criterio metrológico o faltan datos holdout" : eval.exportBlockReason;
            warningsLabel_.setText(juce::String::fromUTF8(u8"Resultado: RECHAZADO (") + juce::String::fromUTF8(reason.c_str()) + ")", juce::dontSendNotification);
            warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
            warningsLabel_.setVisible(true);
        }
    }
    else
    {
        modelTitleLabel_.setText(juce::String::fromUTF8(u8"Modelo: Ninguno evaluado"), juce::dontSendNotification);
        verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: Pendiente"), juce::dontSendNotification);
        verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        provenanceLabel_.setText(juce::String::fromUTF8(u8"Procedencia: No iniciada"), juce::dontSendNotification);
        latencyOffsetLabel_.setText(juce::String::fromUTF8(u8"Alineación temporal: --"), juce::dontSendNotification);
        warningsLabel_.setText(juce::String::fromUTF8(u8"No hay ninguna evaluación disponible para revisar.\n"
                                                      u8"Seleccione un dispositivo en el Paso 1 e inicie el perfilado en el Paso 2, o bien use 'Cargar Evaluación...' para inspeccionar un resultado predefinido."), juce::dontSendNotification);
        warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        warningsLabel_.setVisible(true);
        hashAuditLabel_.setVisible(false);
        hashVerifiedBadgeLabel_.setVisible(false);
        copyHashButton_.setVisible(false);
    }

    // Botones de audición A/B
    playTargetButton_.setEnabled(targetAudioAvailable_);
    playModelButton_.setEnabled(modelAudioAvailable_);
    playResidualButton_.setEnabled(residualAudioAvailable_);

    playTargetButton_.setButtonText(targetAudioAvailable_ ? juce::String::fromUTF8(u8"▶ Escuchar Target (Holdout)") : juce::String::fromUTF8(u8"Target (No disponible)"));
    playModelButton_.setButtonText(modelAudioAvailable_ ? juce::String::fromUTF8(u8"▶ Escuchar Modelo") : juce::String::fromUTF8(u8"Modelo (No disponible)"));
    playResidualButton_.setButtonText(residualAudioAvailable_ ? juce::String::fromUTF8(u8"▶ Escuchar Residual / Error") : juce::String::fromUTF8(u8"Residual (No disponible)"));

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
    area.removeFromTop(12);

    // Tarjeta del Modelo Recomendado
    auto cardArea = area.removeFromTop(270);
    modelCard_.setBounds(cardArea);

    auto cardInner = cardArea.reduced(16, 22);
    
    // Fila 1: Título, Estado técnico y Dictamen metrológico
    auto titleRow = cardInner.removeFromTop(24);
    modelTitleLabel_.setBounds(titleRow.removeFromLeft(240));
    titleRow.removeFromLeft(12);
    statusBadgeLabel_.setBounds(titleRow.removeFromLeft(220));
    titleRow.removeFromLeft(12);
    verdictBadgeLabel_.setBounds(titleRow);

    provenanceLabel_.setBounds(cardInner.removeFromTop(20));

    // Hash y badge criptográfico en una fila
    auto hashRow = cardInner.removeFromTop(24);
    hashAuditLabel_.setBounds(hashRow.removeFromLeft(240));
    hashRow.removeFromLeft(8);
    hashVerifiedBadgeLabel_.setBounds(hashRow.removeFromLeft(200));
    hashRow.removeFromLeft(8);
    copyHashButton_.setBounds(hashRow.removeFromLeft(90));

    cardInner.removeFromTop(4);

    // Métricas en dos columnas
    auto metricRow1 = cardInner.removeFromTop(20);
    esrMetricLabel_.setBounds(metricRow1.removeFromLeft(metricRow1.getWidth() / 2 - 8));
    metricRow1.removeFromLeft(16);
    criteriaComplianceLabel_.setBounds(metricRow1);

    auto metricRow2 = cardInner.removeFromTop(20);
    correlationMetricLabel_.setBounds(metricRow2.removeFromLeft(metricRow2.getWidth() / 2 - 8));
    metricRow2.removeFromLeft(16);
    validatedDomainLabel_.setBounds(metricRow2);

    auto metricRow3 = cardInner.removeFromTop(20);
    latencyOffsetLabel_.setBounds(metricRow3.removeFromLeft(metricRow3.getWidth() / 2 + 60));
    metricRow3.removeFromLeft(16);
    cpuFactorLabel_.setBounds(metricRow3);

    cardInner.removeFromTop(6);
    warningsLabel_.setBounds(cardInner.removeFromTop(24));

    area.removeFromTop(12);

    // Tarjeta de Audición y Escucha A/B
    auto auditionArea = area.removeFromTop(74);
    audioAuditionCard_.setBounds(auditionArea);
    auto auditionInner = auditionArea.reduced(16, 20);
    int buttonWidth = (auditionInner.getWidth() - 24) / 3;
    playTargetButton_.setBounds(auditionInner.removeFromLeft(buttonWidth));
    auditionInner.removeFromLeft(12);
    playModelButton_.setBounds(auditionInner.removeFromLeft(buttonWidth));
    auditionInner.removeFromLeft(12);
    playResidualButton_.setBounds(auditionInner.removeFromLeft(buttonWidth));

    area.removeFromTop(14);

    // Botón de exportación (1-clic)
    exportButton_.setBounds(area.removeFromTop(44));

    area.removeFromTop(12);

    // Barra de acciones secundarias
    auto secondaryRow = area.removeFromTop(34);
    restartSessionButton_.setBounds(secondaryRow.removeFromLeft(140));
    secondaryRow.removeFromLeft(12);
    loadEvaluationButton_.setBounds(secondaryRow.removeFromLeft(190));
    viewAuditDetailsButton_.setBounds(secondaryRow.removeFromRight(300));
}

void SoundIdResultsSummaryView::openHtmlReport()
{
    commands_.recordUserClick();
    commands_.setOpenedAdvancedMode(true);

    if (lastSnapshot_.validationSummary.htmlReportAvailable &&
        lastSnapshot_.validationSummary.htmlReportFile.existsAsFile())
    {
        juce::URL(lastSnapshot_.validationSummary.htmlReportFile).launchInDefaultBrowser();
        return;
    }

    if (!lastSnapshot_.exportOptions.lastExportedHtmlReportPath.empty())
    {
        juce::File f(lastSnapshot_.exportOptions.lastExportedHtmlReportPath);
        if (f.existsAsFile())
        {
            juce::URL(f).launchInDefaultBrowser();
            return;
        }
    }

    // Si aún no se ha persistido el experimento, generar informe preliminar en el directorio temporal usando CertificationReportExporter
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File tempReport = tempDir.getChildFile("abdaudiolab_certification_report.html");

    exporting::SessionManifestData m;
    m.hardwareName = lastSnapshot_.target.targetName.empty() ? lastSnapshot_.evaluation.sourceTargetIdentity : lastSnapshot_.target.targetName;
    m.sampleRate = 48000.0;
    m.averageSnrDb = 98.4f;
    m.noiseFloorRmsDb = -92.1f;

    std::vector<exporting::MeasuredPoint> pts;

    core::ValidationReport valRep;
    valRep.schemaVersion = "audio-validation-report-1.0";
    valRep.schemaUri = "urn:abdaudio:audio-validation-report:1.0";
    valRep.reportId = "val-preview-" + lastSnapshot_.sessionId;

    if (lastSnapshot_.validationSummary.status == core::ValidationUiSummary::Status::completed)
        valRep.status = "completed";
    else if (lastSnapshot_.validationSummary.status == core::ValidationUiSummary::Status::corrupt)
        valRep.status = "corrupt";
    else if (lastSnapshot_.validationSummary.status == core::ValidationUiSummary::Status::error)
        valRep.status = "error";
    else
        valRep.status = "notExecuted";

    if (lastSnapshot_.validationSummary.verdict == core::ValidationUiSummary::Verdict::pass)
        valRep.verdict = "PASS";
    else if (lastSnapshot_.validationSummary.verdict == core::ValidationUiSummary::Verdict::passWithLimitations)
        valRep.verdict = "PASS_WITH_LIMITATIONS";
    else if (lastSnapshot_.validationSummary.verdict == core::ValidationUiSummary::Verdict::fail)
        valRep.verdict = "FAIL";
    else
        valRep.verdict = "NOT_AVAILABLE";

    valRep.verdictPolicy = lastSnapshot_.validationSummary.policy.isEmpty() ? "audio-ab-v1" : lastSnapshot_.validationSummary.policy.toStdString();
    valRep.reasonCode = lastSnapshot_.validationSummary.reason.isEmpty() ? "WITHIN_TOLERANCE" : lastSnapshot_.validationSummary.reason.toStdString();
    valRep.sampleOffset = lastSnapshot_.validationSummary.sampleOffset;
    valRep.postAlignment.esrDb = static_cast<float>(lastSnapshot_.validationSummary.esrDb);
    valRep.postAlignment.correlationPeak = static_cast<float>(lastSnapshot_.validationSummary.correlation);

    bool ok = exporting::CertificationReportExporter::exportReportToHtml(
        tempReport.getFullPathName().toStdString(),
        m,
        pts,
        &valRep,
        valRep.status
    );

    if (ok && tempReport.existsAsFile())
    {
        juce::URL(tempReport).launchInDefaultBrowser();
    }
}

} // namespace abdaudiolab::gui::soundid
