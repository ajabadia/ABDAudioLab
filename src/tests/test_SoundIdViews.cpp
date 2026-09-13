#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gui/session/ProfilingSessionController.h"
#include "gui/soundid/SoundIdTopHeaderStrip.h"
#include "gui/soundid/SoundIdTargetView.h"
#include "gui/soundid/SoundIdProfilingRunView.h"
#include "gui/soundid/SoundIdResultsSummaryView.h"

using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;

TEST_CASE("SoundIdTopHeaderStrip: Actualización de contexto operativo y badges", "[gui][soundid]")
{
    soundid::SoundIdTopHeaderStrip strip;
    strip.setSize(1000, 40);

    ProfilingSessionSnapshot snapshot;
    snapshot.target.targetName = "Minimoog Model D";
    snapshot.workflowStage = ProfilingWorkflowStage::ProfilingActive;
    snapshot.sessionStatus = ProfilingSessionStatus::Profiling;
    snapshot.evaluation.hasEvaluation = true;
    snapshot.evaluation.recommendedModelType = "Ladder Filter TPT";

    UiAlert alert;
    alert.severity = UiAlert::Severity::Warning;
    alert.title = "Aviso";
    snapshot.activeAlerts.push_back(alert);

    // No debe lanzar excepciones al pintar ni al actualizar
    REQUIRE_NOTHROW(strip.updateFromSnapshot(snapshot));
    REQUIRE_NOTHROW(strip.setHardwareTelemetry(96000.0, 256, 12.5));
}

TEST_CASE("SoundIdTargetView: Reflejo del estado de conexión y avance", "[gui][soundid]")
{
    ProfilingSessionController controller;
    soundid::SoundIdTargetView view(controller);
    view.setSize(800, 600);

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(view.updateFromSnapshot(snap));

    // Conectar target
    TargetSelectionState target;
    target.targetId = "ms20_target";
    target.targetName = "Korg MS-20";
    target.manufacturer = "Korg";
    target.kind = TargetKind::HardwareAnalogue;
    target.isConnected = true;
    target.availableDomainDescription = "MIDI Note 24-84";
    target.parameterCount = 12;

    controller.selectTarget(target);
    snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(view.updateFromSnapshot(snap));
}

TEST_CASE("SoundIdProfilingRunView: Monitor de progreso en vivo y salud acústica", "[gui][soundid]")
{
    ProfilingSessionController controller;
    soundid::SoundIdProfilingRunView runView(controller);
    runView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "vst3_ref";
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "Target ready");
    controller.startProfiling();

    controller.updateProgress(10, 50, 15.0, 60.0, "VCF Sweep Cutoff 0.5");
    controller.updateObservation(-12.0, -1.5, 440.0, false, false, 80.0);

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));

    // Telemetría con clipping
    controller.updateObservation(-0.0, 0.5, 440.0, true, false, 40.0);
    snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(runView.updateFromSnapshot(snap));
}

TEST_CASE("SoundIdResultsSummaryView: Métricas interpretables objetivas y botón de exportación", "[gui][soundid]")
{
    ProfilingSessionController controller;
    soundid::SoundIdResultsSummaryView resView(controller);
    resView.setSize(800, 600);

    TargetSelectionState target;
    target.targetId = "target_x";
    target.isConnected = true;
    controller.selectTarget(target);
    controller.requestAudit();
    controller.updateAuditResult(abdaudiolab::synth::ApprovalStatus::Approved,
                                 "Deterministic", "Resettable", 50.0, true, {}, "OK");
    controller.startProfiling();

    // Actualizar con modelo aceptado
    controller.updateModelEvaluation(abdaudiolab::synth::SelectionStatus::Accepted,
                                     "Wiener-Hammerstein Grey-Box",
                                     -48.2, 0.9998, 97.5,
                                     "C1-C6, Vel 1-127", 1.05, {}, {});
    controller.completeProfiling();

    auto snap = controller.getCurrentSnapshot();
    REQUIRE_NOTHROW(resView.updateFromSnapshot(snap));
    CHECK(snap.exportOptions.canExportCpp == true);
}
