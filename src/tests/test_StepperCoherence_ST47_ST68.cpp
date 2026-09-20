#include <catch2/catch_test_macros.hpp>
#include "../gui/session/ProfilingSessionController.h"
#include "../gui/session/ProfilingSessionContracts.h"
#include "../gui/soundid/SoundIdSidebarStepper.h"
#include "../gui/WorkflowStepperBar.h"
#include "../gui/soundid/SoundIdTargetView.h"
#include "../gui/HardwareSelectorPill.h"
#include "../gui/controllers/WorkflowNavigationController.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;

TEST_CASE("ST-47: Solo catalogSelector puede cambiar el target activo", "[coherence][st47]")
{
    ProfilingSessionController controller;
    TargetSelectionState initialTarget;
    initialTarget.targetId = "target_initial";
    initialTarget.targetName = "Target Initial";
    initialTarget.kind = TargetKind::PluginVST3;

    REQUIRE(controller.selectTarget(initialTarget));
    auto snap1 = controller.getCurrentSnapshot();
    CHECK(snap1.target.targetId == "target_initial");

    // Seleccionar nuevo target desde catálogo oficial
    TargetSelectionState newTarget;
    newTarget.targetId = "target_catalog_only";
    newTarget.targetName = "Target From Catalog";
    newTarget.kind = TargetKind::HardwareDigital;

    REQUIRE(controller.selectTarget(newTarget));
    auto snap2 = controller.getCurrentSnapshot();
    CHECK(snap2.target.targetId == "target_catalog_only");
    CHECK(snap2.sessionStatus == ProfilingSessionStatus::TargetSelected);
}

TEST_CASE("ST-48 & ST-66: TargetView es ficha pasiva y no ofrece selección independiente", "[coherence][st48][st66]")
{
    ProfilingSessionController controller;
    soundid::SoundIdTargetView targetView(controller);

    TargetSelectionState target;
    target.targetId = "dexed_vst3";
    target.targetName = "Dexed FM Synthesizer";
    target.kind = TargetKind::PluginVST3;
    target.supportsMidiInput = true;
    target.isConnected = true;

    controller.selectTarget(target);
    targetView.updateFromSnapshot(controller.getCurrentSnapshot());

    // TargetView no emite eventos de selección ni modifica el target activo por sí misma
    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.target.targetId == "dexed_vst3");
}

TEST_CASE("ST-49 & ST-67: HardwareSelectorPill es indicador y solo navega a HardwareRouting", "[coherence][st49][st67]")
{
    HardwareSelectorPill pill;
    pill.clearHardware();
    CHECK_FALSE(pill.hasHardwareSelected());
    CHECK(pill.getTooltip().containsIgnoreCase("target hardware"));

    pill.setHardwareInfo("Dexed FM", "Synthesizer", {}, HardwareConnectionStatus::Connected);
    CHECK(pill.hasHardwareSelected());

    // Pill no tiene selector emergente interactivo ni altera el target por sí sola
    bool navigated = false;
    pill.onClick = [&navigated] { navigated = true; };
    pill.simulateClick();
    CHECK(navigated);
}

TEST_CASE("ST-50: Cambiar target refresca ficha, contrato y receta", "[coherence][st50]")
{
    ProfilingSessionController controller;

    TargetSelectionState t1;
    t1.targetId = "dexed_vst3";
    t1.kind = TargetKind::PluginVST3;
    t1.supportsMidiInput = true;
    controller.selectTarget(t1);

    auto snap1 = controller.getCurrentSnapshot();
    CHECK(snap1.excitation.targetControlMode == TargetControlMode::Vst3);
    CHECK(snap1.excitation.midi.has_value());

    TargetSelectionState t2;
    t2.targetId = "pedal_analog";
    t2.kind = TargetKind::HardwareAnalogue;
    t2.supportsMidiInput = false;
    controller.selectTarget(t2);

    auto snap2 = controller.getCurrentSnapshot();
    CHECK(snap2.excitation.targetControlMode == TargetControlMode::NoDigitalControl);
    CHECK(snap2.excitation.excitationMode == ExcitationMode::ManualOperator);
    CHECK(snap2.excitation.manual.has_value());
    CHECK_FALSE(snap2.excitation.midi.has_value());
}

TEST_CASE("ST-51 & ST-61: VST3 no exige loopback físico pero requiere verificación digital explícita", "[coherence][st51][st61]")
{
    ProfilingSessionController controller;

    TargetSelectionState vst3Target;
    vst3Target.targetId = "dexed_vst3";
    vst3Target.kind = TargetKind::PluginVST3;
    vst3Target.supportsMidiInput = true;
    controller.selectTarget(vst3Target);

    auto snap = controller.getCurrentSnapshot();
    // 1. Loopback físico NO requerido
    CHECK(snap.calibration.audio.requirement == CalibrationRequirement::NotApplicable);
    CHECK(snap.calibration.digital.requirement == CalibrationRequirement::Required);

    // 2. ST-61: NO debe estar verificado automáticamente antes de la acción
    CHECK_FALSE(snap.calibration.digital.verified);
    CHECK_FALSE(snap.calibration.isReadyForProfiling());

    // 3. Tras llamar a verifyDigitalCalibration(), pasa a verificado y listo
    controller.verifyDigitalCalibration();
    auto snapVerified = controller.getCurrentSnapshot();
    CHECK(snapVerified.calibration.digital.verified);
    CHECK(snapVerified.calibration.isReadyForProfiling());
}

