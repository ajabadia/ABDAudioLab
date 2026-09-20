/**
 * @file test_SmokeStep4UI.cpp
 * @brief Automated UI Smoke Test for Step 4 (Export & Report / SoundIdResultsSummaryView).
 *
 * Validates the complete presentation, metrology inspection, audio audition availability,
 * clipboard copy, production package export, and session persistence/reload across both
 * AutomatedMidi/SysEx and ManualOperator/Analogue pathways.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>

#include <juce_gui_basics/juce_gui_basics.h>
#include "gui/session/ProfilingSessionController.h"
#include "gui/soundid/SoundIdResultsSummaryView.h"
#include "gui/controllers/LoadedSessionApplier.h"
#include "gui/WorkflowStepperBar.h"
#include "export/ReportExportService.h"
#include "core/SessionPersistenceService.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::exporting;

namespace
{
struct SmokeTempDirectory
{
    std::filesystem::path path;

    SmokeTempDirectory(const std::string& prefix = "smoke_step4")
    {
        auto tick = std::chrono::system_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
             / "abdaudiolab_smoke"
             / (prefix + "_" + std::to_string(tick));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~SmokeTempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    SmokeTempDirectory(const SmokeTempDirectory&) = delete;
    SmokeTempDirectory& operator=(const SmokeTempDirectory&) = delete;
};
} // namespace

// ===========================================================================
// SMOKE TEST 1: Recorrido Automatizado (Dexed VST3 / Roland AIRA SysEx)
// ===========================================================================
TEST_CASE("Smoke Test Paso 4 (UI): Recorrido Automatizado (Dexed / AIRA)", "[smoke][step4][ui][auto]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    SmokeTempDirectory tempDir("auto_smoke");

    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView summaryView(controller);
    summaryView.setSize(1024, 768);

    // 1. Snapshot representativo de sesión completada en modo automatizado
    ProfilingSessionSnapshot snap;
    snap.workflowMode = UiWorkflowMode::Guided;
    snap.workflowStage = ProfilingWorkflowStage::ReviewResults;
    snap.sessionStatus = ProfilingSessionStatus::Completed;

    snap.target.targetId = "AIRA_TB3_SYSEX_SEQ";
    snap.target.targetName = "Roland AIRA TB-3 SysEx";
    snap.target.kind = TargetKind::HardwareDigital;
    snap.target.isConnected = true;

    snap.excitation.excitationMode = ExcitationMode::AutomatedSysEx;
    snap.excitation.targetControlMode = TargetControlMode::MidiSysEx;
    snap.excitation.status = RecipeStatus::Valid;

    snap.evaluation.hasEvaluation = true;
    snap.evaluation.recommendedModelType = "LUT_SIMD_2D";
    snap.evaluation.selectionStatus = synth::SelectionStatus::Accepted;
    snap.evaluation.canonicalEvaluationHash = "b6776b5b32612e81478ac7c1541c5660fe60f5e150bb2872eda4518d36dff360";
    snap.evaluation.hashVerified = true;
    snap.evaluation.validatedDomain = "C1-C6, Vel 1-127";
    snap.evaluation.relativeCpuCostFactor = 0.50;

    snap.validationSummary.status = core::ValidationUiSummary::Status::completed;
    snap.validationSummary.verdict = core::ValidationUiSummary::Verdict::pass;
    snap.validationSummary.esrDb = -34.8;
    snap.validationSummary.correlation = 0.9994;
    snap.validationSummary.sampleOffset = 12;
    snap.validationSummary.targetAvailable = true;
    snap.validationSummary.modelAvailable = true;
    snap.validationSummary.residualAvailable = true;
    snap.validationSummary.htmlReportAvailable = true;

    snap.exportOptions.canExportCpp = true;

    // 2. Proyección visual en SoundIdResultsSummaryView
    REQUIRE_NOTHROW(summaryView.updateFromSnapshot(snap));

    // 3. Verificación de métricas visibles y dictamen
    CHECK(summaryView.getCurrentVerdict() == synth::SelectionStatus::Accepted);
    CHECK(summaryView.getValidationStatus() == core::ValidationUiSummary::Status::completed);
    CHECK(summaryView.getValidationVerdict() == core::ValidationUiSummary::Verdict::pass);
    CHECK(summaryView.getEsrDb() == Catch::Approx(-34.8));
    CHECK(summaryView.getCorrelation() == Catch::Approx(0.9994));
    CHECK(summaryView.getSampleOffset() == 12);
    CHECK(summaryView.isExportEnabled() == true);
    CHECK(summaryView.isHashVerified() == true);

    // 4. Verificación de controles de audición A/B habilitados
    CHECK(summaryView.isTargetAudioAvailable() == true);
    CHECK(summaryView.isModelAudioAvailable() == true);
    CHECK(summaryView.isResidualAudioAvailable() == true);
    CHECK(summaryView.isHtmlReportAvailable() == true);

    // Verificación de deshabilitado explícito si no hay audio
    snap.validationSummary.targetAvailable = false;
    snap.validationSummary.modelAvailable = false;
    snap.validationSummary.residualAvailable = false;
    summaryView.updateFromSnapshot(snap);
    CHECK(summaryView.isTargetAudioAvailable() == false);
    CHECK(summaryView.isModelAudioAvailable() == false);
    CHECK(summaryView.isResidualAudioAvailable() == false);

    // Restaurar audio disponible
    snap.validationSummary.targetAvailable = true;
    snap.validationSummary.modelAvailable = true;
    snap.validationSummary.residualAvailable = true;
    summaryView.updateFromSnapshot(snap);

    // 5. Copia y verificación de hash canónico en el portapapeles
    const std::string originalHash = summaryView.getFullCanonicalHash();
    REQUIRE(originalHash.length() == 64);
    juce::SystemClipboard::copyTextToClipboard(originalHash);
    const juce::String clipboardContent = juce::SystemClipboard::getTextFromClipboard();
    CHECK(clipboardContent.toStdString() == originalHash);

    // 6. Exportación 1-clic de ProductionPackage
    ReportExportRequest req;
    req.manifest.hardwareId = "AIRA_TB3_SYSEX_SEQ";
    req.manifest.hardwareDisplayName = "Roland AIRA TB-3 SysEx";
    req.manifest.activeFunctionId = "osc_saw_auto";
    req.manifest.activeFunctionName = "Sawtooth Oscillator";
    req.baseFileName = "AutoAiraSmoke";
    req.destinationDirectory = tempDir.path / "auto_export";
    req.options.includeProductionPackage = true;
    req.options.includeHtmlCertification = false;

    for (int i = 0; i < 3; ++i)
    {
        MeasuredPoint pt;
        pt.pointId = "PT_AUTO_" + std::to_string(i);
        pt.blockType = "VCO";
        req.measuredPoints.push_back(pt);
    }

    ReportExportResult expRes = ReportExportService::exportReport(req);
    REQUIRE(expRes.succeeded());
    REQUIRE(expRes.status == ReportExportStatus::Success);
    REQUIRE(expRes.artifacts.size() == 4);

    for (const auto& a : expRes.artifacts)
    {
        REQUIRE(std::filesystem::exists(a.publishedPath));
        REQUIRE(std::filesystem::file_size(a.publishedPath) > 0);
    }

    // Comprobar manifest exportado
    auto manifestFile = tempDir.path / "auto_export" / "AutoAiraSmoke_manifest.json";
    REQUIRE(std::filesystem::exists(manifestFile));
    std::ifstream inM(manifestFile);
    nlohmann::json jm = nlohmann::json::parse(inM);
    CHECK(jm["hardware"]["deviceType"] == "AUTOMATED_SYSEX");
    CHECK(jm["hardware"]["id"] == "AIRA_TB3_SYSEX_SEQ");
    CHECK(jm["totalPointsMeasured"] == 3);

    // 7. Persistencia y recarga de sesión (.abdlabtest)
    core::SessionManifest sManifest;
    sManifest.formatVersion = "1.0";
    sManifest.hardwareId = "AIRA_TB3_SYSEX_SEQ";
    sManifest.hardwareDisplayName = "Roland AIRA TB-3 SysEx";
    sManifest.targetModule = "VCO";
    sManifest.totalMeasuredPoints = 3;

    juce::File pkgFile( (tempDir.path / "auto_session.abdlabtest").string() );
    core::SessionSaveRequest saveReq;
    saveReq.manifest = sManifest;
    saveReq.points = req.measuredPoints;
    saveReq.destination = pkgFile;

    auto saveRes = core::SessionPersistenceService::save(saveReq);
    REQUIRE(saveRes.succeeded());
    REQUIRE(pkgFile.existsAsFile());

    // Recarga
    core::SessionLoadRequest loadReq;
    loadReq.source = pkgFile;
    auto loadRes = core::SessionPersistenceService::load(loadReq);
    REQUIRE(loadRes.succeeded());
    CHECK(loadRes.manifest.hardwareId == "AIRA_TB3_SYSEX_SEQ");
    CHECK(loadRes.points.size() == 3);

    // Cálculo de transición de Stepper al cargar sesión completada
    auto wState = LoadedSessionApplier::computeWorkflowState(
        loadRes.manifest.totalMeasuredPoints,
        loadRes.points.size());
    CHECK(wState.isSessionComplete == true);
    CHECK(wState.targetStepperStep == WorkflowStepperBar::Step::ExportReport);
    CHECK(wState.runSessionStatus == WorkflowStepperBar::StepStatus::Completed);
}

// ===========================================================================
// SMOKE TEST 2: Recorrido Manual Analógico (Moog Modular / Doepfer)
// ===========================================================================
TEST_CASE("Smoke Test Paso 4 (UI): Recorrido Manual Analógico (Moog Modular / Doepfer)", "[smoke][step4][ui][manual]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    SmokeTempDirectory tempDir("manual_smoke");

    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView summaryView(controller);
    summaryView.setSize(1024, 768);

    // 1. Snapshot representativo de sesión manual completada con confirmación
    ProfilingSessionSnapshot snap;
    snap.workflowMode = UiWorkflowMode::Guided;
    snap.workflowStage = ProfilingWorkflowStage::ReviewResults;
    snap.sessionStatus = ProfilingSessionStatus::Completed;

    snap.target.targetId = "EURORACK_MOOG_VCF";
    snap.target.targetName = "Moog Ladder Filter Eurorack";
    snap.target.kind = TargetKind::HardwareAnalogue;
    snap.target.isConnected = true;

    snap.excitation.excitationMode = ExcitationMode::ManualOperator;
    snap.excitation.targetControlMode = TargetControlMode::NoDigitalControl;
    snap.excitation.status = RecipeStatus::Valid;

    snap.evaluation.hasEvaluation = true;
    snap.evaluation.recommendedModelType = "Ladder Filter TPT";
    snap.evaluation.selectionStatus = synth::SelectionStatus::Accepted;
    snap.evaluation.canonicalEvaluationHash = "71839ae91c30c54a7e63c18618677d55f5b564de395b7414931e04878a1d62e8";
    snap.evaluation.hashVerified = true;
    snap.evaluation.validatedDomain = "Cutoff 20Hz-20kHz, Q 0.5-10";
    snap.evaluation.relativeCpuCostFactor = 1.0;

    snap.validationSummary.status = core::ValidationUiSummary::Status::completed;
    snap.validationSummary.verdict = core::ValidationUiSummary::Verdict::pass;
    snap.validationSummary.esrDb = -31.2;
    snap.validationSummary.correlation = 0.9987;
    snap.validationSummary.sampleOffset = 0;
    snap.validationSummary.targetAvailable = true;
    snap.validationSummary.modelAvailable = true;
    snap.validationSummary.residualAvailable = true;
    snap.validationSummary.htmlReportAvailable = true;

    snap.exportOptions.canExportCpp = true;

    // 2. Proyección visual en SoundIdResultsSummaryView
    REQUIRE_NOTHROW(summaryView.updateFromSnapshot(snap));

    // 3. Verificación de métricas visibles
    CHECK(summaryView.getCurrentVerdict() == synth::SelectionStatus::Accepted);
    CHECK(summaryView.getValidationStatus() == core::ValidationUiSummary::Status::completed);
    CHECK(summaryView.getValidationVerdict() == core::ValidationUiSummary::Verdict::pass);
    CHECK(summaryView.getEsrDb() == Catch::Approx(-31.2));
    CHECK(summaryView.getCorrelation() == Catch::Approx(0.9987));
    CHECK(summaryView.getSampleOffset() == 0);
    CHECK(summaryView.isExportEnabled() == true);
    CHECK(summaryView.isHashVerified() == true);

    // 4. Verificación de audición A/B
    CHECK(summaryView.isTargetAudioAvailable() == true);
    CHECK(summaryView.isModelAudioAvailable() == true);
    CHECK(summaryView.isResidualAudioAvailable() == true);

    // 5. Portapapeles
    const std::string originalHash = summaryView.getFullCanonicalHash();
    REQUIRE(originalHash.length() == 64);
    juce::SystemClipboard::copyTextToClipboard(originalHash);
    const juce::String clipboardContent = juce::SystemClipboard::getTextFromClipboard();
    CHECK(clipboardContent.toStdString() == originalHash);

    // 6. Exportación 1-clic con preservación de procedencia manual
    ReportExportRequest req;
    req.manifest.hardwareId = "EURORACK_MOOG_VCF";
    req.manifest.hardwareDisplayName = "Moog Ladder Filter Eurorack";
    req.manifest.activeFunctionId = "vcf_ladder_manual";
    req.manifest.activeFunctionName = "Ladder Lowpass 24dB";
    req.baseFileName = "ManualMoogSmoke";
    req.destinationDirectory = tempDir.path / "manual_export";
    req.context.operatorNotes = "Operator step confirmed. Cutoff at 1.2 kHz, resonance at 4.0.";
    req.context.ambientTemperatureC = 23.0;
    req.options.includeProductionPackage = true;
    req.options.includeHtmlCertification = false;

    MeasuredPoint pt;
    pt.pointId = "PT_MANUAL_1";
    pt.blockType = "VCF";
    req.measuredPoints.push_back(pt);

    ReportExportResult expRes = ReportExportService::exportReport(req);
    REQUIRE(expRes.succeeded());
    REQUIRE(expRes.status == ReportExportStatus::Success);
    REQUIRE(expRes.artifacts.size() == 4);

    // Manifiesto con operatorNotes y hardware analógico
    auto manifestFile = tempDir.path / "manual_export" / "ManualMoogSmoke_manifest.json";
    REQUIRE(std::filesystem::exists(manifestFile));
    std::ifstream inM(manifestFile);
    nlohmann::json jm = nlohmann::json::parse(inM);
    CHECK(jm["hardware"]["deviceType"] == "MANUAL_EURORACK");
    CHECK(jm["hardware"]["id"] == "EURORACK_MOOG_VCF");
    CHECK(jm["laboratoryConditions"]["operatorNotes"] == "Operator step confirmed. Cutoff at 1.2 kHz, resonance at 4.0.");
    CHECK_FALSE(jm.contains("sequenceHash"));

    // 7. Persistencia y recarga de sesión (.abdlabtest)
    core::SessionManifest sManifest;
    sManifest.formatVersion = "1.0";
    sManifest.hardwareId = "EURORACK_MOOG_VCF";
    sManifest.hardwareDisplayName = "Moog Ladder Filter Eurorack";
    sManifest.targetModule = "VCF";
    sManifest.operatorNotes = "Operator step confirmed. Cutoff at 1.2 kHz, resonance at 4.0.";
    sManifest.ambientTemperatureC = 23.0f;
    sManifest.totalMeasuredPoints = 1;

    juce::File pkgFile( (tempDir.path / "manual_session.abdlabtest").string() );
    core::SessionSaveRequest saveReq;
    saveReq.manifest = sManifest;
    saveReq.points = req.measuredPoints;
    saveReq.destination = pkgFile;

    auto saveRes = core::SessionPersistenceService::save(saveReq);
    REQUIRE(saveRes.succeeded());

    // Recarga
    core::SessionLoadRequest loadReq;
    loadReq.source = pkgFile;
    auto loadRes = core::SessionPersistenceService::load(loadReq);
    REQUIRE(loadRes.succeeded());
    CHECK(loadRes.manifest.operatorNotes == "Operator step confirmed. Cutoff at 1.2 kHz, resonance at 4.0.");
    CHECK(loadRes.manifest.hardwareId == "EURORACK_MOOG_VCF");

    // Stepper aterriza en ExportReport
    auto wState = LoadedSessionApplier::computeWorkflowState(
        loadRes.manifest.totalMeasuredPoints,
        loadRes.points.size());
    CHECK(wState.isSessionComplete == true);
    CHECK(wState.targetStepperStep == WorkflowStepperBar::Step::ExportReport);
}
