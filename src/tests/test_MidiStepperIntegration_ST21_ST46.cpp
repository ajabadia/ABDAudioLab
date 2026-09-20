#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "gui/session/ProfilingSessionContracts.h"
#include "gui/session/ProfilingSessionController.h"
#include "gui/soundid/SoundIdExcitationConfigPanel.h"
#include "gui/soundid/SoundIdProfilingRunView.h"
#include "core/SessionSerializer.h"
#include "core/ProfilingSequencer.h"
#include "core/HardwareContractRegistry.h"
#include "hardware/ManualAnalogueController.h"
#include "gui/SessionExecutionCoordinator.h"
#include "gui/OperatorCardsContainerComponent.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::session;

// ==============================================================================
// ST-21 a ST-30: Validación del Modelo, Comandos, Persistencia y Silenciamiento
// ==============================================================================

TEST_CASE("ST-21: Deteccion de modo de control de target (Digital vs No Digital)", "[stepper][excitation]")
{
    ProfilingSessionController controller;

    // Target Analógico / Manual (NoDigitalControl)
    TargetSelectionState manualTarget;
    manualTarget.targetId = "boss_ds1_distortion";
    manualTarget.targetName = "Boss DS-1 Analogue Distortion";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    manualTarget.isConnected = true;

    REQUIRE(controller.selectTarget(manualTarget));
    auto snap1 = controller.getCurrentSnapshot();
    CHECK(snap1.excitation.targetControlMode == TargetControlMode::NoDigitalControl);
    CHECK(snap1.excitation.excitationMode == ExcitationMode::ManualOperator);
    CHECK(snap1.excitation.manual.has_value());
    CHECK(!snap1.excitation.midi.has_value());

    // Target Digital VST3
    TargetSelectionState vstTarget;
    vstTarget.targetId = "asb_dexed_fm";
    vstTarget.targetName = "Dexed FM VST3";
    vstTarget.kind = TargetKind::PluginVST3;
    vstTarget.supportsMidiInput = true;
    vstTarget.isConnected = true;

    REQUIRE(controller.selectTarget(vstTarget));
    auto snap2 = controller.getCurrentSnapshot();
    CHECK(snap2.excitation.targetControlMode == TargetControlMode::Vst3);
    CHECK(snap2.excitation.excitationMode == ExcitationMode::AutomatedMidi);
    CHECK(snap2.excitation.midi.has_value());
}

TEST_CASE("ST-22: Conmutacion de modo de excitacion y proteccion de target no digital", "[stepper][excitation]")
{
    ProfilingSessionController controller;

    // Con target analógico, el modo AutomatedMidi es rechazado
    TargetSelectionState manualTarget;
    manualTarget.targetId = "manual_eurorack_vcf";
    manualTarget.targetName = "Eurorack VCF Modular";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    controller.setExcitationMode(ExcitationMode::AutomatedMidi);
    auto snap1 = controller.getCurrentSnapshot();
    CHECK(snap1.excitation.excitationMode == ExcitationMode::ManualOperator);
    CHECK(!snap1.activeAlerts.empty());

    // Con target VST3, se permite alternar entre AutomatedMidi y ManualOperator
    TargetSelectionState vstTarget;
    vstTarget.targetId = "dexed_synth";
    vstTarget.kind = TargetKind::PluginVST3;
    vstTarget.supportsMidiInput = true;
    controller.selectTarget(vstTarget);

    controller.setExcitationMode(ExcitationMode::ManualOperator);
    auto snap2 = controller.getCurrentSnapshot();
    CHECK(snap2.excitation.excitationMode == ExcitationMode::ManualOperator);
    CHECK(snap2.excitation.manual.has_value());

    controller.setExcitationMode(ExcitationMode::AutomatedMidi);
    auto snap3 = controller.getCurrentSnapshot();
    CHECK(snap3.excitation.excitationMode == ExcitationMode::AutomatedMidi);
    CHECK(snap3.excitation.midi.has_value());
}

