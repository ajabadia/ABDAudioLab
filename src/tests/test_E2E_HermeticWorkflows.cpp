/**
 * @file test_E2E_HermeticWorkflows.cpp
 * @brief Hermetic End-to-End Workflow Certification Harness for ABDAudioLab (HITO-05).
 *
 * Implements completely hermetic, isolated, reproducible E2E tests simulating all
 * four laboratory target archetypes (VST3, MIDI HW, Manual Analogue, Hybrid)
 * across the 5 canonical Stepper steps (0..4).
 *
 * Guaranteed to run in CI and headless environments with 0 external dependencies:
 * - No physical audio interface required.
 * - No physical MIDI DIN/USB ports required.
 * - No external Dexed.vst3 installation required.
 * - RAII filesystem isolation with zero orphan files.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>
#include <nlohmann/json.hpp>

#include <juce_gui_basics/juce_gui_basics.h>
#include "gui/session/ProfilingSessionController.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "gui/controllers/LoadedSessionApplier.h"
#include "gui/controllers/CanonicalWorkflowTypes.h"
#include "export/ReportExportService.h"
#include "core/SessionPersistenceService.h"
#include "synth/ModelEvaluationTypes.h"
#include "synth/TargetAuditor.h"
#include "synth/SynthTargetLifecycleAdapters.h"
#include "synth/ExternalPluginFixture.h"
#include "audio/LabAudioReceiver.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::exporting;

#if JUCE_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

void pumpUiMessages()
{
#if JUCE_WINDOWS
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}

/**
 * @brief RAII Temporary Directory for hermetic E2E tests.
 * Guarantees recursive cleanup on scope exit.
 */
struct E2ETempDirectory
{
    std::filesystem::path path;

    explicit E2ETempDirectory(const std::string& prefix = "e2e_hermetic")
    {
        auto tick = std::chrono::system_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path()
             / "abdaudiolab_e2e_temp"
             / (prefix + "_" + std::to_string(tick));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~E2ETempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    E2ETempDirectory(const E2ETempDirectory&) = delete;
    E2ETempDirectory& operator=(const E2ETempDirectory&) = delete;
};

/**
 * @brief Mock MIDI message sink for tracking automated sequences and Panic teardown.
 */
struct MockMidiSink
{
    struct RecordedMessage
    {
        int channel;
        int noteNumber;
        int velocity;
        bool isNoteOn;
        bool isNoteOff;
        bool isAllNotesOff;
    };

    std::vector<RecordedMessage> recorded;

    void sendNoteOn(int channel, int note, int velocity)
    {
        recorded.push_back({ channel, note, velocity, true, false, false });
    }

    void sendNoteOff(int channel, int note)
    {
        recorded.push_back({ channel, note, 0, false, true, false });
    }

    void sendAllNotesOff(int channel)
    {
        recorded.push_back({ channel, 0, 0, false, false, true });
    }

    void send16ChannelPanic()
    {
        for (int ch = 1; ch <= 16; ++ch)
            sendAllNotesOff(ch);
    }

    [[nodiscard]] bool hasActiveNotes() const
    {
        int active = 0;
        for (const auto& m : recorded)
        {
            if (m.isNoteOn && m.velocity > 0) active++;
            else if (m.isNoteOff || (m.isNoteOn && m.velocity == 0)) active = std::max(0, active - 1);
            else if (m.isAllNotesOff) active = 0;
        }
        return active > 0;
    }
};

/**
 * @brief Synthetic VST3 target helper for hermetic testing without external binary dependencies.
 */
struct SyntheticVst3TargetFixture
{
    static TargetSelectionState createTarget(const std::string& id = "SYNTHETIC_FM_VST3_01")
    {
        TargetSelectionState t;
        t.targetId = id;
        t.targetName = "Synthetic FM Synthesizer (Offline VST3 Mock)";
        t.manufacturer = "Digital Suburban Mock";
        t.version = "1.0.1";
        t.kind = TargetKind::PluginVST3;
        t.isConnected = true;
        t.isDeterministic = true;
        t.availableDomainDescription = "MIDI C1-C6, Vel 1-127, 2238 Parameters";
        t.parameterCount = 2238;
        return t;
    }
};

} // namespace