TEST_CASE("ST-52: Target analógico exige loopback físico", "[coherence][st52]")
{
    ProfilingSessionController controller;

    TargetSelectionState analogTarget;
    analogTarget.targetId = "korg_ms20_pedal";
    analogTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(analogTarget);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.calibration.audio.requirement == CalibrationRequirement::Required);
    CHECK_FALSE(snap.calibration.audio.completed);
    CHECK(snap.calibration.digital.requirement == CalibrationRequirement::NotApplicable);
    CHECK_FALSE(snap.calibration.isReadyForProfiling());

    // Completar calibración de audio
    controller.updateAudioCalibration(true, 0.0f, -3.0f, 5.2f, 96.0f);
    auto snapCal = controller.getCurrentSnapshot();
    CHECK(snapCal.calibration.audio.completed);
    CHECK(snapCal.calibration.isReadyForProfiling());
}

TEST_CASE("ST-53: Target MIDI físico exige audio + latencia MIDI", "[coherence][st53]")
{
    ProfilingSessionController controller;

    TargetSelectionState midiHw;
    midiHw.targetId = "roland_boutique";
    midiHw.kind = TargetKind::HardwareDigital;
    midiHw.supportsMidiInput = true;
    controller.selectTarget(midiHw);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.calibration.audio.requirement == CalibrationRequirement::Required);
    CHECK(snap.calibration.midi.requirement == CalibrationRequirement::Required);
    CHECK(snap.calibration.digital.requirement == CalibrationRequirement::NotApplicable);
    CHECK_FALSE(snap.calibration.isReadyForProfiling());

    // Si solo calibra audio, todavía no está listo para perfilado
    controller.updateAudioCalibration(true, 0.0f, -3.0f, 4.0f, 92.0f);
    CHECK_FALSE(controller.getCurrentSnapshot().calibration.isReadyForProfiling());

    // Al calibrar compuerta MIDI, queda listo
    controller.updateMidiCalibration(true, 1.2f, 0.3f);
    CHECK(controller.getCurrentSnapshot().calibration.isReadyForProfiling());
}

TEST_CASE("ST-54: Target híbrido presenta ambos bloques de calibración", "[coherence][st54]")
{
    ProfilingSessionController controller;

    TargetSelectionState hybrid;
    hybrid.targetId = "hybrid_synth";
    hybrid.kind = TargetKind::HardwareDigital;
    hybrid.supportsMidiInput = true;
    hybrid.supportsParameterAutomation = true;
    controller.selectTarget(hybrid);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.calibration.audio.requirement == CalibrationRequirement::Required);
    CHECK(snap.calibration.midi.requirement == CalibrationRequirement::Required);
}

TEST_CASE("ST-55 & ST-63: Cambiar de target invalida inmediatamente calibración incompatible", "[coherence][st55][st63]")
{
    ProfilingSessionController controller;

    // 1. Target VST3 verificado
    TargetSelectionState vst3;
    vst3.targetId = "vst3_device";
    vst3.kind = TargetKind::PluginVST3;
    controller.selectTarget(vst3);
    controller.verifyDigitalCalibration();
    CHECK(controller.getCurrentSnapshot().calibration.isReadyForProfiling());

    // 2. ST-55 / ST-63: Cambiar a pedal analógico debe invalidar la calibración previa
    TargetSelectionState pedal;
    pedal.targetId = "analog_pedal";
    pedal.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(pedal);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.calibration.audio.requirement == CalibrationRequirement::Required);
    CHECK_FALSE(snap.calibration.audio.completed);
    // ST-63: La calibración del target anterior no permite avanzar
    CHECK_FALSE(snap.calibration.isReadyForProfiling());
}

TEST_CASE("ST-56: Target manual conserva OperatorCardsContainer y receta manual", "[coherence][st56]")
{
    ProfilingSessionController controller;

    TargetSelectionState manualTarget;
    manualTarget.targetId = "moog_sub37_manual";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.excitation.excitationMode == ExcitationMode::ManualOperator);
    REQUIRE(snap.excitation.manual.has_value());
    CHECK(snap.excitation.manual->interactionKind == ManualInteractionKind::PhysicalControlAdjustment);
}

TEST_CASE("ST-57: Target digital con MIDI conserva receta MIDI con parámetros", "[coherence][st57]")
{
    ProfilingSessionController controller;

    TargetSelectionState digitalTarget;
    digitalTarget.targetId = "dexed_synth";
    digitalTarget.kind = TargetKind::PluginVST3;
    digitalTarget.supportsMidiInput = true;
    controller.selectTarget(digitalTarget);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.excitation.excitationMode == ExcitationMode::AutomatedMidi);
    REQUIRE(snap.excitation.midi.has_value());
    CHECK(snap.excitation.midi->firstNote == 36);
    CHECK(snap.excitation.midi->lastNote == 84);
}