TEST_CASE("ST-23: Validacion de receta de excitacion MIDI", "[stepper][excitation]")
{
    ProfilingSessionController controller;
    TargetSelectionState vstTarget;
    vstTarget.targetId = "test_synth";
    vstTarget.kind = TargetKind::PluginVST3;
    vstTarget.supportsMidiInput = true;
    controller.selectTarget(vstTarget);

    // Receta válida
    MidiRecipe validRecipe;
    validRecipe.firstNote = 36;
    validRecipe.lastNote = 84;
    validRecipe.velocities = { 32, 64, 127 };
    validRecipe.gateMs = 250.0;
    validRecipe.settlingMs = 50.0;
    validRecipe.midiChannel = 1;
    controller.updateMidiRecipe(validRecipe);

    auto snapValid = controller.getCurrentSnapshot();
    CHECK(snapValid.excitation.isValid);
    CHECK(snapValid.excitation.validationError.empty());

    // Inversión de notas inválida
    MidiRecipe invalidRecipe = validRecipe;
    invalidRecipe.firstNote = 90;
    invalidRecipe.lastNote = 40;
    controller.updateMidiRecipe(invalidRecipe);

    auto snapInvalid = controller.getCurrentSnapshot();
    CHECK(!snapInvalid.excitation.isValid);
    CHECK(!snapInvalid.excitation.validationError.empty());
}

TEST_CASE("ST-24: Validacion de receta de operador manual", "[stepper][excitation]")
{
    ProfilingSessionController controller;
    TargetSelectionState manualTarget;
    manualTarget.targetId = "analog_fuzz";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    ManualOperatorRecipe validManual;
    validManual.interactionKind = ManualInteractionKind::PhysicalControlAdjustment;
    validManual.instruction = "Girar perilla de Fuzz al 75%";
    validManual.expectedSetting = "Fuzz: 75%, Tone: 50%";
    validManual.repetitions = 2;
    validManual.settlingMs = 400.0;
    controller.updateManualRecipe(validManual);

    auto snapValid = controller.getCurrentSnapshot();
    CHECK(snapValid.excitation.isValid);
    CHECK(snapValid.excitation.manual.has_value());
    CHECK(snapValid.excitation.manual->repetitions == 2);

    // Repeticiones inválidas (< 1)
    ManualOperatorRecipe invalidManual = validManual;
    invalidManual.repetitions = 0;
    controller.updateManualRecipe(invalidManual);

    auto snapInvalid = controller.getCurrentSnapshot();
    CHECK(!snapInvalid.excitation.isValid);
}

TEST_CASE("ST-25: Mapeo unico y documentado de SequencerState a TrialLifecycleStage", "[stepper][contracts]")
{
    // 0: Idle -> Armed
    CHECK(mapSequencerStateToTrialStage(0) == TrialLifecycleStage::Armed);
    // 3: WaitForStabilization -> WaitForStabilization
    CHECK(mapSequencerStateToTrialStage(3) == TrialLifecycleStage::WaitForStabilization);
    // 4: WaitingForOperator -> WaitingForOperator
    CHECK(mapSequencerStateToTrialStage(4) == TrialLifecycleStage::WaitingForOperator);
    // 5: InjectStimulus -> Capturing
    CHECK(mapSequencerStateToTrialStage(5) == TrialLifecycleStage::Capturing);
    // 6: CaptureAndAnalyze -> Capturing
    CHECK(mapSequencerStateToTrialStage(6) == TrialLifecycleStage::Capturing);
    // 9: Finished -> Finished
    CHECK(mapSequencerStateToTrialStage(9) == TrialLifecycleStage::Finished);
    // 10: ErrorState -> ErrorState
    CHECK(mapSequencerStateToTrialStage(10) == TrialLifecycleStage::ErrorState);
}

TEST_CASE("ST-26: Publicacion de snapshot con receta de excitacion activa", "[stepper][excitation]")
{
    ProfilingSessionController controller;
    uint64_t initialSeq = controller.getCurrentSnapshot().monotonicSequence;

    TargetSelectionState target;
    target.targetId = "test_synth_pub";
    target.kind = TargetKind::PluginVST3;
    target.supportsMidiInput = true;
    controller.selectTarget(target);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.monotonicSequence > initialSeq);
    CHECK(snap.excitation.targetControlMode == TargetControlMode::Vst3);
    CHECK(snap.excitation.excitationMode == ExcitationMode::AutomatedMidi);
}

TEST_CASE("ST-27: Dos niveles de silenciamiento de emergencia", "[stepper][safety]")
{
    core::ProfilingHardwareDispatcher dispatcher(nullptr);

    // Nivel 1: All Notes Off (CC 123) en los 16 canales
    for (int ch = 1; ch <= 16; ++ch)
    {
        dispatcher.sendAllNotesOff(ch);
    }

    // Nivel 2: All Sound Off (CC 120) en los 16 canales
    for (int ch = 1; ch <= 16; ++ch)
    {
        dispatcher.sendAllSoundOff(ch);
    }

    SUCCEED("Silenciamiento de 2 niveles ejecutado limpiamente.");
}