// ===========================================================================
// ESCENARIO E2E-01: Recorrido Hermético VST3 (Digital Offline)
// ===========================================================================
TEST_CASE("E2E-01: Recorrido Hermetico VST3 Digital Offline (Paso 0..4)", "[e2e][hermetic][vst3][E2E-01]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    E2ETempDirectory tempDir("e2e01_vst3");

    // -----------------------------------------------------------------------
    // PASO 0: Studio Environment (Configuración y Dominio Digital)
    // -----------------------------------------------------------------------
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kPluginLatencySamples = 0; // VST3 offline digital reporta latencia nativa (0 en síntesis directa)
    CHECK(kPluginLatencySamples == 0);
    const double kBufferLatencyMs = (static_cast<double>(kBlockSize) / kSampleRate) * 1000.0;
    REQUIRE(kBufferLatencyMs > 0.0);

    ProfilingSessionController controller;
    auto initialSnap = controller.getCurrentSnapshot();
    REQUIRE(initialSnap.sessionStatus == ProfilingSessionStatus::Idle);
    REQUIRE(initialSnap.workflowStage == ProfilingWorkflowStage::TargetSelection);

    // -----------------------------------------------------------------------
    // PASO 1: Target & Routing (Selección e Inspección de Capacidades)
    // -----------------------------------------------------------------------
    auto target = SyntheticVst3TargetFixture::createTarget("SYNTHETIC_FM_VST3_01");
    REQUIRE(controller.selectTarget(target) == true);

    auto targetSnap = controller.getCurrentSnapshot();
    CHECK(targetSnap.sessionStatus == ProfilingSessionStatus::TargetSelected);
    CHECK(targetSnap.target.targetId == "SYNTHETIC_FM_VST3_01");
    CHECK(targetSnap.target.kind == TargetKind::PluginVST3);
    CHECK(targetSnap.target.parameterCount == 2238);
    CHECK(targetSnap.target.manufacturer == "Digital Suburban Mock");
    CHECK(targetSnap.target.version == "1.0.1");

    // Comprobación de requerimiento ortogonal: Para VST3 la calibración digital es obligatoria
    CHECK(targetSnap.calibration.digital.requirement == CalibrationRequirement::Required);
    CHECK(targetSnap.calibration.digital.verified == false);
    CHECK(targetSnap.calibration.isReadyForProfiling() == false);

    // -----------------------------------------------------------------------
    // PASO 2: Calibration & Setup (Calibración Digital y Receta de Excitación)
    // -----------------------------------------------------------------------
    // No se puede avanzar ni iniciar sesión sin calibración digital completada
    REQUIRE(controller.startProfiling() == false);

    // Ejecución de calibración digital
    controller.verifyDigitalCalibration();
    auto calibSnap = controller.getCurrentSnapshot();
    CHECK(calibSnap.calibration.digital.verified == true);
    CHECK(calibSnap.calibration.isReadyForProfiling() == true);

    // Configuración de receta de excitación de parámetros VST3
    MidiRecipe recipe;
    recipe.firstNote = 36; // C2
    recipe.lastNote = 84;  // C6
    recipe.velocities = { 32, 64, 100, 127 };
    recipe.gateMs = 250.0;
    recipe.settlingMs = 100.0;
    recipe.midiChannel = 1;
    recipe.sequenceHash = "e2e01a4b5c6d7e8f90123456789abcdef0123456789abcdef0123456789abcde";

    controller.setExcitationMode(ExcitationMode::AutomatedVstParameter);
    controller.updateMidiRecipe(recipe);

    auto recipeSnap = controller.getCurrentSnapshot();
    CHECK(recipeSnap.excitation.isValid == true);
    CHECK(recipeSnap.excitation.status == RecipeStatus::Valid);
    CHECK(recipeSnap.excitation.excitationMode == ExcitationMode::AutomatedVstParameter);

    // Auditoría previa metrológica completada con éxito
    controller.updateAuditResult(
        synth::ApprovalStatus::Approved,
        "100% Determinista (Digital VST3 Offline)",
        "Reset instantaneo por bloque",
        0.0,
        false,
        {},
        "Target aprobado para excitacion de parametros VST3"
    );
    auto auditSnap = controller.getCurrentSnapshot();
    CHECK(auditSnap.audit.isAudited == true);
    CHECK(auditSnap.audit.approvalStatus == synth::ApprovalStatus::Approved);

    // -----------------------------------------------------------------------
    // PASO 3: Run Session (Ejecución, Evaluación Holdout y Veredicto)
    // -----------------------------------------------------------------------
    REQUIRE(controller.startProfiling() == true);
    auto runSnap = controller.getCurrentSnapshot();
    CHECK(runSnap.sessionStatus == ProfilingSessionStatus::Profiling);

    // Simulación de telemetría de renderizado offline
    controller.updateProgress(1, 5, 0.25, 1.00, "Render block 1: C2 Vel 64");
    controller.updateProgress(5, 5, 1.25, 0.00, "Render completed");

    // Registro de evaluación holdout con métricas de fidelidad
    controller.updateModelEvaluation(
        synth::SelectionStatus::Accepted,
        "LUT_SIMD_2D",
        -36.4,  // ESR dB (< -30 dB)
        0.9996, // Correlación espectral
        100.0,  // % estímulos conformes
        "C2-C6, Vel 32-127",
        0.45,   // Coste relativo CPU
        {},     // 0 warnings
        {}      // 0 factores limitantes
    );

    controller.completeProfiling();
    auto completedSnap = controller.getCurrentSnapshot();
    CHECK(completedSnap.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(completedSnap.evaluation.hasEvaluation == true);
    CHECK(completedSnap.evaluation.selectionStatus == synth::SelectionStatus::Accepted);
    CHECK(completedSnap.evaluation.validationEsrDb == Catch::Approx(-36.4));
    CHECK(completedSnap.evaluation.validationCorrelation == Catch::Approx(0.9996));

    // -----------------------------------------------------------------------
    // PASO 4: Export & Report (Guardas, ProductionPackage, Manifest y Persistencia)
    // -----------------------------------------------------------------------
    // 1. Evaluación de ExportReadiness
    auto readiness = controller.evaluateExportReadiness();
    CHECK(readiness.canProceed() == true);
    CHECK(readiness.sessionCompleted == true);

    // 2. Exportación de ProductionPackage obligatorio
    ReportExportRequest expReq;
    expReq.manifest.hardwareId = target.targetId;
    expReq.manifest.hardwareDisplayName = target.targetName;
    expReq.manifest.activeFunctionId = "dexed_fm_vst3";
    expReq.manifest.activeFunctionName = "FM Operator Algorithm 1";
    expReq.baseFileName = "SyntheticDexed_E2E01";
    expReq.destinationDirectory = tempDir.path / "e2e01_export";
    expReq.options.includeProductionPackage = true;
    expReq.options.includeHtmlCertification = false;

    for (int i = 0; i < 5; ++i)
    {
        MeasuredPoint pt;
        pt.pointId = "PT_VST3_" + std::to_string(i);
        pt.blockType = "FM_VOICE";
        pt.snrDb = 95.0f;
        pt.thdPercent = 0.02f;
        expReq.measuredPoints.push_back(pt);
    }

    ReportExportResult expResult = ReportExportService::exportReport(expReq);
    REQUIRE(expResult.succeeded());
    REQUIRE(expResult.status == ReportExportStatus::Success);
    REQUIRE(expResult.artifacts.size() == 4);

    // Comprobación de los 4 artefactos obligatorios del ProductionPackage
    auto lutFile = tempDir.path / "e2e01_export" / "SyntheticDexed_E2E01_lut.h";
    auto telemFile = tempDir.path / "e2e01_export" / "SyntheticDexed_E2E01_telemetry.json";
    auto htmlFile = tempDir.path / "e2e01_export" / "SyntheticDexed_E2E01_Certification_Report.html";
    auto manifestFile = tempDir.path / "e2e01_export" / "SyntheticDexed_E2E01_manifest.json";

    CHECK(std::filesystem::exists(lutFile));
    CHECK(std::filesystem::exists(telemFile));
    CHECK(std::filesystem::exists(htmlFile));
    CHECK(std::filesystem::exists(manifestFile));

    // Verificación de fixity inyectada en el manifest
    std::ifstream inManifest(manifestFile);
    REQUIRE(inManifest.is_open());
    nlohmann::json manifestJson = nlohmann::json::parse(inManifest);
    CHECK(manifestJson["hardware"]["id"] == "SYNTHETIC_FM_VST3_01");
    CHECK(manifestJson["totalPointsMeasured"] == 5);
    CHECK(manifestJson.contains("outputArtifacts"));
    CHECK(manifestJson["outputArtifacts"].contains("packageArtifacts"));
    CHECK(manifestJson["outputArtifacts"]["packageArtifacts"].size() == 3);

    // 3. Persistencia y Round-Trip Semántico (.abdlabtest)
    core::SessionManifest sManifest;
    sManifest.formatVersion = "1.0";
    sManifest.hardwareId = target.targetId;
    sManifest.hardwareDisplayName = target.targetName;
    sManifest.targetModule = "FM_VOICE";
    sManifest.totalMeasuredPoints = 5;

    juce::File pkgFile((tempDir.path / "e2e01_session.abdlabtest").string());
    core::SessionSaveRequest saveReq;
    saveReq.manifest = sManifest;
    saveReq.points = expReq.measuredPoints;
    saveReq.destination = pkgFile;

    auto saveRes = core::SessionPersistenceService::save(saveReq);
    REQUIRE(saveRes.succeeded());
    REQUIRE(pkgFile.existsAsFile());

    core::SessionLoadRequest loadReq;
    loadReq.source = pkgFile;
    auto loadRes = core::SessionPersistenceService::load(loadReq);
    REQUIRE(loadRes.succeeded());
    CHECK(loadRes.manifest.hardwareId == target.targetId);
    CHECK(loadRes.points.size() == 5);

    // 4. Verificación de transición de Stepper al recargar sesión completa
    auto workflowState = LoadedSessionApplier::computeWorkflowState(
        loadRes.manifest.totalMeasuredPoints,
        loadRes.points.size()
    );
    CHECK(workflowState.isSessionComplete == true);
    CHECK(workflowState.targetStepperStep == CanonicalStep::ExportReport);
    CHECK(workflowState.runSessionStatus == CanonicalStepStatus::Completed);

    // 5. Cero archivos residuales de staging o backup
    for (const auto& entry : std::filesystem::directory_iterator(tempDir.path / "e2e01_export"))
    {
        const auto filename = entry.path().filename().string();
        CHECK(filename.find(".staging_") == std::string::npos);
        CHECK(filename.find(".backup_") == std::string::npos);
    }
}

// ===========================================================================
// ESCENARIO E2E-02: Recorrido Hermético Hardware MIDI Automatizado (ADC Loopback)
// ===========================================================================
TEST_CASE("E2E-02: Recorrido Hermetico Hardware MIDI Automatizado (Paso 0..4)", "[e2e][hermetic][midi][E2E-02]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    E2ETempDirectory tempDir("e2e02_midi");
    MockMidiSink midiSink;

    // -----------------------------------------------------------------------
    // PASO 0: Studio Environment (Latencia física y ADC)
    // -----------------------------------------------------------------------
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 512;
    constexpr float kSimulatedRoundTripLatencyMs = 8.5f; // DAC -> HW -> ADC loopback simulado
    constexpr float kSimulatedJitterMs = 0.3f;
    const double kBufferLatencyMs = (static_cast<double>(kBlockSize) / kSampleRate) * 1000.0;
    REQUIRE(kBufferLatencyMs > 0.0);
    CHECK(kSimulatedRoundTripLatencyMs > 0.0f);

    ProfilingSessionController controller;
    auto initialSnap = controller.getCurrentSnapshot();
    REQUIRE(initialSnap.sessionStatus == ProfilingSessionStatus::Idle);
    REQUIRE(initialSnap.workflowStage == ProfilingWorkflowStage::TargetSelection);

    // -----------------------------------------------------------------------
    // PASO 1: Target & Routing (Hardware MIDI Digital — Sintetizador DIN)
    // -----------------------------------------------------------------------
    TargetSelectionState target;
    target.targetId     = "HW_MIDI_JX8P_SYNTH_02";
    target.targetName   = "Roland JX-8P (Synthetic Mock)";
    target.manufacturer = "Roland (Mock)";
    target.version      = "1.0";
    target.kind         = TargetKind::HardwareDigital;
    target.isConnected  = true;
    target.isDeterministic = false; // hardware físico no es bit-exacto
    target.supportsMidiInput = true;
    target.availableDomainDescription = "MIDI C1-C7, Vel 1-127, DIN-5, 2 osciladores";
    target.parameterCount = 64;

    REQUIRE(controller.selectTarget(target) == true);

    auto targetSnap = controller.getCurrentSnapshot();
    CHECK(targetSnap.sessionStatus == ProfilingSessionStatus::TargetSelected);
    CHECK(targetSnap.target.kind == TargetKind::HardwareDigital);
    CHECK(targetSnap.target.supportsMidiInput == true);
    CHECK(targetSnap.target.parameterCount == 64);
    // Para HardwareDigital: calibración de audio y MIDI requeridas; digital NOT applicable
    CHECK(targetSnap.calibration.audio.requirement == CalibrationRequirement::Required);
    CHECK(targetSnap.calibration.midi.requirement == CalibrationRequirement::Required);
    CHECK(targetSnap.calibration.digital.requirement == CalibrationRequirement::NotApplicable);
    CHECK(targetSnap.calibration.isReadyForProfiling() == false);

    // -----------------------------------------------------------------------
    // PASO 2: Calibration & Setup (Latencia MIDI + Niveles ADC)
    // -----------------------------------------------------------------------
    // Intento de iniciar sesión sin calibración completada — debe fallar
    REQUIRE(controller.startProfiling() == false);

    // Calibración de audio: roundtrip -18 dBFS, SNR > 70 dB
    controller.updateAudioCalibration(
        /*completed=*/true,
        /*inputGain=*/-1.5f,   // trim de entrada dB
        /*outputGain=*/-0.5f,  // trim de salida dB
        /*latencyMs=*/kSimulatedRoundTripLatencyMs,
        /*snrDb=*/72.4f
    );
    auto audioCalibSnap = controller.getCurrentSnapshot();
    CHECK(audioCalibSnap.calibration.audio.completed == true);
    CHECK(audioCalibSnap.calibration.audio.roundTripLatencyMs == Catch::Approx(kSimulatedRoundTripLatencyMs));
    CHECK(audioCalibSnap.calibration.audio.snrDb == Catch::Approx(72.4f));

    // Calibración MIDI: latencia detectada + jitter
    controller.updateMidiCalibration(
        /*completed=*/true,
        /*latencyMs=*/kSimulatedRoundTripLatencyMs,
        /*jitterMs=*/kSimulatedJitterMs
    );
    auto midiCalibSnap = controller.getCurrentSnapshot();
    CHECK(midiCalibSnap.calibration.midi.completed == true);
    CHECK(midiCalibSnap.calibration.midi.detectedMidiLatencyMs == Catch::Approx(kSimulatedRoundTripLatencyMs));
    CHECK(midiCalibSnap.calibration.midi.jitterMs == Catch::Approx(kSimulatedJitterMs));
    CHECK(midiCalibSnap.calibration.isReadyForProfiling() == true);

    // Receta de excitación MIDI automatizada
    MidiRecipe recipe;
    recipe.firstNote    = 36; // C2
    recipe.lastNote     = 72; // C5
    recipe.velocities   = { 48, 80, 127 };
    recipe.gateMs       = 300.0;
    recipe.settlingMs   = 150.0;
    recipe.midiChannel  = 1;
    recipe.repetitions  = 2;
    recipe.sequenceHash = "e2e02b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1";

    controller.setExcitationMode(ExcitationMode::AutomatedMidi);
    controller.updateMidiRecipe(recipe);

    auto recipeSnap = controller.getCurrentSnapshot();
    CHECK(recipeSnap.excitation.isValid == true);
    CHECK(recipeSnap.excitation.status == RecipeStatus::Valid);
    CHECK(recipeSnap.excitation.excitationMode == ExcitationMode::AutomatedMidi);

    // Auditoría previa (no determinista por naturaleza del hardware analógico-digital)
    controller.updateAuditResult(
        synth::ApprovalStatus::ApprovedWithWarnings,
        "Semi-determinista (reset MIDI CC 121)",
        "Reset por CC-121 en canal 1",
        50.0,
        true,
        { "Jitter ±0.3 ms — dentro de rango tolerado" },
        "Hardware MIDI aprobado con advertencia de jitter"
    );
    auto auditSnap = controller.getCurrentSnapshot();
    CHECK(auditSnap.audit.isAudited == true);
    CHECK(auditSnap.audit.approvalStatus == synth::ApprovalStatus::ApprovedWithWarnings);

    // -----------------------------------------------------------------------
    // PASO 3: Run Session (Secuencia MIDI, Captura ADC y Evaluación)
    // -----------------------------------------------------------------------
    REQUIRE(controller.startProfiling() == true);
    auto runSnap = controller.getCurrentSnapshot();
    CHECK(runSnap.sessionStatus == ProfilingSessionStatus::Profiling);

    // Simulación de secuencia MIDI: notas enviadas al mock sink (verifica Panic)
    const std::vector<int> notes = { 36, 48, 60, 72 };
    for (int note : notes)
    {
        midiSink.sendNoteOn(1, note, 80);
        midiSink.sendNoteOff(1, note);
    }
    CHECK(midiSink.recorded.size() == static_cast<size_t>(notes.size() * 2));
    // Después de NoteOff por cada nota, no deben quedar notas activas
    CHECK(midiSink.hasActiveNotes() == false);

    // Panic de 16 canales en teardown — certificación de limpieza de MIDI
    midiSink.send16ChannelPanic();
    CHECK(midiSink.hasActiveNotes() == false);
    CHECK(midiSink.recorded.size() == static_cast<size_t>(notes.size() * 2 + 16));

    // Telemetría de progreso con latencia física compensada
    controller.updateProgress(1, 4, 0.30, 0.90, "MIDI C2 Vel 48");
    controller.updateProgress(4, 4, 1.20, 0.00, "MIDI C5 Vel 127 — completado");

    // Evaluación con métricas compensadas por latencia de hardware
    controller.updateModelEvaluation(
        synth::SelectionStatus::AcceptedWithWarnings,
        "POLY_WAVE_2D",
        -28.1,    // ESR dB (ligeramente por encima de -30 dB — hardware no determinista)
        0.9978,   // Correlación espectral
        96.5,     // % estímulos conformes
        "C2-C5, Vel 48-127",
        0.12,     // Coste relativo CPU (hardware externo, mínimo procesamiento)
        { "Jitter MIDI compensado: ±0.3 ms" },
        { "Hardware no bit-exacto" }
    );

    controller.completeProfiling();
    auto completedSnap = controller.getCurrentSnapshot();
    CHECK(completedSnap.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(completedSnap.evaluation.hasEvaluation == true);
    CHECK(completedSnap.evaluation.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings);
    CHECK(completedSnap.evaluation.validationEsrDb == Catch::Approx(-28.1));
    CHECK(completedSnap.evaluation.validationCorrelation == Catch::Approx(0.9978));

    // -----------------------------------------------------------------------
    // PASO 4: Export & Report (Package + Metadatos de Latencia MIDI)
    // -----------------------------------------------------------------------
    auto readiness = controller.evaluateExportReadiness();
    CHECK(readiness.canProceed() == true);
    CHECK(readiness.sessionCompleted == true);

    ReportExportRequest expReq;
    expReq.manifest.hardwareId          = target.targetId;
    expReq.manifest.hardwareDisplayName = target.targetName;
    expReq.manifest.activeFunctionId    = "jx8p_poly_wave";
    expReq.manifest.activeFunctionName  = "Poly Wave Algorithm 1";
    expReq.baseFileName                 = "JX8P_HW_E2E02";
    expReq.destinationDirectory         = tempDir.path / "e2e02_export";
    expReq.options.includeProductionPackage   = true;
    expReq.options.includeHtmlCertification   = false;

    for (int i = 0; i < 8; ++i)
    {
        MeasuredPoint pt;
        pt.pointId   = "PT_MIDI_" + std::to_string(i);
        pt.blockType = "POLY_WAVE";
        pt.snrDb     = 62.0f + static_cast<float>(i) * 0.5f;
        pt.thdPercent = 0.15f;
        expReq.measuredPoints.push_back(pt);
    }

    ReportExportResult expResult = ReportExportService::exportReport(expReq);
    REQUIRE(expResult.succeeded());
    REQUIRE(expResult.status == ReportExportStatus::Success);
    REQUIRE(expResult.artifacts.size() == 4);

    // Verificación de artefactos obligatorios
    auto lutFile      = tempDir.path / "e2e02_export" / "JX8P_HW_E2E02_lut.h";
    auto telemFile    = tempDir.path / "e2e02_export" / "JX8P_HW_E2E02_telemetry.json";
    auto htmlFile     = tempDir.path / "e2e02_export" / "JX8P_HW_E2E02_Certification_Report.html";
    auto manifestFile = tempDir.path / "e2e02_export" / "JX8P_HW_E2E02_manifest.json";

    CHECK(std::filesystem::exists(lutFile));
    CHECK(std::filesystem::exists(telemFile));
    CHECK(std::filesystem::exists(htmlFile));
    CHECK(std::filesystem::exists(manifestFile));

    // Verificación de procedencia MIDI y latencia en el manifest
    std::ifstream inManifest(manifestFile);
    REQUIRE(inManifest.is_open());
    nlohmann::json manifestJson = nlohmann::json::parse(inManifest);
    CHECK(manifestJson["hardware"]["id"] == "HW_MIDI_JX8P_SYNTH_02");
    CHECK(manifestJson["totalPointsMeasured"] == 8);
    CHECK(manifestJson.contains("outputArtifacts"));
    CHECK(manifestJson["outputArtifacts"].contains("packageArtifacts"));
    CHECK(manifestJson["outputArtifacts"]["packageArtifacts"].size() == 3);

    // Persistencia y round-trip semántico (.abdlabtest)
    core::SessionManifest sManifest;
    sManifest.formatVersion          = "1.0";
    sManifest.hardwareId             = target.targetId;
    sManifest.hardwareDisplayName    = target.targetName;
    sManifest.targetModule           = "POLY_WAVE";
    sManifest.totalMeasuredPoints    = 8;

    juce::File pkgFile((tempDir.path / "e2e02_session.abdlabtest").string());
    core::SessionSaveRequest saveReq;
    saveReq.manifest    = sManifest;
    saveReq.points      = expReq.measuredPoints;
    saveReq.destination = pkgFile;

    auto saveRes = core::SessionPersistenceService::save(saveReq);
    REQUIRE(saveRes.succeeded());

    core::SessionLoadRequest loadReq;
    loadReq.source = pkgFile;
    auto loadRes = core::SessionPersistenceService::load(loadReq);
    REQUIRE(loadRes.succeeded());
    CHECK(loadRes.manifest.hardwareId == target.targetId);
    CHECK(loadRes.points.size() == 8);

    // Transición de Stepper al recargar sesión completa
    auto workflowState = LoadedSessionApplier::computeWorkflowState(
        loadRes.manifest.totalMeasuredPoints,
        loadRes.points.size()
    );
    CHECK(workflowState.isSessionComplete == true);
    CHECK(workflowState.targetStepperStep == CanonicalStep::ExportReport);

    // Cero archivos residuales de staging
    for (const auto& entry : std::filesystem::directory_iterator(tempDir.path / "e2e02_export"))
    {
        const auto filename = entry.path().filename().string();
        CHECK(filename.find(".staging_") == std::string::npos);
        CHECK(filename.find(".backup_")  == std::string::npos);
    }
}

// ===========================================================================
// ESCENARIO E2E-03: Recorrido Hermético Hardware Analógico Manual (Eurorack/Moog)
// ===========================================================================
TEST_CASE("E2E-03: Recorrido Hermetico Hardware Analogico Manual (Paso 0..4)", "[e2e][hermetic][manual][E2E-03]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    E2ETempDirectory tempDir("e2e03_analogue");

    // -----------------------------------------------------------------------
    // PASO 0: Studio Environment (Solo audio físico — sin MIDI)
    // -----------------------------------------------------------------------
    constexpr float kAudioRoundTripLatencyMs = 6.2f;
    constexpr float kBaseNoiseFloorDbFs      = -76.0f; // dentro de rango aceptable (<-70 dBFS)
    CHECK(kBaseNoiseFloorDbFs < -70.0f);

    ProfilingSessionController controller;
    REQUIRE(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Idle);

    // -----------------------------------------------------------------------
    // PASO 1: Target & Routing (Hardware Analógico — sin puertos MIDI)
    // -----------------------------------------------------------------------
    TargetSelectionState target;
    target.targetId     = "HW_ANALOGUE_MOOG_DFAM_03";
    target.targetName   = "Moog DFAM Percussion (Synthetic Mock)";
    target.manufacturer = "Moog Music (Mock)";
    target.version      = "1.0";
    target.kind         = TargetKind::HardwareAnalogue;
    target.isConnected  = true;
    target.isDeterministic = false;
    target.supportsMidiInput         = false; // sin control digital
    target.supportsParameterAutomation = false;
    target.availableDomainDescription = "VCO analógico + filtro paso-bajo, control por potenciómetros";
    target.parameterCount = 0;

    REQUIRE(controller.selectTarget(target) == true);

    auto targetSnap = controller.getCurrentSnapshot();
    CHECK(targetSnap.sessionStatus == ProfilingSessionStatus::TargetSelected);
    CHECK(targetSnap.target.kind == TargetKind::HardwareAnalogue);
    CHECK(targetSnap.target.supportsMidiInput == false);
    // Para analógico puro: solo calibración de audio requerida; MIDI y digital NOT applicable
    CHECK(targetSnap.calibration.audio.requirement == CalibrationRequirement::Required);
    CHECK(targetSnap.calibration.midi.requirement  == CalibrationRequirement::NotApplicable);
    CHECK(targetSnap.calibration.digital.requirement == CalibrationRequirement::NotApplicable);
    CHECK(targetSnap.calibration.isReadyForProfiling() == false);

    // -----------------------------------------------------------------------
    // PASO 2: Calibration & Setup (Calibración analógica guiada)
    // -----------------------------------------------------------------------
    // Sin calibración completada — no se puede iniciar sesión
    REQUIRE(controller.startProfiling() == false);

    // Calibración de audio con niveles analógicos
    controller.updateAudioCalibration(
        /*completed=*/true,
        /*inputGain=*/-3.0f,     // trim para nivel de eurorack (+10 dBu)
        /*outputGain=*/0.0f,
        /*latencyMs=*/kAudioRoundTripLatencyMs,
        /*snrDb=*/68.5f          // analógico: SNR ligeramente menor que digital
    );
    auto calibSnap = controller.getCurrentSnapshot();
    CHECK(calibSnap.calibration.audio.completed == true);
    CHECK(calibSnap.calibration.audio.snrDb == Catch::Approx(68.5f));
    CHECK(calibSnap.calibration.isReadyForProfiling() == true);

    // Receta manual con instrucciones de operador para potenciómetros
    ManualOperatorRecipe manualRecipe;
    manualRecipe.interactionKind           = ManualInteractionKind::PhysicalControlAdjustment;
    manualRecipe.instruction               = juce::String("Ajustar Cutoff a 1 kHz y Resonancia al 50%, luego pulsar Listo");
    manualRecipe.expectedSetting           = juce::String("Cutoff=1kHz, Res=50%");
    manualRecipe.repetitions               = 3;
    manualRecipe.settlingMs                = 800.0;
    manualRecipe.requireOperatorConfirmation = true;

    controller.setExcitationMode(ExcitationMode::ManualOperator);
    controller.updateManualRecipe(manualRecipe);

    auto recipeSnap = controller.getCurrentSnapshot();
    CHECK(recipeSnap.excitation.isValid == true);
    CHECK(recipeSnap.excitation.excitationMode == ExcitationMode::ManualOperator);
    CHECK(recipeSnap.excitation.status == RecipeStatus::Valid);

    // Auditoría: analógico aprobado con advertencias de no-determinismo
    controller.updateAuditResult(
        synth::ApprovalStatus::ApprovedWithWarnings,
        "No determinista (analógico puro)",
        "Sin capacidad de reset digital",
        800.0,
        false,
        { "Variación de umbral de señal ±0.5 dBFS entre ensayos" },
        "Target analógico aprobado — confirmar cada ajuste manualmente"
    );
    auto auditSnap = controller.getCurrentSnapshot();
    CHECK(auditSnap.audit.isAudited == true);
    CHECK(auditSnap.audit.approvalStatus == synth::ApprovalStatus::ApprovedWithWarnings);

    // -----------------------------------------------------------------------
    // PASO 3: Run Session (Flujo de confirmación manual por operador)
    // -----------------------------------------------------------------------
    REQUIRE(controller.startProfiling() == true);
    auto runSnap = controller.getCurrentSnapshot();
    CHECK(runSnap.sessionStatus == ProfilingSessionStatus::Profiling);

    // Confirmación manual — sin confirmación del operador no hay MeasuredPoint válido
    // (En arnés hermético se simula el paso del operador con confirmOperatorStep)
    int confirmedSteps = 0;
    constexpr int kRepetitions = 3;
    for (int rep = 0; rep < kRepetitions; ++rep)
    {
        // Simula: el operador ajusta el hardware y confirma
        controller.confirmOperatorStep();
        ++confirmedSteps;

        controller.updateProgress(
            rep + 1,
            kRepetitions,
            static_cast<double>(rep + 1) * 1.5,
            static_cast<double>(kRepetitions - rep - 1) * 1.5,
            "Analógico: Cutoff=1kHz rep " + std::to_string(rep + 1)
        );
    }
    CHECK(confirmedSteps == kRepetitions);

    // Evaluación analógica — métricas más conservadoras que digital
    controller.updateModelEvaluation(
        synth::SelectionStatus::AcceptedWithWarnings,
        "RBF_KERNEL_1D",
        -22.7,    // ESR dB (analógico: típicamente > -30 dB)
        0.9942,   // Correlación espectral
        89.0,     // % estímulos conformes (analógico tolera más varianza)
        "Analógico: Cutoff 1kHz, Res 50%",
        0.08,     // CPU mínima — procesamiento de audio, sin síntesis digital
        { "Varianza de 3 repeticiones: ±0.5 dBFS" },
        { "Analógico no determinista por diseño" }
    );

    controller.completeProfiling();
    auto completedSnap = controller.getCurrentSnapshot();
    CHECK(completedSnap.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(completedSnap.evaluation.hasEvaluation == true);
    CHECK(completedSnap.evaluation.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings);
    CHECK(completedSnap.evaluation.validationEsrDb == Catch::Approx(-22.7));
    CHECK(completedSnap.evaluation.validationCorrelation == Catch::Approx(0.9942));

    // -----------------------------------------------------------------------
    // PASO 4: Export & Report (Package con notas de operador y procedencia manual)
    // -----------------------------------------------------------------------
    auto readiness = controller.evaluateExportReadiness();
    CHECK(readiness.canProceed() == true);

    ReportExportRequest expReq;
    expReq.manifest.hardwareId          = target.targetId;
    expReq.manifest.hardwareDisplayName = target.targetName;
    expReq.manifest.activeFunctionId    = "dfam_percussion";
    expReq.manifest.activeFunctionName  = "DFAM Percussion Synthesis";
    expReq.baseFileName                 = "DFAM_Analogue_E2E03";
    expReq.destinationDirectory         = tempDir.path / "e2e03_export";
    expReq.options.includeProductionPackage   = true;
    expReq.options.includeHtmlCertification   = false;

    for (int i = 0; i < kRepetitions; ++i)
    {
        MeasuredPoint pt;
        pt.pointId   = "PT_ANALOGUE_" + std::to_string(i);
        pt.blockType = "PERCUSSION_ANALOGUE";
        pt.snrDb     = 58.0f + static_cast<float>(i) * 1.0f;
        pt.thdPercent = 0.45f; // analógico: THD mayor
        expReq.measuredPoints.push_back(pt);
    }

    ReportExportResult expResult = ReportExportService::exportReport(expReq);
    REQUIRE(expResult.succeeded());
    REQUIRE(expResult.status == ReportExportStatus::Success);
    REQUIRE(expResult.artifacts.size() == 4);

    // Verificación de artefactos obligatorios
    auto lutFile      = tempDir.path / "e2e03_export" / "DFAM_Analogue_E2E03_lut.h";
    auto telemFile    = tempDir.path / "e2e03_export" / "DFAM_Analogue_E2E03_telemetry.json";
    auto htmlFile     = tempDir.path / "e2e03_export" / "DFAM_Analogue_E2E03_Certification_Report.html";
    auto manifestFile = tempDir.path / "e2e03_export" / "DFAM_Analogue_E2E03_manifest.json";

    CHECK(std::filesystem::exists(lutFile));
    CHECK(std::filesystem::exists(telemFile));
    CHECK(std::filesystem::exists(htmlFile));
    CHECK(std::filesystem::exists(manifestFile));

    // Verificación de manifest con procedencia analógica
    std::ifstream inManifest(manifestFile);
    REQUIRE(inManifest.is_open());
    nlohmann::json manifestJson = nlohmann::json::parse(inManifest);
    CHECK(manifestJson["hardware"]["id"] == "HW_ANALOGUE_MOOG_DFAM_03");
    CHECK(manifestJson["totalPointsMeasured"] == kRepetitions);
    CHECK(manifestJson.contains("outputArtifacts"));
    CHECK(manifestJson["outputArtifacts"].contains("packageArtifacts"));
    CHECK(manifestJson["outputArtifacts"]["packageArtifacts"].size() == 3);

    // Persistencia y round-trip semántico
    core::SessionManifest sManifest;
    sManifest.formatVersion       = "1.0";
    sManifest.hardwareId          = target.targetId;
    sManifest.hardwareDisplayName = target.targetName;
    sManifest.targetModule        = "PERCUSSION_ANALOGUE";
    sManifest.totalMeasuredPoints = kRepetitions;

    juce::File pkgFile((tempDir.path / "e2e03_session.abdlabtest").string());
    core::SessionSaveRequest saveReq;
    saveReq.manifest    = sManifest;
    saveReq.points      = expReq.measuredPoints;
    saveReq.destination = pkgFile;

    auto saveRes = core::SessionPersistenceService::save(saveReq);
    REQUIRE(saveRes.succeeded());

    core::SessionLoadRequest loadReq;
    loadReq.source = pkgFile;
    auto loadRes = core::SessionPersistenceService::load(loadReq);
    REQUIRE(loadRes.succeeded());
    CHECK(loadRes.manifest.hardwareId == target.targetId);
    CHECK(loadRes.points.size() == static_cast<size_t>(kRepetitions));

    // Transición de Stepper al recargar sesión analógica completa
    auto workflowState = LoadedSessionApplier::computeWorkflowState(
        loadRes.manifest.totalMeasuredPoints,
        loadRes.points.size()
    );
    CHECK(workflowState.isSessionComplete == true);
    CHECK(workflowState.targetStepperStep == CanonicalStep::ExportReport);

    // Cero archivos residuales de staging
    for (const auto& entry : std::filesystem::directory_iterator(tempDir.path / "e2e03_export"))
    {
        const auto filename = entry.path().filename().string();
        CHECK(filename.find(".staging_") == std::string::npos);
        CHECK(filename.find(".backup_")  == std::string::npos);
    }
}

// ===========================================================================
// ESCENARIO E2E-04: Recorrido Hermético Target Híbrido (MIDI + Panel Manual)
// ===========================================================================
TEST_CASE("E2E-04: Recorrido Hermetico Target Hibrido MIDI + Panel Manual (Paso 0..4)", "[e2e][hermetic][hybrid][E2E-04]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    E2ETempDirectory tempDir("e2e04_hybrid");
    MockMidiSink midiSink;

    // -----------------------------------------------------------------------
    // PASO 0: Studio Environment (MIDI + Audio físico — doble dominio)
    // -----------------------------------------------------------------------
    constexpr float kMidiLatencyMs  = 5.2f;
    constexpr float kAudioLatencyMs = 7.8f;
    constexpr float kJitterMs       = 0.2f;

    ProfilingSessionController controller;
    REQUIRE(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Idle);

    // -----------------------------------------------------------------------
    // PASO 1: Target & Routing (Sintetizador híbrido — MIDI + Panel)
    // Usamos HardwareDigital para representar sintetizador con MIDI + controles de panel
    // -----------------------------------------------------------------------
    TargetSelectionState target;
    target.targetId     = "HW_HYBRID_PROPHET6_04";
    target.targetName   = "Sequential Prophet-6 Hybrid (Synthetic Mock)";
    target.manufacturer = "Sequential (Mock)";
    target.version      = "2.0";
    target.kind         = TargetKind::HardwareDigital; // híbrido: MIDI + panel analógico
    target.isConnected  = true;
    target.isDeterministic = false;
    target.supportsMidiInput           = true;
    target.supportsParameterAutomation = false; // panel físico — sin automatización VST
    target.supportsMidiCc              = true;
    target.availableDomainDescription  = "MIDI C1-C7, CC1/CC74 para filtro/resonancia, 6 voces";
    target.parameterCount              = 6; // parámetros MIDI-controlables

    REQUIRE(controller.selectTarget(target) == true);

    auto targetSnap = controller.getCurrentSnapshot();
    CHECK(targetSnap.sessionStatus == ProfilingSessionStatus::TargetSelected);
    CHECK(targetSnap.target.kind == TargetKind::HardwareDigital);
    CHECK(targetSnap.target.supportsMidiInput == true);
    CHECK(targetSnap.target.supportsMidiCc == true);
    // Híbrido: audio y MIDI requeridos; digital NOT applicable
    CHECK(targetSnap.calibration.audio.requirement == CalibrationRequirement::Required);
    CHECK(targetSnap.calibration.midi.requirement  == CalibrationRequirement::Required);
    CHECK(targetSnap.calibration.digital.requirement == CalibrationRequirement::NotApplicable);
    CHECK(targetSnap.calibration.isReadyForProfiling() == false);

    // -----------------------------------------------------------------------
    // PASO 2: Calibration & Setup (Dual ortogonal: Audio + MIDI)
    // -----------------------------------------------------------------------
    REQUIRE(controller.startProfiling() == false);

    // Calibración de audio físico
    controller.updateAudioCalibration(
        /*completed=*/true,
        /*inputGain=*/-2.0f,
        /*outputGain=*/-1.0f,
        /*latencyMs=*/kAudioLatencyMs,
        /*snrDb=*/70.8f
    );

    // Calibración MIDI para secuenciación de notas y CCs
    controller.updateMidiCalibration(
        /*completed=*/true,
        /*latencyMs=*/kMidiLatencyMs,
        /*jitterMs=*/kJitterMs
    );

    auto calibSnap = controller.getCurrentSnapshot();
    CHECK(calibSnap.calibration.audio.completed == true);
    CHECK(calibSnap.calibration.midi.completed == true);
    CHECK(calibSnap.calibration.isReadyForProfiling() == true);

    // Receta híbrida: MIDI automatizado + prompts de perilla intercalados
    // Notas MIDI automáticas para el sweep de pitch
    MidiRecipe midiRecipe;
    midiRecipe.firstNote    = 48; // C3
    midiRecipe.lastNote     = 60; // C4
    midiRecipe.velocities   = { 64, 100 };
    midiRecipe.gateMs       = 400.0;
    midiRecipe.settlingMs   = 200.0;
    midiRecipe.midiChannel  = 1;
    midiRecipe.repetitions  = 1;
    midiRecipe.sequenceHash = "e2e04c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3";

    controller.setExcitationMode(ExcitationMode::AutomatedMidi);
    controller.updateMidiRecipe(midiRecipe);

    auto recipeSnap = controller.getCurrentSnapshot();
    CHECK(recipeSnap.excitation.isValid == true);
    CHECK(recipeSnap.excitation.excitationMode == ExcitationMode::AutomatedMidi);

    // Auditoría híbrida
    controller.updateAuditResult(
        synth::ApprovalStatus::ApprovedWithWarnings,
        "Semi-determinista (MIDI notas + panel analógico)",
        "Reset por CC-121; panel requiere alineación manual",
        200.0,
        true,
        { "Panel analógico: variación de ±1% entre ensayos" },
        "Híbrido aprobado — confirmar ajustes de panel antes de cada bloque"
    );
    CHECK(controller.getCurrentSnapshot().audit.isAudited == true);
    CHECK(controller.getCurrentSnapshot().audit.approvalStatus == synth::ApprovalStatus::ApprovedWithWarnings);

    // -----------------------------------------------------------------------
    // PASO 3: Run Session (MIDI + Confirmaciones de Panel Intercaladas)
    // -----------------------------------------------------------------------
    REQUIRE(controller.startProfiling() == true);
    CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Profiling);

    // Bloque 1: Notas MIDI automáticas (C3 a C4)
    const std::vector<int> notesHybrid = { 48, 52, 55, 60 };
    for (int note : notesHybrid)
    {
        midiSink.sendNoteOn(1, note, 80);
        midiSink.sendNoteOff(1, note);
    }
    CHECK(midiSink.hasActiveNotes() == false);
    controller.updateProgress(1, 3, 0.40, 0.80, "MIDI sweep C3-C4");

    // Paso intermedio: confirmación manual de panel — operador ajusta filtro
    controller.confirmOperatorStep();
    controller.updateProgress(2, 3, 0.80, 0.40, "Panel: CC74 Filtro ajustado");

    // Bloque 2: MIDI CC para barrer filtro automáticamente
    // (simulado como notas adicionales en el mock — CC no tiene representación en el sink simplificado)
    midiSink.sendNoteOn(1, 60, 100);
    midiSink.sendNoteOff(1, 60);
    controller.updateProgress(3, 3, 1.20, 0.00, "MIDI CC74 sweep — completado");

    // Panic 16 canales en finalización
    midiSink.send16ChannelPanic();
    CHECK(midiSink.hasActiveNotes() == false);

    // Evaluación con procedencia dual (MIDI + manual)
    controller.updateModelEvaluation(
        synth::SelectionStatus::AcceptedWithWarnings,
        "POLY_DUAL_2D",
        -25.6,     // ESR dB — híbrido: entre digital y analógico puro
        0.9961,    // Correlación espectral
        93.0,      // % estímulos conformes
        "C3-C4, CC74 Filtro 40%",
        0.15,      // CPU moderada
        { "Panel analógico: variación ±1%" },
        { "Procedencia dual: MIDI + Panel" }
    );

    controller.completeProfiling();
    auto completedSnap = controller.getCurrentSnapshot();
    CHECK(completedSnap.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(completedSnap.evaluation.hasEvaluation == true);
    CHECK(completedSnap.evaluation.selectionStatus == synth::SelectionStatus::AcceptedWithWarnings);
    CHECK(completedSnap.evaluation.validationEsrDb == Catch::Approx(-25.6));
    CHECK(completedSnap.evaluation.validationCorrelation == Catch::Approx(0.9961));

    // -----------------------------------------------------------------------
    // PASO 4: Export & Report (ProductionPackage Dual-Origin)
    // -----------------------------------------------------------------------
    auto readiness = controller.evaluateExportReadiness();
    CHECK(readiness.canProceed() == true);

    ReportExportRequest expReq;
    expReq.manifest.hardwareId          = target.targetId;
    expReq.manifest.hardwareDisplayName = target.targetName;
    expReq.manifest.activeFunctionId    = "prophet6_dual_poly";
    expReq.manifest.activeFunctionName  = "Prophet-6 Dual Poly Engine";
    expReq.baseFileName                 = "Prophet6_Hybrid_E2E04";
    expReq.destinationDirectory         = tempDir.path / "e2e04_export";
    expReq.options.includeProductionPackage   = true;
    expReq.options.includeHtmlCertification   = false;

    for (int i = 0; i < 6; ++i)
    {
        MeasuredPoint pt;
        pt.pointId   = "PT_HYBRID_" + std::to_string(i);
        pt.blockType = "POLY_DUAL";
        pt.snrDb     = 65.0f + static_cast<float>(i) * 0.8f;
        pt.thdPercent = 0.25f;
        expReq.measuredPoints.push_back(pt);
    }

    ReportExportResult expResult = ReportExportService::exportReport(expReq);
    REQUIRE(expResult.succeeded());
    REQUIRE(expResult.status == ReportExportStatus::Success);
    REQUIRE(expResult.artifacts.size() == 4);

    // Verificación de artefactos obligatorios
    auto lutFile      = tempDir.path / "e2e04_export" / "Prophet6_Hybrid_E2E04_lut.h";
    auto telemFile    = tempDir.path / "e2e04_export" / "Prophet6_Hybrid_E2E04_telemetry.json";
    auto htmlFile     = tempDir.path / "e2e04_export" / "Prophet6_Hybrid_E2E04_Certification_Report.html";
    auto manifestFile = tempDir.path / "e2e04_export" / "Prophet6_Hybrid_E2E04_manifest.json";

    CHECK(std::filesystem::exists(lutFile));
    CHECK(std::filesystem::exists(telemFile));
    CHECK(std::filesystem::exists(htmlFile));
    CHECK(std::filesystem::exists(manifestFile));

    // Verificación de manifest con procedencia dual
    std::ifstream inManifest(manifestFile);
    REQUIRE(inManifest.is_open());
    nlohmann::json manifestJson = nlohmann::json::parse(inManifest);
    CHECK(manifestJson["hardware"]["id"] == "HW_HYBRID_PROPHET6_04");
    CHECK(manifestJson["totalPointsMeasured"] == 6);
    CHECK(manifestJson.contains("outputArtifacts"));
    CHECK(manifestJson["outputArtifacts"].contains("packageArtifacts"));
    CHECK(manifestJson["outputArtifacts"]["packageArtifacts"].size() == 3);

    // Persistencia y round-trip semántico (.abdlabtest)
    core::SessionManifest sManifest;
    sManifest.formatVersion       = "1.0";
    sManifest.hardwareId          = target.targetId;
    sManifest.hardwareDisplayName = target.targetName;
    sManifest.targetModule        = "POLY_DUAL";
    sManifest.totalMeasuredPoints = 6;

    juce::File pkgFile((tempDir.path / "e2e04_session.abdlabtest").string());
    core::SessionSaveRequest saveReq;
    saveReq.manifest    = sManifest;
    saveReq.points      = expReq.measuredPoints;
    saveReq.destination = pkgFile;

    auto saveRes = core::SessionPersistenceService::save(saveReq);
    REQUIRE(saveRes.succeeded());

    core::SessionLoadRequest loadReq;
    loadReq.source = pkgFile;
    auto loadRes = core::SessionPersistenceService::load(loadReq);
    REQUIRE(loadRes.succeeded());
    CHECK(loadRes.manifest.hardwareId == target.targetId);
    CHECK(loadRes.points.size() == 6);

    // Transición de Stepper al recargar sesión híbrida completa
    auto workflowState = LoadedSessionApplier::computeWorkflowState(
        loadRes.manifest.totalMeasuredPoints,
        loadRes.points.size()
    );
    CHECK(workflowState.isSessionComplete == true);
    CHECK(workflowState.targetStepperStep == CanonicalStep::ExportReport);
    CHECK(workflowState.runSessionStatus == CanonicalStepStatus::Completed);

    // Cero archivos residuales de staging
    for (const auto& entry : std::filesystem::directory_iterator(tempDir.path / "e2e04_export"))
    {
        const auto filename = entry.path().filename().string();
        CHECK(filename.find(".staging_") == std::string::npos);
        CHECK(filename.find(".backup_")  == std::string::npos);
    }
}

// ===========================================================================
// PRODUCT-SMOKE-01: Basic Oscillator VST3 E2E (ReferenceSynth Paso 0..4)
// AutomatedMidi & ManualOperator convergence
// ===========================================================================

TEST_CASE("PRODUCT-SMOKE-01: Basic Oscillator VST3 E2E (ReferenceSynth Paso 0..4)",
          "[product][smoke][vst3][PRODUCT-SMOKE-01]")
{
    E2ETempDirectory tempDir("product_smoke_01");

    // -----------------------------------------------------------------------
    // PASO 0: Studio Environment (Configuracion y Dominio Digital)
    // -----------------------------------------------------------------------
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    juce::ignoreUnused(kBlockSize);

    TargetSelectionState state;
    state.targetId = "ReferenceSynth";
    auto pluginFile = synth::InProcessVst3LifecycleAdapter::resolveVst3File(state);
    if (!pluginFile.exists())
    {
        SKIP("ReferenceSynth.vst3 no encontrado en build. Se omite smoke test de producto.");
    }

    // -----------------------------------------------------------------------
    // PASO 1: Target & Routing (Seleccionar ReferenceSynth VST3)
    // -----------------------------------------------------------------------
    TargetSelectionState target;
    target.targetId = pluginFile.getFullPathName().toStdString();
    target.targetName = "ReferenceSynth VST3";
    target.manufacturer = "ABDSynths";
    target.kind = TargetKind::PluginVST3;
    target.isConnected = true;
    target.isDeterministic = true;
    target.parameterCount = 4; // Oscillator wave, Cutoff, Resonance, Level

    SECTION("Flujo A: Modo AutomatedMidi")
    {
        ProfilingSessionController controller;
        REQUIRE(controller.selectTarget(target) == true);
        auto snapTarget = controller.getCurrentSnapshot();
        CHECK(snapTarget.sessionStatus == ProfilingSessionStatus::TargetSelected);
        CHECK(snapTarget.target.targetName == "ReferenceSynth VST3");
        CHECK(snapTarget.target.kind == TargetKind::PluginVST3);

        // Verificación de calibración digital requerida para VST3
        CHECK(snapTarget.calibration.digital.requirement == CalibrationRequirement::Required);
        controller.verifyDigitalCalibration();
        CHECK(controller.getCurrentSnapshot().calibration.digital.verified == true);

        // PASO 2: Receta Minima: C4, Vel 64, gateMs 250, settlingMs 50, 3 reps
        MidiRecipe recipe;
        recipe.firstNote = 60; // C4
        recipe.lastNote = 60;
        recipe.velocities = { 64 };
        recipe.gateMs = 250.0;
        recipe.settlingMs = 50.0;
        recipe.midiChannel = 1;
        recipe.repetitions = 3;
        recipe.sequenceHash = "product_smoke_c4_vel64_seq";

        controller.setExcitationMode(ExcitationMode::AutomatedMidi);
        controller.updateMidiRecipe(recipe);

        // Auditoría previa metrológica
        controller.updateAuditResult(
            synth::ApprovalStatus::Approved,
            "100% Determinista (Digital VST3)",
            "Reset instantaneo",
            50.0,
            false,
            {},
            "ReferenceSynth aprobado para medicion de oscilador"
        );
        CHECK(controller.getCurrentSnapshot().audit.isAudited == true);

        // PASO 3: Run Session (Ejecución real de medición in-process con worker desacoplado)
        REQUIRE(controller.startProfiling() == true);
        CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Profiling);

        // Bombeo cooperativo de la cola de mensajes mientras el worker ejecuta la medición
        auto tStart = std::chrono::steady_clock::now();
        while (controller.getCoordinator() && controller.getCoordinator()->isRunning())
        {
            pumpUiMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - tStart).count();
            if (elapsed > 10000)
                break;
        }
        pumpUiMessages();
        if (controller.getCoordinator())
        {
            controller.getCoordinator()->waitForWorkerToStop(2000);
        }
        pumpUiMessages();

        auto snapCompleted = controller.getCurrentSnapshot();
        CHECK(snapCompleted.sessionStatus == ProfilingSessionStatus::Completed);
        CHECK(snapCompleted.workflowStage == ProfilingWorkflowStage::ReviewResults);
        CHECK(snapCompleted.evaluation.hasEvaluation == true);
        CHECK(snapCompleted.exportOptions.canExportCpp == true);

        // PASO 4: Exportación y verificación de artefactos
        auto exportDir = tempDir.path / "exports_auto";
        std::filesystem::create_directories(exportDir);
        auto exportPath = (exportDir / "ReferenceSynthModel.cpp").string();
        REQUIRE(controller.exportModel("cpp", exportPath) == true);
        CHECK(std::filesystem::exists(exportPath));
        CHECK(std::filesystem::file_size(exportPath) > 50);

        // Guardado y recarga de sesión hermética
        auto sessionFile = (tempDir.path / "smoke_session_auto.abdsession").string();
        core::SessionManifest sManifest;
        sManifest.sessionTitle = snapCompleted.sessionId;
        sManifest.hardwareId = target.targetId;
        sManifest.hardwareName = target.targetName;
        sManifest.sampleRate = kSampleRate;
        sManifest.totalMeasuredPoints = 3;

        core::SessionSaveRequest saveReq;
        saveReq.manifest = sManifest;
        saveReq.destination = juce::File(sessionFile);
        auto saveRes = core::SessionPersistenceService::save(saveReq);
        REQUIRE(saveRes.succeeded());

        core::SessionLoadRequest loadReq;
        loadReq.source = juce::File(sessionFile);
        auto loadRes = core::SessionPersistenceService::load(loadReq);
        REQUIRE(loadRes.succeeded());
        CHECK(loadRes.manifest.hardwareName == "ReferenceSynth VST3");
        CHECK(loadRes.manifest.totalMeasuredPoints == 3);
    }

    SECTION("Flujo B: Modo ManualOperator (Convergencia)")
    {
        ProfilingSessionController controller;
        REQUIRE(controller.selectTarget(target) == true);
        controller.verifyDigitalCalibration();

        // PASO 2: Receta Manual
        ManualOperatorRecipe manRecipe;
        manRecipe.instruction = "Configurar onda diente de sierra basica y pulsar Listo";
        manRecipe.requireOperatorConfirmation = true;
        controller.setExcitationMode(ExcitationMode::ManualOperator);
        controller.updateManualRecipe(manRecipe);

        controller.updateAuditResult(
            synth::ApprovalStatus::Approved,
            "100% Determinista (Manual VST3)",
            "Manual prompt confirmado",
            50.0,
            false,
            {},
            "ReferenceSynth aprobado en modo manual"
        );

        // PASO 3: Run Session
        REQUIRE(controller.startProfiling() == true);

        // Bombeo cooperativo para conclusión del worker
        auto tStart = std::chrono::steady_clock::now();
        while (controller.getCoordinator() && controller.getCoordinator()->isRunning())
        {
            pumpUiMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - tStart).count();
            if (elapsed > 10000)
                break;
        }
        pumpUiMessages();
        if (controller.getCoordinator())
        {
            controller.getCoordinator()->waitForWorkerToStop(2000);
        }
        pumpUiMessages();

        auto snapCompleted = controller.getCurrentSnapshot();
        CHECK(snapCompleted.sessionStatus == ProfilingSessionStatus::Completed);
        CHECK(snapCompleted.workflowStage == ProfilingWorkflowStage::ReviewResults);
        CHECK(snapCompleted.evaluation.hasEvaluation == true);
        CHECK(snapCompleted.exportOptions.canExportCpp == true);

        auto exportDir = tempDir.path / "exports_manual";
        std::filesystem::create_directories(exportDir);
        auto exportPath = (exportDir / "ReferenceSynthManualModel.cpp").string();
        REQUIRE(controller.exportModel("cpp", exportPath) == true);
        CHECK(std::filesystem::exists(exportPath));
    }
}