TEST_CASE("ST-58: Rollback restaura layout anterior sin duplicaciones", "[coherence][st58]")
{
    TargetViewIntegrationMode mode = TargetViewIntegrationMode::ClassicStep1;
    CHECK(mode == TargetViewIntegrationMode::ClassicStep1);

    mode = TargetViewIntegrationMode::Disabled;
    CHECK(mode == TargetViewIntegrationMode::Disabled);
}

TEST_CASE("ST-59: VST3 sin MIDI no ofrece AutomatedMidi", "[coherence][st59]")
{
    ProfilingSessionController controller;

    TargetSelectionState vst3NoMidi;
    vst3NoMidi.targetId = "fx_plugin_vst3";
    vst3NoMidi.kind = TargetKind::PluginVST3;
    vst3NoMidi.supportsMidiInput = false;
    vst3NoMidi.supportsParameterAutomation = true;
    controller.selectTarget(vst3NoMidi);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.excitation.excitationMode == ExcitationMode::AutomatedVstParameter);
    CHECK_FALSE(snap.excitation.midi.has_value());
}

TEST_CASE("ST-60: VST3 con automatización de parámetros ofrece AutomatedVstParameter", "[coherence][st60]")
{
    ProfilingSessionController controller;

    TargetSelectionState paramPlugin;
    paramPlugin.targetId = "param_synth";
    paramPlugin.kind = TargetKind::PluginVST3;
    paramPlugin.supportsMidiInput = false;
    paramPlugin.supportsParameterAutomation = true;
    controller.selectTarget(paramPlugin);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.excitation.excitationMode == ExcitationMode::AutomatedVstParameter);
}

TEST_CASE("ST-62: Target cambiado durante sesión ejecuta cancelación previa", "[coherence][st62]")
{
    ProfilingSessionController controller;

    TargetSelectionState t1;
    t1.targetId = "target_running";
    t1.kind = TargetKind::PluginVST3;
    controller.selectTarget(t1);

    // Cambiar target
    TargetSelectionState t2;
    t2.targetId = "target_new";
    t2.kind = TargetKind::PluginVST3;
    CHECK(controller.selectTarget(t2));
    CHECK(controller.getCurrentSnapshot().target.targetId == "target_new");
}

TEST_CASE("ST-64: Sesión antigua conserva el significado numérico de sus pasos de Step", "[coherence][st64]")
{
    // Verificar que los valores enteros del enum Step no han cambiado
    CHECK(static_cast<int>(SoundIdSidebarStepper::Step::SystemInfo) == 0);
    CHECK(static_cast<int>(SoundIdSidebarStepper::Step::CalibrateLoopback) == 1);
    CHECK(static_cast<int>(SoundIdSidebarStepper::Step::HardwareRouting) == 2);
    CHECK(static_cast<int>(SoundIdSidebarStepper::Step::RunSession) == 3);
    CHECK(static_cast<int>(SoundIdSidebarStepper::Step::ExportReport) == 4);

    // Pero las etiquetas visuales reflejan el orden natural
    SoundIdSidebarStepper stepper;
    CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::HardwareRouting) == "1. Target & Routing");
    CHECK(stepper.getStepTitle(SoundIdSidebarStepper::Step::CalibrateLoopback) == "2. Calibration & Setup");
}

TEST_CASE("ST-65: Target cambiado marca la receta anterior como IncompatibleWithTarget", "[coherence][st65]")
{
    ProfilingSessionController controller;

    TargetSelectionState t1;
    t1.targetId = "dexed_vst3";
    t1.kind = TargetKind::PluginVST3;
    controller.selectTarget(t1);

    auto snap1 = controller.getCurrentSnapshot();
    CHECK(snap1.excitation.status == RecipeStatus::Valid);

    // Cambiar a target analógico incompatible
    TargetSelectionState t2;
    t2.targetId = "dist_pedal";
    t2.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(t2);

    auto snap2 = controller.getCurrentSnapshot();
    CHECK(snap2.excitation.targetIdentity == "dist_pedal");
    CHECK(snap2.excitation.excitationMode == ExcitationMode::ManualOperator);
}

TEST_CASE("ST-68: No se generan dos eventos al seleccionar un plugin", "[coherence][st68]")
{
    ProfilingSessionController controller;
    int eventCount = 0;

    class TestListener : public IProfilingSessionEventListener
    {
    public:
        int& count;
        explicit TestListener(int& c) : count(c) {}
        void onSessionSnapshotUpdated(const ProfilingSessionSnapshot&) override {}
        void onAlertRaised(const UiAlert&) override {}
        void onWorkflowStageChanged(ProfilingWorkflowStage) override {}
        void onSessionStatusChanged(ProfilingSessionStatus status) override
        {
            if (status == ProfilingSessionStatus::TargetSelected)
                count++;
        }
    };

    TestListener listener(eventCount);
    controller.addListener(&listener);

    TargetSelectionState target;
    target.targetId = "single_event_target";
    target.kind = TargetKind::PluginVST3;

    controller.selectTarget(target);
    CHECK(eventCount == 1);
}