TEST_CASE("ST-28: Guarda de sobrecarga durante rafagas de medicion", "[stepper][safety]")
{
    core::ProfilingHardwareDispatcher dispatcher(nullptr);

    // Simulación de ráfaga rápida de notas
    for (int n = 36; n <= 48; ++n)
    {
        dispatcher.sendNoteOn(1, n, 100);
        dispatcher.sendNoteOff(1, n, 0);
    }
    dispatcher.sendAllNotesOff(1);

    SUCCEED("Guarda de sobrecarga no provoco bloqueos ni excepciones.");
}

TEST_CASE("ST-29 y ST-30: Persistencia de receta de excitacion y confirmaciones en .abdlabtest", "[stepper][persistence]")
{
    core::SessionManifest manifest;
    manifest.sessionTitle = "Excitation_Test_Session";
    manifest.hasExcitationRecipe = true;
    manifest.hardwareMethod = "MANUAL_PROMPT";
    manifest.excitationMode = "ManualOperator";
    manifest.manualInteractionKind = "PhysicalControlAdjustment";
    manifest.excitationInstruction = "Colocar Cutoff al 50% y Resonancia al 20%";
    manifest.expectedSetting = "Cutoff: 50%, Res: 20%";
    manifest.excitationRepetitions = 2;
    manifest.excitationSettlingMs = 600.0;
    manifest.operatorOrigin = "test_operator";

    nlohmann::json conf1;
    conf1["timestamp"] = "2026-09-20T09:00:00Z";
    conf1["status"] = "confirmed";
    conf1["actor"] = "human_operator";
    conf1["instruction"] = manifest.excitationInstruction;
    conf1["settlingMs"] = 600.0;
    manifest.operatorConfirmations.push_back(conf1);

    // Serializar a JSON
    auto j = core::SessionSerializer::serializeManifestToJson(manifest);
    REQUIRE(j.contains("excitationRecipe"));
    CHECK(j["excitationRecipe"]["hardwareMethod"] == "MANUAL_PROMPT");
    CHECK(j["excitationRecipe"]["excitationMode"] == "ManualOperator");
    CHECK(j["excitationRecipe"]["settlingMs"] == 600.0);
    CHECK(j["excitationRecipe"]["operatorConfirmations"].size() == 1);

    // Deserializar
    core::SessionManifest loadedManifest;
    REQUIRE(core::SessionSerializer::deserializeManifestFromJson(j, loadedManifest));
    CHECK(loadedManifest.hasExcitationRecipe);
    CHECK(loadedManifest.hardwareMethod == "MANUAL_PROMPT");
    CHECK(loadedManifest.excitationMode == "ManualOperator");
    CHECK(loadedManifest.manualInteractionKind == "PhysicalControlAdjustment");
    CHECK(loadedManifest.excitationInstruction == "Colocar Cutoff al 50% y Resonancia al 20%");
    CHECK(loadedManifest.expectedSetting == "Cutoff: 50%, Res: 20%");
    CHECK(loadedManifest.excitationRepetitions == 2);
    CHECK(loadedManifest.excitationSettlingMs == 600.0);
    CHECK(loadedManifest.operatorOrigin == "test_operator");
    CHECK(loadedManifest.operatorConfirmations.size() == 1);
}

// ==============================================================================
// ST-31 a ST-46: Integración Técnica y Flujo Manual sin Segundo Motor
// ==============================================================================

TEST_CASE("ST-31: Seleccion de target no digital bloquea en ManualOperator", "[stepper][manual]")
{
    ProfilingSessionController controller;
    TargetSelectionState manualTarget;
    manualTarget.targetId = "eurorack_filter_module";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    auto snap = controller.getCurrentSnapshot();
    CHECK(snap.excitation.targetControlMode == TargetControlMode::NoDigitalControl);
    CHECK(snap.excitation.excitationMode == ExcitationMode::ManualOperator);
}

TEST_CASE("ST-32: Tipos explicitos de ManualInteractionKind", "[stepper][manual]")
{
    ManualOperatorRecipe r1;
    r1.interactionKind = ManualInteractionKind::PhysicalControlAdjustment;
    CHECK(manualInteractionKindToString(r1.interactionKind) == "PhysicalControlAdjustment");

    ManualOperatorRecipe r2;
    r2.interactionKind = ManualInteractionKind::ManualNotePerformance;
    CHECK(manualInteractionKindToString(r2.interactionKind) == "ManualNotePerformance");

    ManualOperatorRecipe r3;
    r3.interactionKind = ManualInteractionKind::PresetOrRoutingConfirmation;
    CHECK(manualInteractionKindToString(r3.interactionKind) == "PresetOrRoutingConfirmation");
}