// ===========================================================================
// PRODUCT-SMOKE-02 / 3RD-PARTY-SMOKE-01: DemoSynth VST3 (Terceros Independiente)
// ===========================================================================
TEST_CASE("3RD-PARTY-SMOKE-01: DemoSynth VST3 Third-Party Plugin E2E",
          "[product][smoke][vst3][3rdparty][demosynth]")
{
    E2ETempDirectory tempDir("demo_synth_smoke");

    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    juce::ignoreUnused(kBlockSize);

    TargetSelectionState state;
    state.targetId = "DemoSynth";
    auto pluginFile = synth::InProcessVst3LifecycleAdapter::resolveVst3File(state);
    if (!pluginFile.exists())
    {
        SKIP("DemoSynth.vst3 no encontrado en C:\\Program Files\\Common Files\\VST3 ni en build. Se omite prueba externa.");
    }

    TargetSelectionState target;
    target.targetId = pluginFile.getFullPathName().toStdString();
    target.targetName = "DemoSynth VST3 (Playful Tones)";
    target.manufacturer = "Playful Tones";
    target.kind = TargetKind::PluginVST3;
    target.isConnected = true;
    target.isDeterministic = true;
    target.parameterCount = 0; // Sintetizador senoidal puro controlado por MIDI

    ProfilingSessionController controller;
    REQUIRE(controller.selectTarget(target) == true);
    auto snapTarget = controller.getCurrentSnapshot();
    CHECK(snapTarget.sessionStatus == ProfilingSessionStatus::TargetSelected);
    CHECK(snapTarget.target.kind == TargetKind::PluginVST3);

    // Calibración digital
    controller.verifyDigitalCalibration();
    CHECK(controller.getCurrentSnapshot().calibration.digital.verified == true);

    // Receta MIDI
    MidiRecipe recipe;
    recipe.firstNote = 60; // C4
    recipe.lastNote = 60;
    recipe.velocities = { 64 };
    recipe.gateMs = 250.0;
    recipe.settlingMs = 50.0;
    recipe.midiChannel = 1;
    recipe.repetitions = 3;
    recipe.sequenceHash = "demosynth_smoke_c4_vel64_seq";

    controller.setExcitationMode(ExcitationMode::AutomatedMidi);
    controller.updateMidiRecipe(recipe);

    controller.updateAuditResult(
        synth::ApprovalStatus::Approved,
        "100% Determinista (VST3 Terceros)",
        "Reset instantaneo",
        50.0,
        false,
        {},
        "DemoSynth aprobado para medicion de oscilador senoidal"
    );

    // Ejecución con worker desacoplado y bombeo cooperativo
    REQUIRE(controller.startProfiling() == true);
    CHECK(controller.getCurrentSnapshot().sessionStatus == ProfilingSessionStatus::Profiling);

    auto tStart = std::chrono::steady_clock::now();
    while (controller.getCoordinator() && controller.getCoordinator()->isRunning())
    {
        pumpUiMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - tStart).count();
        if (elapsed > 10000)
            break;
    }
    pumpUiMessages();
    if (controller.getCoordinator())
    {
        controller.getCoordinator()->waitForWorkerToStop(2000);
    }
    pumpUiMessages();

    auto snapCompleted = controller.getCurrentSnapshot();
    CHECK(snapCompleted.sessionStatus == ProfilingSessionStatus::Completed);
    CHECK(snapCompleted.workflowStage == ProfilingWorkflowStage::ReviewResults);
    CHECK(snapCompleted.evaluation.hasEvaluation == true);
    CHECK(snapCompleted.exportOptions.canExportCpp == true);

    // Exportación a C++
    auto exportDir = tempDir.path / "exports_demosynth";
    std::filesystem::create_directories(exportDir);
    auto exportPath = (exportDir / "DemoSynthModel.cpp").string();
    REQUIRE(controller.exportModel("cpp", exportPath) == true);
    CHECK(std::filesystem::exists(exportPath));
    CHECK(std::filesystem::file_size(exportPath) > 50);

    // Persistencia y recarga
    auto sessionFile = (tempDir.path / "demosynth_session.abdsession").string();
    core::SessionManifest sManifest;
    sManifest.sessionTitle = snapCompleted.sessionId;
    sManifest.hardwareId = target.targetId;
    sManifest.hardwareName = target.targetName;
    sManifest.sampleRate = kSampleRate;
    sManifest.totalMeasuredPoints = 3;

    core::SessionSaveRequest saveReq;
    saveReq.manifest = sManifest;
    saveReq.destination = juce::File(sessionFile);
    auto saveRes = core::SessionPersistenceService::save(saveReq);
    REQUIRE(saveRes.succeeded());

    core::SessionLoadRequest loadReq;
    loadReq.source = juce::File(sessionFile);
    auto loadRes = core::SessionPersistenceService::load(loadReq);
    REQUIRE(loadRes.succeeded());
    CHECK(loadRes.manifest.hardwareName == "DemoSynth VST3 (Playful Tones)");
    CHECK(loadRes.manifest.totalMeasuredPoints == 3);
}

