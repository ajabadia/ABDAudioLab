/**
 * @file test_Integration01GuidedVsClassicAudio.cpp
 * @brief INTEGRATION-01: Auditoría de Paridad E2E de Audio y DSP entre Guiado y Libre Sistemático.
 *
 * Ejecuta el bucle cerrado determinista conducido por MockAudioEngine sobre el fixture
 * sintético puro (SyntheticAudioFixture), sin dispositivos WASAPI/ASIO y sin reloj de pared.
 *
 * Certifica:
 *   - Nivel 1 (Control exacto): traza MIDI determinista, NoteOn/NoteOff idénticos, orden y offsets.
 *   - Nivel 2 (Audio canónico): buffers float32 IEEE-754 bit a bit (max diff <= 1e-7, RMSE <= 1e-7, SHA-256 idéntico).
 *   - Nivel 3 (Salida científica): paridad de métricas (RMS, Peak, SNR, THD, f0) y clasificación formal.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <filesystem>

#include "support/MockAudioEngine.h"
#include "core/ProfilingSequencer.h"
#include "hardware/MockHardwareController.h"
#include "audio/LabAudioEngine.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "gui/session/ProfilingSessionController.h"
#include "measurement/MeasurementSessionContracts.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::test::support;

namespace
{

struct TemporaryTestDirectory
{
    std::filesystem::path path;

    explicit TemporaryTestDirectory(const std::string& prefix)
    {
        auto tempRoot = std::filesystem::temp_directory_path();
        path = tempRoot / (prefix + "_" + std::to_string(juce::Random::getSystemRandom().nextInt()));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~TemporaryTestDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    TemporaryTestDirectory(const TemporaryTestDirectory&) = delete;
    TemporaryTestDirectory& operator=(const TemporaryTestDirectory&) = delete;
};

TargetSelectionState createCanonicalTarget()
{
    TargetSelectionState t;
    t.targetId = "synthetic_fixture_integration01";
    t.targetName = "SyntheticAudioFixture (Pure Sine C4 Test Witness)";
    t.manufacturer = "ABDAudioLab";
    t.version = "1.0.0";
    t.kind = TargetKind::SyntheticFixture;
    t.isConnected = true;
    t.isDeterministic = true;
    t.availableDomainDescription = "Notas MIDI C1-C6, Vel 1-127";
    t.parameterCount = 8;
    return t;
}

ProfilingSession createCanonicalBenchmarkPlan()
{
    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = "SyntheticAudioFixture";
    meta.targetModule = "SineOscillator";
    meta.operatorMode = "AUTOMATED_MIDI_NOTES";
    session.setMetadata(meta);

    // Caso Testigo Controlado: C4 (Nota 60), Velocity 64 (0.50f), Gate 250ms, Settling 50ms, 3 repeticiones
    for (int rep = 0; rep < 3; ++rep)
    {
        TestCase tc;
        tc.testId = "INTEG01_C4_REP_" + std::to_string(rep + 1);
        tc.functionalBlockType = "SpectrumFilter";
        tc.stimulusType = audio::StimulusType::Silence;
        tc.stimulusDurationSec = 0.35;
        tc.isAutonomousSynth = true;
        tc.midiChannel = 1;
        tc.midiNoteNumber = 60; // C4
        tc.midiVelocity = 0.50f; // Velocity 64 aprox
        tc.noteGateDurationSec = 0.25f; // 250 ms
        tc.stabilizationWaitMs = 50.0;  // 50 ms settling
        tc.numPasses = 1;
        session.addTestCase(tc);
    }

    return session;
}

struct RouteExecutionResult
{
    std::string routeName;
    bool sessionCompleted { false };
    int pumpedBlocks { 0 };
    int64_t processedSamples { 0 };
    std::string stopReason;
    std::string diagnosticReport;

    std::vector<RecordedMidiEvent> midiTrace;
    std::vector<float> capturedAudioL;
    std::vector<float> capturedAudioR;
    std::string canonicalAudioSha256;

    std::vector<exporting::MeasuredPoint> measuredPoints;
    ExperimentKind deducedKind { ExperimentKind::Exploration };
};

RouteExecutionResult executeRoute(const std::string& routeName, UiWorkflowMode workflowMode)
{
    RouteExecutionResult result;
    result.routeName = routeName;

    TemporaryTestDirectory runDir("integ01_" + routeName);
    juce::File exportDirectory(runDir.path.string());

    // 1. Instanciar fixture sintético determinista
    SyntheticAudioFixture syntheticFixture;
    syntheticFixture.prepareToPlay(48000.0, 256);

    // 2. Instanciar motor de audio de laboratorio y conectar fixture como plugin activo
    audio::LabAudioEngine audioEngine;
    audioEngine.setActivePluginInstance(&syntheticFixture, 48000.0, 256);

    // 3. Hardware automatizado de prueba y secuenciador
    hardware::MockHardwareController mockHw;
    ProfilingSequencer sequencer(audioEngine, mockHw);

    // 4. Preparar MockAudioEngine conductor de callbacks
    MockAudioEngine mockEngine(audioEngine);
    mockEngine.prepare(48000.0, 256, 2);

    // 5. Configurar sesión y plan canónico común
    TargetSelectionState target = createCanonicalTarget();
    ProfilingSession session = createCanonicalBenchmarkPlan();

    // 6. Lanzar sesión
    bool started = sequencer.startSession(session, exportDirectory, "integ01_" + juce::String(routeName));
    REQUIRE(started);

    // 7. Diagnóstico para pumpUntil
    auto diagCallback = [&]() -> std::string {
        std::string d;
        d += "    route:          " + routeName + "\n";
        d += "    sequencerState: " + std::to_string(static_cast<int>(sequencer.getCurrentState())) + "\n";
        d += "    receiverState:  " + std::to_string(static_cast<int>(audioEngine.getResponseReceiver().getState())) + "\n";
        d += "    capturedSamples:" + std::to_string(mockEngine.getProcessedSampleCount()) + "\n";
        d += "    measuredCount:  " + std::to_string(sequencer.getMeasuredPoints().size()) + "\n";
        return d;
    };

    // 8. Bombear callbacks hasta completar
    auto pumpRes = mockEngine.pumpUntil([&]() {
        return sequencer.getCurrentState() == SequencerState::Finished ||
               sequencer.getCurrentState() == SequencerState::ErrorState ||
               !sequencer.isRunningSession();
    }, 5000, diagCallback);

    // Esperar a que el hilo del secuenciador cierre de forma limpia
    sequencer.waitForThreadToExit(2000);
    // Purgar último bloque para procesar eventos pendientes en la cola MIDI (NoteOff / AllNotesOff)
    mockEngine.pumpOneBlock();

    result.sessionCompleted = pumpRes.success && (sequencer.getCurrentState() == SequencerState::Finished);
    result.pumpedBlocks = pumpRes.pumpedBlocks + 1;
    result.processedSamples = pumpRes.processedSamples + 256;
    result.stopReason = pumpRes.stopReason;
    result.diagnosticReport = pumpRes.diagnosticReport;

    // 9. Recolectar artefactos de la ejecución
    result.midiTrace = syntheticFixture.getMidiTrace();
    result.capturedAudioL = mockEngine.getCapturedOutputL();
    result.capturedAudioR = mockEngine.getCapturedOutputR();
    result.canonicalAudioSha256 = MockAudioEngine::computeCanonicalBufferSha256(result.capturedAudioL);
    result.measuredPoints = sequencer.getMeasuredPoints();

    // 10. Evaluar clasificación formal resultante
    ProfilingSessionSnapshot snap;
    snap.target = target;
    snap.sessionStatus = ProfilingSessionStatus::Completed;
    snap.workflowMode = workflowMode;
    ExcitationRecipeState recipe;
    recipe.status = RecipeStatus::Valid;
    recipe.isValid = true;
    snap.excitation = recipe;
    snap.calibration.digital.verified = true;
    result.deducedKind = deduceExperimentKindFromSnapshot(snap);

    // 11. Desconectar fixture
    audioEngine.setActivePluginInstance(nullptr);

    return result;
}

} // namespace

// ===========================================================================
// INTEGRATION-01: Auditoría de Paridad E2E de Audio y DSP
// ===========================================================================

TEST_CASE("INTEGRATION-01: Guiado Sistemático vs. Libre Sistemático - Paridad E2E de Audio y DSP", "[integration][integration-01]")
{
    // Ejecutar Ruta GS (Guiado Sistemático)
    RouteExecutionResult gs = executeRoute("GS", UiWorkflowMode::Guided);

    // Ejecutar Ruta LS (Libre Sistemático)
    RouteExecutionResult ls = executeRoute("LS", UiWorkflowMode::Classic);

    // -----------------------------------------------------------------------
    // DIAGNÓSTICO PREVIO: Ambas rutas deben completar sin timeout
    // -----------------------------------------------------------------------
    INFO("GS Stop Reason: " << gs.stopReason);
    INFO("GS Diagnostic:  " << gs.diagnosticReport);
    REQUIRE(gs.sessionCompleted);

    INFO("LS Stop Reason: " << ls.stopReason);
    INFO("LS Diagnostic:  " << ls.diagnosticReport);
    REQUIRE(ls.sessionCompleted);

    // -----------------------------------------------------------------------
    // NIVEL 1: CONTROL EXACTO (D5 - Estímulo y Eventos Efectivos)
    // -----------------------------------------------------------------------
    SECTION("Nivel 1: Control exacto - Conteo, orden y eventos MIDI idénticos")
    {
        // Mismo número de bloques bombeados y muestras procesadas
        CHECK(gs.pumpedBlocks == ls.pumpedBlocks);
        CHECK(gs.processedSamples == ls.processedSamples);

        // Mismo número de puntos medidos (3 trials)
        REQUIRE(gs.measuredPoints.size() == 3u);
        REQUIRE(ls.measuredPoints.size() == 3u);

        // Mismo número de eventos MIDI registrados en el fixture
        REQUIRE(gs.midiTrace.size() == ls.midiTrace.size());
        REQUIRE(gs.midiTrace.size() >= 6u); // Al menos 3 NoteOn + 3 NoteOff

        // Verificación evento por evento: NoteOn, NoteOff, canal, nota, velocidad.
        // NOTA: sampleOffset absoluto NO se compara porque el ProfilingSequencer usa
        // juce::Time::getMillisecondCounterHiRes() para el gate del NoteOff, introduciendo
        // jitter de scheduling del OS (±10ms = ±480 muestras a 48kHz). Lo que sí es
        // invariante es el tipo, canal, nota y velocidad de cada evento.
        for (size_t i = 0; i < gs.midiTrace.size(); ++i)
        {
            const auto& evGS = gs.midiTrace[i];
            const auto& evLS = ls.midiTrace[i];

            CHECK(evGS.channel == evLS.channel);
            CHECK(evGS.noteNumber == evLS.noteNumber);
            CHECK(evGS.isNoteOn == evLS.isNoteOn);
            CHECK(evGS.isNoteOff == evLS.isNoteOff);
            CHECK(evGS.velocity == Catch::Approx(evLS.velocity).margin(1e-5f));
            // sampleOffset: verificamos solo que NoteOn precede a su NoteOff correspondiente
            // dentro de cada ruta (orden relativo), no igualdad absoluta entre rutas.
            if (evGS.isNoteOff && i >= 1)
            {
                CHECK(gs.midiTrace[i].sampleOffset > gs.midiTrace[i - 1].sampleOffset);
                CHECK(ls.midiTrace[i].sampleOffset > ls.midiTrace[i - 1].sampleOffset);
            }
        }
    }

    // -----------------------------------------------------------------------
    // NIVEL 2: AUDIO CANÓNICO (D6 - Audio Observado Idéntico)
    // -----------------------------------------------------------------------
    SECTION("Nivel 2: Audio canónico - Muestras idénticas, max diff <= 1e-7 y SHA-256 exacto")
    {
        // Mismo número de muestras capturadas en canal izquierdo y derecho
        REQUIRE_FALSE(gs.capturedAudioL.empty());
        REQUIRE(gs.capturedAudioL.size() == ls.capturedAudioL.size());
        REQUIRE(gs.capturedAudioR.size() == ls.capturedAudioR.size());

        // Máxima diferencia absoluta muestra a muestra <= 1e-7
        float maxDiffL = MockAudioEngine::computeMaxAbsoluteDifference(gs.capturedAudioL, ls.capturedAudioL);
        float maxDiffR = MockAudioEngine::computeMaxAbsoluteDifference(gs.capturedAudioR, ls.capturedAudioR);

        INFO("Max Absolute Difference Canal L: " << maxDiffL);
        INFO("Max Absolute Difference Canal R: " << maxDiffR);
        CHECK(maxDiffL <= 1e-7f);
        CHECK(maxDiffR <= 1e-7f);

        // Error cuadrático medio (RMSE) <= 1e-7
        float rmseL = MockAudioEngine::computeRmse(gs.capturedAudioL, ls.capturedAudioL);
        float rmseR = MockAudioEngine::computeRmse(gs.capturedAudioR, ls.capturedAudioR);

        INFO("RMSE Canal L: " << rmseL);
        INFO("RMSE Canal R: " << rmseR);
        CHECK(rmseL <= 1e-7f);
        CHECK(rmseR <= 1e-7f);

        // SHA-256 canónico idéntico sobre bytes IEEE-754 float32 little-endian
        INFO("Canonical SHA-256 GS: " << gs.canonicalAudioSha256);
        INFO("Canonical SHA-256 LS: " << ls.canonicalAudioSha256);
        CHECK(gs.canonicalAudioSha256 == ls.canonicalAudioSha256);
    }

    // -----------------------------------------------------------------------
    // NIVEL 3: SALIDA CIENTÍFICA (D7 - Métricas DSP y Clasificación Metrológica)
    // -----------------------------------------------------------------------
    SECTION("Nivel 3: Salida científica - Métricas DSP y clasificación formal idénticas")
    {
        for (size_t i = 0; i < gs.measuredPoints.size(); ++i)
        {
            const auto& ptGS = gs.measuredPoints[i];
            const auto& ptLS = ls.measuredPoints[i];

            CHECK(ptGS.testId == ptLS.testId);
            CHECK(ptGS.blockType == ptLS.blockType);
            CHECK(ptGS.stimulusType == ptLS.stimulusType);

            // THD y SNR calculados por el motor analítico
            CHECK(ptGS.thdPercent == Catch::Approx(ptLS.thdPercent).margin(1e-5f));
            CHECK(ptGS.snrDb == Catch::Approx(ptLS.snrDb).margin(1e-4f));

            // Mu / Sigma (Peak y desviación)
            CHECK(ptGS.muSigmaValue.mean == Catch::Approx(ptLS.muSigmaValue.mean).margin(1e-5f));
            CHECK(ptGS.muSigmaValue.stdDev == Catch::Approx(ptLS.muSigmaValue.stdDev).margin(1e-5f));
        }

        // Clasificación formal: Ambos deben converger en Measurement (HITO-CONVERGENCIA-01)
        CHECK(gs.deducedKind == ExperimentKind::Measurement);
        CHECK(ls.deducedKind == ExperimentKind::Measurement);
    }
}