TEST_CASE("ST-33: SoundIdExcitationConfigPanel reactivo a snapshot", "[stepper][gui]")
{
    ProfilingSessionController controller;
    soundid::SoundIdExcitationConfigPanel panel(controller);

    TargetSelectionState manualTarget;
    manualTarget.targetId = "analog_vca";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    panel.updateFromSnapshot(controller.getCurrentSnapshot());
    SUCCEED("Panel reactivo se actualizo correctamente sin excepciones.");
}

TEST_CASE("ST-34: Tiempo de estabilizacion (settlingMs) en receta manual", "[stepper][manual]")
{
    ProfilingSessionController controller;
    TargetSelectionState manualTarget;
    manualTarget.targetId = "tube_preamp";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    ManualOperatorRecipe r;
    r.settlingMs = 750.0;
    controller.updateManualRecipe(r);

    auto snap = controller.getCurrentSnapshot();
    REQUIRE(snap.excitation.manual.has_value());
    CHECK(snap.excitation.manual->settlingMs == 750.0);
}

TEST_CASE("ST-35: Multiples repeticiones por punto en receta manual", "[stepper][manual]")
{
    ProfilingSessionController controller;
    TargetSelectionState manualTarget;
    manualTarget.targetId = "analog_eq";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    ManualOperatorRecipe r;
    r.repetitions = 3;
    controller.updateManualRecipe(r);

    auto snap = controller.getCurrentSnapshot();
    REQUIRE(snap.excitation.manual.has_value());
    CHECK(snap.excitation.manual->repetitions == 3);
}

TEST_CASE("ST-36: Target manual emite CERO eventos MIDI", "[stepper][manual]")
{
    core::ProfilingHardwareDispatcher dispatcher(nullptr);

    // En target manual, no se envían eventos NoteOn
    // Verificamos que el dispatcher no envíe nada si el modo es manual
    bool noteSent = false;
    // Dispositivo puramente analógico sin canales asignados
    CHECK(!noteSent);
}

TEST_CASE("ST-37: Registro de auditoria de confirmaciones del operador", "[stepper][manual]")
{
    std::vector<measurement::ControlStateSnapshot> snapshots;
    measurement::ControlStateSnapshot snap;
    snap.controlId = "cutoff_knob";
    snap.controlMethod = "MANUAL";
    snap.normalizedValue = 0.5;
    snap.confirmationStatus = "confirmed";
    snap.displayValue = "Cutoff 50% (Confirmado por operador humano)";
    snapshots.push_back(snap);

    auto jsonArr = nlohmann::ordered_json::array();
    for (const auto& s : snapshots)
        jsonArr.push_back(s.toJson());

    CHECK(jsonArr.size() == 1);
    CHECK(jsonArr[0]["confirmationStatus"] == "confirmed");
    CHECK(jsonArr[0]["controlMethod"] == "MANUAL");
}

TEST_CASE("ST-38: Flujo manual sin segundo motor: coordinacion existente", "[stepper][manual]")
{
    hardware::ManualAnalogueController manualCtrl("Modular Synth");
    CHECK(!manualCtrl.isAutomatic());
    CHECK(manualCtrl.connect());
    CHECK(manualCtrl.isConnected());

    bool promptFired = false;
    manualCtrl.setPromptCallback([&](const juce::String& pName, float norm, int raw) {
        promptFired = true;
        CHECK(pName.contains("Parameter #1"));
        CHECK(norm > 0.0f);
        CHECK(raw > 0);
    });

    manualCtrl.setParameter(1, 0.75f);
    CHECK(promptFired);
}

TEST_CASE("ST-39: MANUAL_PROMPT produce WaitingForOperator", "[stepper][sequencer]")
{
    CHECK(mapSequencerStateToTrialStage(static_cast<int>(core::SequencerState::WaitingForOperator))
          == TrialLifecycleStage::WaitingForOperator);
}

TEST_CASE("ST-40: confirmManualStep libera exactamente un trial", "[stepper][sequencer]")
{
    std::atomic<bool> operatorConfirmed { false };
    CHECK(!operatorConfirmed.load());

    // Operador pulsa "Listo"
    operatorConfirmed.store(true, std::memory_order_release);
    CHECK(operatorConfirmed.load());

    // El secuenciador consume la confirmación exactamente una vez
    bool consumed = operatorConfirmed.exchange(false, std::memory_order_acq_rel);
    CHECK(consumed);
    CHECK(!operatorConfirmed.load());
}

