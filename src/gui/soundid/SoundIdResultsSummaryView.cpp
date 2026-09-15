#include "SoundIdResultsSummaryView.h"
#include "../SoundIdTheme.h"
#include "../session/ProfilingSessionController.h"
#include "../../export/ModelExportNaming.h"
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
    modelCard_.setText(juce::String::fromUTF8(u8"Modelo Acústico Recomendado"));
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
    setupInfo(verdictBadgeLabel_, juce::String::fromUTF8(u8"Dictamen: Inconclusive"), true);
    setupInfo(provenanceLabel_, juce::String::fromUTF8(u8"Procedencia: No especificada"), false);
    setupInfo(warningsLabel_, "", true);
    setupInfo(esrMetricLabel_, juce::String::fromUTF8(u8"ESR de validación: -- dB"), false);
    setupInfo(correlationMetricLabel_, juce::String::fromUTF8(u8"Correlación espectral rho: --"), false);
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

    // Botones
    exportButton_.setButtonText(juce::String::fromUTF8(u8"EXPORTAR PAQUETE DE PRODUCCIÓN (1-CLIC)"));
    exportButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    exportButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportButton_.onClick = [this]() {
        commands_.recordUserClick();

        // Determinar carpeta de exportación visible y reproducible
        auto baseDir = session::ProfilingSessionController::getEvaluationsDirectory().getParentDirectory();
        juce::File exportDir = baseDir.getChildFile("exports");
        if (!exportDir.isDirectory())
            exportDir.createDirectory();

        std::string targetName = lastSnapshot_.target.targetName;
        if (targetName.empty())
            targetName = lastSnapshot_.evaluation.sourceTargetIdentity;
        if (targetName.empty())
            targetName = "Target";

        std::string modelType = lastSnapshot_.evaluation.recommendedModelType;
        if (modelType.empty())
        {
            juce::String cleanName = modelTitleLabel_.getText().replace("Modelo: ", "");
            modelType = cleanName.toStdString();
        }

        std::string baseFileName = exporting::ModelExportNaming::buildFileName(
            targetName,
            modelType,
            juce::Time::getCurrentTime(),
            fullCanonicalHash_
        );

        juce::File destFile = exporting::ModelExportNaming::resolveUniqueExportFile(exportDir, baseFileName);
        bool success = commands_.exportModel("cpp", destFile.getFullPathName().toStdString());

        if (success)
        {
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
            });
    };
    addAndMakeVisible(loadEvaluationButton_);

    viewAuditDetailsButton_.setButtonText(juce::String::fromUTF8(u8"Ver Informe de Auditoría y Metrología..."));
    viewAuditDetailsButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    viewAuditDetailsButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    viewAuditDetailsButton_.onClick = [this]() {
        commands_.recordUserClick();
        commands_.setOpenedAdvancedMode(true);
        showAuditReportDialog();
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
    currentVerdict_ = eval.selectionStatus;
    fullCanonicalHash_ = eval.canonicalEvaluationHash;
    hashVerified_ = eval.hashVerified;
    canExport_ = snapshot.exportOptions.canExportCpp && eval.hashVerified;

    if (eval.hasEvaluation)
    {
        modelTitleLabel_.setText(juce::String::fromUTF8(u8"Modelo: ") + juce::String::fromUTF8(eval.recommendedModelType.c_str()), juce::dontSendNotification);
        verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: ") + juce::String::fromUTF8(synth::selectionStatusToString(eval.selectionStatus).c_str()), juce::dontSendNotification);

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
        ssEsr << "ESR de validación (Holdout): " << std::fixed << std::setprecision(1) << eval.validationEsrDb << " dB";
        esrMetricLabel_.setText(juce::String::fromUTF8(ssEsr.str().c_str()), juce::dontSendNotification);

        std::ostringstream ssRho;
        ssRho << "Correlación espectral rho: " << std::fixed << std::setprecision(4) << eval.validationCorrelation;
        correlationMetricLabel_.setText(juce::String::fromUTF8(ssRho.str().c_str()), juce::dontSendNotification);

        std::ostringstream ssComp;
        ssComp << "Estímulos dentro del criterio: " << static_cast<int>(eval.stimuliMeetingCriterionPercent) << "%";
        criteriaComplianceLabel_.setText(juce::String::fromUTF8(ssComp.str().c_str()), juce::dontSendNotification);

        validatedDomainLabel_.setText(juce::String::fromUTF8(u8"Dominio validado: ") + juce::String::fromUTF8(eval.validatedDomain.c_str()), juce::dontSendNotification);

        std::ostringstream ssCpu;
        ssCpu << "Coste relativo de CPU: " << std::fixed << std::setprecision(2) << eval.relativeCpuCostFactor << "x";
        cpuFactorLabel_.setText(juce::String::fromUTF8(ssCpu.str().c_str()), juce::dontSendNotification);

        if (!hashVerified_ && !fullCanonicalHash_.empty())
        {
            verdictBadgeLabel_.setText(juce::String::fromUTF8(u8"Dictamen: NO DISPONIBLE (Declarado: ") + juce::String::fromUTF8(synth::selectionStatusToString(eval.selectionStatus).c_str()) + ")", juce::dontSendNotification);
            verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
            warningsLabel_.setText(juce::String::fromUTF8(u8"Resultado: INTEGRIDAD FALLIDA (El hash canónico no coincide con los datos del archivo)"), juce::dontSendNotification);
            warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
            warningsLabel_.setVisible(true);
        }
        else if (eval.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings)
        {
            verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
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
        else if (eval.selectionStatus == synth::SelectionStatus::Accepted)
        {
            verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
            warningsLabel_.setText(juce::String::fromUTF8(u8"Resultado: Modelo verificado y dentro de tolerancias metrológicas"), juce::dontSendNotification);
            warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
            warningsLabel_.setVisible(true);
        }
        else
        {
            verdictBadgeLabel_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
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
        warningsLabel_.setText(juce::String::fromUTF8(u8"No hay ninguna evaluación disponible para revisar.\n"
                                                      u8"Seleccione un dispositivo en el Paso 1 e inicie el perfilado en el Paso 2, o bien use 'Cargar Evaluación...' para inspeccionar un resultado predefinido."), juce::dontSendNotification);
        warningsLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        warningsLabel_.setVisible(true);
        hashAuditLabel_.setVisible(false);
        hashVerifiedBadgeLabel_.setVisible(false);
        copyHashButton_.setVisible(false);
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

    // Tarjeta del Modelo Recomendado
    auto cardArea = area.removeFromTop(250);
    modelCard_.setBounds(cardArea);

    auto cardInner = cardArea.reduced(16, 24);
    
    // Título y Dictamen en una fila
    auto titleRow = cardInner.removeFromTop(24);
    modelTitleLabel_.setBounds(titleRow.removeFromLeft(300));
    titleRow.removeFromLeft(16);
    verdictBadgeLabel_.setBounds(titleRow.removeFromLeft(350));

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
    cpuFactorLabel_.setBounds(metricRow3.removeFromLeft(metricRow3.getWidth() / 2 - 8));

    cardInner.removeFromTop(8);
    warningsLabel_.setBounds(cardInner.removeFromTop(24));

    area.removeFromTop(20);

    // Botón gigante de exportación (1-clic)
    exportButton_.setBounds(area.removeFromTop(48));

    area.removeFromTop(16);

    // Barra de acciones secundarias
    auto secondaryRow = area.removeFromTop(36);
    restartSessionButton_.setBounds(secondaryRow.removeFromLeft(150));
    secondaryRow.removeFromLeft(16);
    loadEvaluationButton_.setBounds(secondaryRow.removeFromLeft(200));
    viewAuditDetailsButton_.setBounds(secondaryRow.removeFromRight(280));
}

void SoundIdResultsSummaryView::showAuditReportDialog()
{
    const auto& eval = lastSnapshot_.evaluation;
    const auto& audit = lastSnapshot_.audit;
    const auto& target = lastSnapshot_.target;

    juce::String report;
    report << "================================================================================\n";
    report << juce::String::fromUTF8(u8"           INFORME TÉCNICO DE AUDITORÍA Y METROLOGÍA ACÚSTICA\n");
    report << "================================================================================\n\n";

    report << juce::String::fromUTF8(u8"1. TARGET Y PROCEDENCIA:\n");
    report << "   - " << juce::String::fromUTF8(u8"Dispositivo: ") 
           << (target.targetName.empty() ? juce::String::fromUTF8(u8"Sintetizador Virtual de Prueba") : juce::String::fromUTF8(target.targetName.c_str())) << "\n";
    if (!target.manufacturer.empty())
        report << "   - " << juce::String::fromUTF8(u8"Fabricante: ") << juce::String::fromUTF8(target.manufacturer.c_str()) << "\n";
    report << "   - " << juce::String::fromUTF8(u8"Procedencia: ")
           << juce::String::fromUTF8(synth::evaluationOriginToString(eval.evaluationOrigin).c_str()) << "\n";
    if (!eval.sourceFilePath.empty())
        report << "   - " << juce::String::fromUTF8(u8"Archivo Origen: ") << juce::String::fromUTF8(eval.sourceFilePath.c_str()) << "\n";
    report << "\n";

    report << juce::String::fromUTF8(u8"2. DICTAMEN METROLÓGICO Y VALIDACIÓN HOLDOUT:\n");
    report << "   - " << juce::String::fromUTF8(u8"Dictamen: ") 
           << juce::String::fromUTF8(synth::selectionStatusToString(eval.selectionStatus).c_str()) << "\n";
    report << "   - " << juce::String::fromUTF8(u8"Modelo Sintetizado: ") 
           << (eval.recommendedModelType.empty() ? juce::String("LUT_SIMD_2D") : juce::String::fromUTF8(eval.recommendedModelType.c_str())) << "\n";
    report << "   - " << juce::String::fromUTF8(u8"Error Relativo (ESR): ") 
           << juce::String(eval.validationEsrDb, 2) << " dB\n";
    report << "   - " << juce::String::fromUTF8(u8"Correlación Espectral (rho): ") 
           << juce::String(eval.validationCorrelation, 4) << "\n";
    report << "   - " << juce::String::fromUTF8(u8"Cumplimiento de Criterios: ") 
           << juce::String(static_cast<int>(eval.stimuliMeetingCriterionPercent)) << "%\n";
    report << "   - " << juce::String::fromUTF8(u8"Dominio Validado: ") 
           << juce::String::fromUTF8(eval.validatedDomain.c_str()) << "\n";
    report << "   - " << juce::String::fromUTF8(u8"Coste Relativo CPU: ") 
           << juce::String(eval.relativeCpuCostFactor, 2) << "x\n\n";

    report << juce::String::fromUTF8(u8"3. CARACTERÍSTICAS DINÁMICAS Y RESETEO:\n");
    report << "   - " << juce::String::fromUTF8(u8"Determinismo: ") 
           << (audit.determinismText.empty() ? juce::String::fromUTF8(u8"100% Determinista (Fixture)") : juce::String::fromUTF8(audit.determinismText.c_str())) << "\n";
    report << "   - " << juce::String::fromUTF8(u8"Capacidad Reset: ") 
           << (audit.resetCapabilityText.empty() ? juce::String::fromUTF8(u8"Reset instantáneo") : juce::String::fromUTF8(audit.resetCapabilityText.c_str())) << "\n";
    report << "   - " << juce::String::fromUTF8(u8"Tiempo de Estabilización (Settling): ") 
           << juce::String(audit.recommendedSettlingTimeMs, 1) << " ms\n";
    report << "   - " << juce::String::fromUTF8(u8"Requiere Reset entre Ensayos: ") 
           << (audit.requiresResetBeforeEachTrial ? juce::String::fromUTF8(u8"Sí") : juce::String("No")) << "\n\n";

    report << juce::String::fromUTF8(u8"4. INTEGRIDAD CRIPTOGRÁFICA (RFC 8785):\n");
    report << "   - " << juce::String::fromUTF8(u8"Hash Canónico de la Evaluación (RFC 8785): ") 
           << juce::String::fromUTF8(eval.canonicalEvaluationHash.c_str()) << "\n";
    if (!eval.sourceFileHash.empty())
    {
        report << "   - " << juce::String::fromUTF8(u8"Hash SHA-256 del Archivo Fuente en Disco: ") 
               << juce::String::fromUTF8(eval.sourceFileHash.c_str()) << "\n";
    }
    report << "   - " << juce::String::fromUTF8(u8"Estado de Verificación: ") 
           << (eval.hashVerified ? juce::String::fromUTF8(u8"VERIFICADA (Hash canónico recalculado coincidente)")
                                 : juce::String::fromUTF8(u8"FALLO DE HASH (Discrepancia de integridad en evaluación)")) << "\n\n";

    if (!eval.evaluationWarnings.empty() || !audit.operationalWarnings.empty())
    {
        report << juce::String::fromUTF8(u8"5. ADVERTENCIAS OPERATIVAS:\n");
        for (const auto& w : eval.evaluationWarnings)
            report << "   * " << juce::String::fromUTF8(w.c_str()) << "\n";
        for (const auto& w : audit.operationalWarnings)
            report << "   * " << juce::String::fromUTF8(w.c_str()) << "\n";
        report << "\n";
    }

    if (!eval.limitingFactors.empty())
    {
        report << juce::String::fromUTF8(u8"6. FACTORES LIMITANTES:\n");
        for (const auto& lf : eval.limitingFactors)
            report << "   * " << juce::String::fromUTF8(lf.c_str()) << "\n";
        report << "\n";
    }

    report << "================================================================================\n";

    auto* alert = new juce::AlertWindow(
        juce::String::fromUTF8(u8"Informe de Auditoría y Metrología Acústica"),
        report,
        juce::AlertWindow::InfoIcon);

    alert->addButton(juce::String::fromUTF8(u8"Copiar Informe"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton(juce::String::fromUTF8(u8"Cerrar"), 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true, juce::ModalCallbackFunction::create([report](int result) {
        if (result == 1)
        {
            juce::SystemClipboard::copyTextToClipboard(report);
        }
    }), true);
}

} // namespace abdaudiolab::gui::soundid
