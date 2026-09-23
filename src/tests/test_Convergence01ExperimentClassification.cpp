/**
 * @file test_Convergence01ExperimentClassification.cpp
 * @brief HITO-CONVERGENCIA-01: Verificación de Convergencia Semántica y Clasificación Canónica.
 *
 * Valida la resolución de la divergencia D3 y el desacoplamiento de workflowMode:
 * - CONVERGENCE-01A: GS con target canónico y receta formal -> ExperimentKind::Measurement.
 * - CONVERGENCE-01B: LS con idéntico target y receta formal -> ExperimentKind::Measurement (Convergencia D3 PASS).
 * - CONVERGENCE-01C: Toma Libre (ad-hoc / sin plan) -> ExperimentKind::Exploration y exportación bloqueada.
 * - CONVERGENCE-01D: Plan inválido o sin target -> ExperimentKind::Exploration.
 * - CONVERGENCE-01E: D4 Preflight a dos niveles (isReadyForProfiling vs deduceExperimentKind).
 * - CONVERGENCE-01F: Controller buildCurrentExperimentRecord() convergente (mismo kind en GS y LS).
 */

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "gui/session/ProfilingSessionContracts.h"
#include "gui/session/ProfilingSessionController.h"
#include "core/ExperimentRecord.h"
#include "core/ExperimentStorage.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
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

ExcitationRecipeState createValidRecipe()
{
    ExcitationRecipeState r;
    r.status = RecipeStatus::Valid;
    r.isValid = true;
    r.targetIdentity = "synthetic_fixture_demo";
    
    MidiRecipe midi;
    midi.firstNote = 60;
    midi.lastNote = 60;
    midi.velocities = { 64 };
    midi.repetitions = 3;
    midi.gateMs = 250.0;
    midi.settlingMs = 50.0;
    r.midi = midi;

    return r;
}

CalibrationStatus createReadyCalibration()
{
    CalibrationStatus c;
    c.audio.requirement = CalibrationRequirement::NotApplicable;
    c.midi.requirement = CalibrationRequirement::NotApplicable;
    c.digital.requirement = CalibrationRequirement::Required;
    c.digital.verified = true;
    return c;
}

} // namespace