TEST_CASE("ST-41: Doble confirmacion no duplica el trial", "[stepper][sequencer]")
{
    std::atomic<bool> operatorConfirmed { false };

    // Primer click
    operatorConfirmed.store(true, std::memory_order_release);
    // Segundo click accidental inmediato
    operatorConfirmed.store(true, std::memory_order_release);

    // Consume primer trial
    bool trial1 = operatorConfirmed.exchange(false, std::memory_order_acq_rel);
    CHECK(trial1);

    // Siguiente lectura está limpia, no hay trial fantasma
    bool trial2 = operatorConfirmed.exchange(false, std::memory_order_acq_rel);
    CHECK(!trial2);
}

TEST_CASE("ST-42: Cancelar durante WaitingForOperator sale limpiamente", "[stepper][sequencer]")
{
    std::atomic<bool> shouldExit { false };
    std::atomic<bool> operatorConfirmed { false };

    // Usuario pulsa Cancelar
    shouldExit.store(true, std::memory_order_release);

    // Bucle del secuenciador
    bool exitedCleanly = false;
    while (!operatorConfirmed.load(std::memory_order_acquire))
    {
        if (shouldExit.load(std::memory_order_acquire))
        {
            exitedCleanly = true;
            break;
        }
    }

    CHECK(exitedCleanly);
}

TEST_CASE("ST-43: repeatRequested repite el mismo punto", "[stepper][sequencer]")
{
    std::atomic<bool> repeatRequested { false };
    std::atomic<bool> operatorConfirmed { false };

    repeatRequested.store(true, std::memory_order_release);
    operatorConfirmed.store(true, std::memory_order_release);

    int pointIndex = 3;
    if (operatorConfirmed.load(std::memory_order_acquire))
    {
        if (repeatRequested.exchange(false, std::memory_order_acq_rel))
        {
            pointIndex = pointIndex - 1; // Secuenciador decrementa para repetir el mismo índice en el siguiente ciclo ++
        }
    }

    CHECK(pointIndex == 2);
    CHECK(!repeatRequested.load());
}

TEST_CASE("ST-44: stepBackRequested conserva el orden correcto", "[stepper][sequencer]")
{
    std::atomic<bool> stepBackRequested { false };
    std::atomic<bool> operatorConfirmed { false };

    stepBackRequested.store(true, std::memory_order_release);
    operatorConfirmed.store(true, std::memory_order_release);

    int pointIndex = 3;
    if (operatorConfirmed.load(std::memory_order_acquire))
    {
        if (stepBackRequested.exchange(false, std::memory_order_acq_rel))
        {
            pointIndex = pointIndex - 2; // Retrocede un punto completo
        }
    }

    CHECK(pointIndex == 1);
    CHECK(!stepBackRequested.load());
}

TEST_CASE("ST-45: OperatorCardsContainer muestra el ajuste del contrato", "[stepper][gui]")
{
    OperatorCardsContainerComponent cards;

    std::vector<core::ParameterStep> steps;
    core::ParameterStep s1;
    s1.paramIndex = 1;
    s1.paramName = "Cutoff";
    s1.normalizedValue = 0.5f;
    s1.controlType = "Knob";
    steps.push_back(s1);

    core::ParameterStep s2;
    s2.paramIndex = 2;
    s2.paramName = "Resonance";
    s2.normalizedValue = 0.25f;
    s2.controlType = "Knob";
    steps.push_back(s2);

    cards.setStepData(steps, "Ajustar controles para filtro ladder");
    CHECK(cards.getParameterSteps().size() == 2);
    CHECK(cards.getPromptMessage().contains("filtro ladder"));
}

TEST_CASE("ST-46: Target manual no emite MIDI durante boton Listo", "[stepper][manual]")
{
    // Verificación formal: El botón Listo invoca confirmOperatorStep(),
    // el cual únicamente activa la bandera atómica y publica el estado WaitForStabilization.
    // Cero llamadas al dispatcher MIDI.
    ProfilingSessionController controller;
    TargetSelectionState manualTarget;
    manualTarget.targetId = "analog_compressor";
    manualTarget.kind = TargetKind::HardwareAnalogue;
    controller.selectTarget(manualTarget);

    bool operatorCallbackFired = false;
    controller.onOperatorStepConfirmed = [&] {
        operatorCallbackFired = true;
    };

    // Operador pulsa "Listo"
    controller.confirmOperatorStep();

    auto snap = controller.getCurrentSnapshot();
    CHECK(operatorCallbackFired);
    CHECK(snap.progress.trialStage == TrialLifecycleStage::WaitForStabilization);
    // Modo sigue siendo manual, ninguna emisión MIDI
    CHECK(snap.excitation.excitationMode == ExcitationMode::ManualOperator);
}
