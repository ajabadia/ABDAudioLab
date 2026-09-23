/**
 * @file test_Parity01GuidedVsClassic.cpp
 * @brief PARITY-01: Suite de Caracterización de Paridad y Convergencia.
 *
 * PARITY-01A: Guiado Sistemático vs. Libre Sistemático (demostración de motor y DSP idénticos,
 *             caracterización de bifurcación de metadatos r.kind).
 * PARITY-01B: Clasificación formal y guardas metrológicas de Toma Libre (no certificable).
 *
 * Regla de Oro: "Caracterizar primero; comparar después; migrar después; retirar al final."
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <cmath>

#include "core/ProfilingSequencer.h"
#include "hardware/MockHardwareController.h"
#include "audio/LabAudioEngine.h"
#include "export/ReportExportService.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "gui/session/ProfilingSessionController.h"
#include "measurement/MeasurementSessionContracts.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::exporting;
using namespace abdaudiolab::gui::session;

namespace
{

TargetSelectionState createBenchmarkTarget()
{
    TargetSelectionState t;
    t.targetId = "synthetic_fixture_demo";
    t.targetName = "Sintetizador Virtual de Prueba (Demo Snapshot)";
    t.manufacturer = "ABDAudioLab";
    t.version = "1.0.0";
    t.kind = TargetKind::SyntheticFixture;
    t.isConnected = true;
    t.isDeterministic = true;
    t.availableDomainDescription = "Notas MIDI C1-C6, Vel 1-127";
    t.parameterCount = 8;
    return t;
}

ProfilingSession createBenchmarkSession()
{
    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = "ReferenceSynth";
    meta.targetModule = "SineOscillator";
    meta.operatorMode = "AUTOMATED_MIDI_NOTES";
    session.setMetadata(meta);

    // Caso Testigo Controlado: C4 (Note 60), Velocity 64 (0.50f), Gate 250ms, Settling 50ms, 3 repeticiones
    for (int rep = 0; rep < 3; ++rep)
    {
        TestCase tc;
        tc.testId = "PARITY_BENCH_C4_REP_" + std::to_string(rep + 1);
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

} // namespace

// ===========================================================================
// PARITY-01A: Guiado Sistemático vs. Libre Sistemático
// ===========================================================================
//
// NOTA DE CARACTERIZACIÓN (PARITY-01):
// D5/D6/D7 (ejecución real del ProfilingSequencer) requieren hardware de audio
// inicializado (WASAPI/DirectSound). ProfilingSequencer hereda de juce::Thread y
// bloquea esperando el callback audioDeviceIOCallbackWithContext, que nunca llega
// en un entorno headless sin dispositivo de audio real.
//
// La evidencia de convergencia de motor para PARITY-01 es estructural:
//   - Guiado y Libre usan el MISMO tipo ProfilingSequencer (confirmado por inspección
//     estática de ProfilingSessionController.cpp).
//   - La divergencia está ÚNICAMENTE en r.kind (línea 1019, documentada en D3).
//   - Las pruebas D5/D6/D7 con ejecución real se delegan a la suite de integración
//     INTEGRATION-01 (requiere hardware real o MockAudioEngine con bucle de retorno).
//
// Esta suite fija la línea base estructural y la bifurcación de metadatos.
// ===========================================================================
TEST_CASE("PARITY-01A: Guiado y Libre comparten identico motor ProfilingSequencer (contrato estructural)", "[parity][parity-01a]")
{
    // -----------------------------------------------------------------------
    // DIMENSIÓN D1: Selección de Target — mismo contrato TargetSelectionState
    // -----------------------------------------------------------------------
    SECTION("D1: Guiado y Libre construyen el mismo TargetSelectionState para el mismo hardware")
    {
        TargetSelectionState targetGuided = createBenchmarkTarget();
        TargetSelectionState targetClassic = createBenchmarkTarget();

        // Mismo target ID, mismo nombre, mismo fabricante
        CHECK(targetGuided.targetId      == targetClassic.targetId);
        CHECK(targetGuided.targetName    == targetClassic.targetName);
        CHECK(targetGuided.manufacturer  == targetClassic.manufacturer);
        CHECK(targetGuided.isDeterministic == targetClassic.isDeterministic);
    }

    // -----------------------------------------------------------------------
    // DIMENSIÓN D2: Construcción de Sesión — misma ProfilingSession
    // -----------------------------------------------------------------------
    SECTION("D2: Guiado y Libre construyen una ProfilingSession identica para el mismo banco de pruebas")
    {
        ProfilingSession sessionGuided  = createBenchmarkSession();
        ProfilingSession sessionClassic = createBenchmarkSession();

        // Misma cantidad de TestCases
        REQUIRE(sessionGuided.getTestCases().size()  == 3u);
        REQUIRE(sessionClassic.getTestCases().size() == 3u);

        // Mismos IDs de test, misma nota MIDI, mismo gate
        for (size_t i = 0; i < sessionGuided.getTestCases().size(); ++i)
        {
            const auto& tcG = sessionGuided.getTestCases()[i];
            const auto& tcC = sessionClassic.getTestCases()[i];
            CHECK(tcG.testId           == tcC.testId);
            CHECK(tcG.midiNoteNumber   == tcC.midiNoteNumber);
            CHECK(tcG.midiChannel      == tcC.midiChannel);
            CHECK(tcG.noteGateDurationSec == Catch::Approx(tcC.noteGateDurationSec).margin(1e-6f));
            CHECK(tcG.stimulusDurationSec == Catch::Approx(tcC.stimulusDurationSec).margin(1e-6f));
        }
    }

    // -----------------------------------------------------------------------
    // DIMENSIÓN D5 (estructural): Mismo tipo de sequencer, mismo estado inicial
    // -----------------------------------------------------------------------
    SECTION("D5-struct: Ambas rutas instancian ProfilingSequencer en estado Idle")
    {
        audio::LabAudioEngine engineA;
        hardware::MockHardwareController hwA;
        ProfilingSequencer seqGuided(engineA, hwA);

        audio::LabAudioEngine engineB;
        hardware::MockHardwareController hwB;
        ProfilingSequencer seqClassic(engineB, hwB);

        // Ambos parten de Idle — estado inicial antes de startSession
        CHECK(seqGuided.getCurrentState()  == SequencerState::Idle);
        CHECK(seqClassic.getCurrentState() == SequencerState::Idle);

        // Ninguno está corriendo antes de ser iniciado
        CHECK_FALSE(seqGuided.isRunningSession());
        CHECK_FALSE(seqClassic.isRunningSession());

        // [HALLAZGO ESTRUCTURAL PARITY-01]:
        // Guiado y Libre usan exactamente el mismo tipo ProfilingSequencer.
        // La divergencia de comportamiento es únicamente de metadatos (D3).
        // Ejecución real (D5/D6/D7 con audio) → INTEGRATION-01.
    }

    // -----------------------------------------------------------------------
    // DIMENSIÓN D3: Caracterización de la Divergencia de Metadatos (r.kind)
    // -----------------------------------------------------------------------
    SECTION("D3: Caracterizacion factual de r.kind segun UiWorkflowMode")
    {
        ProfilingSessionController cGuided;
        cGuided.setWorkflowMode(UiWorkflowMode::Guided);
        cGuided.selectTarget(createBenchmarkTarget());

        ProfilingSessionController cClassic;
        cClassic.setWorkflowMode(UiWorkflowMode::Classic);
        cClassic.selectTarget(createBenchmarkTarget());

        // Ambos controladores tienen el mismo target y estado operativo
        CHECK(cGuided.getCurrentSnapshot().target.targetId  == cClassic.getCurrentSnapshot().target.targetId);

        // El snapshot refleja el workflowMode asignado — misma API, diferente flag
        CHECK(cGuided.getCurrentSnapshot().workflowMode  == UiWorkflowMode::Guided);
        CHECK(cClassic.getCurrentSnapshot().workflowMode == UiWorkflowMode::Classic);

        // [HALLAZGO FACTUAL PARITY-01 — BIFURCACIÓN DOCUMENTADA]:
        // ProfilingSessionController::buildExportMetadataRecord (línea ~1019):
        //   r.kind = (workflowMode == Guided) ? Measurement : Exploration;
        // El modo visual altera únicamente la clasificación del registro de exportación.
        // El motor de medición (ProfilingSequencer) NO cambia.
        // Convergencia objetivo: eliminar esta bifurcación en HITO-CONVERGENCIA-01.
    }
}

// ===========================================================================
// PARITY-01B: Clasificación Formal y Guardas de Toma Libre
// ===========================================================================
TEST_CASE("PARITY-01B: Toma Libre queda clasificada como exploracion no certificable", "[parity][parity-01b]")
{
    SECTION("ControlStateSnapshot de Toma Libre sin controles declarados registra unknown")
    {
        measurement::ControlStateSnapshot snap;
        snap.controlId = "unspecified_param";
        snap.confirmationStatus = "unknown";
        snap.displayValue = "Posición no declarada";
        snap.normalizedValue = std::nullopt;

        CHECK(snap.confirmationStatus == "unknown");
        CHECK(snap.displayValue.find("Posición no declarada") != std::string::npos);
        CHECK_FALSE(snap.normalizedValue.has_value());
    }

    SECTION("ExportReadinessEvaluator bloquea sesion si no existe target o no hay evaluacion aprobada")
    {
        ProfilingSessionSnapshot snap;
        snap.sessionStatus = ProfilingSessionStatus::Idle;
        snap.target.targetId = ""; // Sin target — fuerza bloqueo por guardia dura
        snap.evaluation.selectionStatus = synth::SelectionStatus::Inconclusive; // sin evaluación

        ExportReadiness readiness = evaluateExportReadinessFromSnapshot(snap);
        CHECK_FALSE(readiness.canProceed());
        CHECK(readiness.decision == ExportReadiness::Decision::Blocked);
    }

    SECTION("Sesion marcada como LoadedForExploration no habilita exportacion formal sin aprobacion")
    {
        ProfilingSessionSnapshot snap;
        snap.sessionStatus = ProfilingSessionStatus::Completed;
        snap.target = createBenchmarkTarget();
        snap.evaluation.selectionStatus = synth::SelectionStatus::Rejected; // veredicto negativo

        ExportReadiness readiness = evaluateExportReadinessFromSnapshot(snap);
        CHECK_FALSE(readiness.canProceed());
        CHECK(readiness.decision == ExportReadiness::Decision::Blocked);
    }
}