TEST_CASE("HITO-CONVERGENCIA-01: Clasificacion Metrologica y Convergencia Semantica", "[convergence][convergence-01]")
{
    SECTION("CONVERGENCE-01A: Guiado Sistematico con plan valido clasifica como Measurement")
    {
        ProfilingSessionSnapshot snap;
        snap.workflowMode = UiWorkflowMode::Guided;
        snap.target = createBenchmarkTarget();
        snap.excitation = createValidRecipe();
        snap.calibration = createReadyCalibration();

        // La intención declarada responde a un ensayo formal
        CHECK(deduceExperimentKindFromSnapshot(snap) == ExperimentKind::Measurement);
    }

    SECTION("CONVERGENCE-01B: Libre Sistematico con identico plan clasifica como Measurement (D3 PASS)")
    {
        ProfilingSessionSnapshot snap;
        snap.workflowMode = UiWorkflowMode::Classic;
        snap.target = createBenchmarkTarget();
        snap.excitation = createValidRecipe();
        snap.calibration = createReadyCalibration();

        // Ambos modos de interfaz deben deducir exactamente el mismo ExperimentKind
        CHECK(deduceExperimentKindFromSnapshot(snap) == ExperimentKind::Measurement);
    }

    SECTION("CONVERGENCE-01C: Toma Libre queda clasificada como Exploration y bloqueada para exportar")
    {
        ProfilingSessionSnapshot snap;
        snap.workflowMode = UiWorkflowMode::Classic;
        // En Toma Libre no hay receta formal declarada
        snap.target = createBenchmarkTarget();
        snap.excitation.isValid = false;
        snap.excitation.status = RecipeStatus::Uninitialized;
        snap.excitation.midi = std::nullopt;
        snap.excitation.manual = std::nullopt;

        CHECK(deduceExperimentKindFromSnapshot(snap) == ExperimentKind::Exploration);

        // Además, las guardas de exportación bloquean la toma libre aislada
        ExportReadiness readiness = evaluateExportReadinessFromSnapshot(snap);
        CHECK_FALSE(readiness.canProceed());
        CHECK(readiness.decision == ExportReadiness::Decision::Blocked);
    }

    SECTION("CONVERGENCE-01D: Snapshot sin target valido degrada a Exploration")
    {
        ProfilingSessionSnapshot snap;
        snap.target.targetId = ""; // Sin target
        snap.excitation = createValidRecipe();

        CHECK(deduceExperimentKindFromSnapshot(snap) == ExperimentKind::Exploration);
    }

    SECTION("CONVERGENCE-01E: Auditoria D4 de Preflight a dos niveles")
    {
        // Sub-nivel 1: Calibracion valida en GS y LS
        {
            ProfilingSessionSnapshot gsSnap;
            gsSnap.workflowMode = UiWorkflowMode::Guided;
            gsSnap.target = createBenchmarkTarget();
            gsSnap.excitation = createValidRecipe();
            gsSnap.calibration = createReadyCalibration();

            ProfilingSessionSnapshot lsSnap;
            lsSnap.workflowMode = UiWorkflowMode::Classic;
            lsSnap.target = createBenchmarkTarget();
            lsSnap.excitation = createValidRecipe();
            lsSnap.calibration = createReadyCalibration();

            CHECK(gsSnap.calibration.isReadyForProfiling());
            CHECK(lsSnap.calibration.isReadyForProfiling());
            CHECK(deduceExperimentKindFromSnapshot(gsSnap) == ExperimentKind::Measurement);
            CHECK(deduceExperimentKindFromSnapshot(lsSnap) == ExperimentKind::Measurement);
        }

        // Sub-nivel 2: Calibracion requerida pendiente no altera la intencion metrologica, pero bloquea preflight
        {
            ProfilingSessionSnapshot snap;
            snap.target = createBenchmarkTarget();
            snap.excitation = createValidRecipe();
            snap.calibration.audio.requirement = CalibrationRequirement::Required;
            snap.calibration.audio.completed = false;
            snap.calibration.audio.bypassed = false;

            // Preflight bloqueado: no listo para perfilar
            CHECK_FALSE(snap.calibration.isReadyForProfiling());

            // Pero la intencion del ensayo sigue siendo formalmente Measurement (no se degrada a exploracion ad-hoc)
            CHECK(deduceExperimentKindFromSnapshot(snap) == ExperimentKind::Measurement);
        }

        // Sub-nivel 3: Target digital sin calibracion analogica requerida
        {
            ProfilingSessionSnapshot digitalSnap;
            digitalSnap.target = createBenchmarkTarget();
            digitalSnap.excitation = createValidRecipe();
            digitalSnap.calibration.audio.requirement = CalibrationRequirement::NotApplicable;
            digitalSnap.calibration.midi.requirement = CalibrationRequirement::NotApplicable;
            digitalSnap.calibration.digital.requirement = CalibrationRequirement::Required;
            digitalSnap.calibration.digital.verified = true;

            CHECK(digitalSnap.calibration.isReadyForProfiling());
            CHECK(deduceExperimentKindFromSnapshot(digitalSnap) == ExperimentKind::Measurement);
        }
    }

    SECTION("CONVERGENCE-01F: Controller buildCurrentExperimentRecord() no bifurca por workflowMode")
    {
        ProfilingSessionController cGuided;
        cGuided.setWorkflowMode(UiWorkflowMode::Guided);
        cGuided.selectTarget(createBenchmarkTarget());

        ProfilingSessionController cClassic;
        cClassic.setWorkflowMode(UiWorkflowMode::Classic);
        cClassic.selectTarget(createBenchmarkTarget());

        auto recGuided = cGuided.buildCurrentExperimentRecord();
        auto recClassic = cClassic.buildCurrentExperimentRecord();

        // [CONVERGENCIA D3 CERTIFICADA]:
        // Guiado Sistemático y Libre Sistemático obtienen el mismo ExperimentKind
        CHECK(recGuided.kind == recClassic.kind);
        CHECK(recGuided.kind == ExperimentKind::Measurement);
    }
}
