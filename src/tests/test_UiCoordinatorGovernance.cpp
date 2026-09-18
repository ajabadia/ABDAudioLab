#include <catch2/catch_test_macros.hpp>
#include "../measurement/CoordinatorStateMachine.h"
#include "../gui/SessionExecutionCoordinator.h"
#include "../gui/OperatorStepModalDialog.h"
#include "../core/ProfilingSequencer.h"
#include "../core/SessionManager.h"
#include "../gui/SoundIdCurvePlotter.h"
#include "../audio/LabAudioEngine.h"
#include "../hardware/MockHardwareController.h"

using namespace abdaudiolab;
using namespace abdaudiolab::measurement;

TEST_CASE("UI Governance: Interaction Mode Switching & Execution Guards", "[ui_governance]")
{
    CoordinatorStateMachine sm;
    CoordinatorContext ctx;
    ctx.mode = WorkspaceInteractionMode::Guided;
    ctx.hasActiveSession = true;
    ctx.profileSha256Verified = true;
    ctx.stimulusReady = true;

    // 1. Initial passive states allow switching mode
    REQUIRE(sm.isModeChangeAllowed());
    sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_1");
    REQUIRE(sm.isModeChangeAllowed());
    sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_1");
    REQUIRE(sm.isModeChangeAllowed());

    // 2. Active execution states strictly block mode change
    ctx.isManualControlRequired = true;
    sm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx, "sess_gov_1");
    sm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::Capturing);

    // During Capturing, mode change is prohibited
    REQUIRE_FALSE(sm.isModeChangeAllowed());
    std::string reason = sm.getRejectionReasonForAction("change_mode", ctx);
    REQUIRE(reason == "Cambiar modo: no disponible durante la captura o validacion activa");

    // Advance to Validation & Persistence
    ctx.rawSha256Verified = true;
    sm.dispatch(CoordinatorEvent::CaptureFinished, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::Validating);
    REQUIRE_FALSE(sm.isModeChangeAllowed());

    sm.dispatch(CoordinatorEvent::ValidationPassed, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::Persisting);
    REQUIRE_FALSE(sm.isModeChangeAllowed());

    sm.dispatch(CoordinatorEvent::PersistenceSucceeded, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::PointCompleted);
    // Point completed: passive again, mode change is allowed
    REQUIRE(sm.isModeChangeAllowed());
}

TEST_CASE("UI Governance: Blocked Action Explanations & Tooltip Text", "[ui_governance]")
{
    CoordinatorStateMachine sm;
    CoordinatorContext ctx;

    SECTION("Confirm manual action reasons")
    {
        // No session: confirm blocked
        REQUIRE_FALSE(sm.isManualConfirmationAllowed());
        std::string r1 = sm.getRejectionReasonForAction("confirm", ctx);
        REQUIRE(r1 == "Confirmar: no hay paso manual pendiente");

        // During capturing
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_2");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_2");
        ctx.isManualControlRequired = true;
        sm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx, "sess_gov_2");
        sm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "sess_gov_2");
        REQUIRE(sm.getState() == CoordinatorState::Capturing);

        std::string r2 = sm.getRejectionReasonForAction("confirm", ctx);
        REQUIRE(r2 == "Confirmar: no se puede confirmar mientras la grabacion esta activa");
    }

    SECTION("Capture action reasons")
    {
        // Stimulus not ready
        ctx.stimulusReady = false;
        std::string r1 = sm.getRejectionReasonForAction("capture", ctx);
        REQUIRE(r1 == "Capturar: prepara primero el estimulo");

        // During capturing
        ctx.stimulusReady = true;
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_3");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_3");
        ctx.mode = WorkspaceInteractionMode::Free;
        sm.dispatch(CoordinatorEvent::CaptureStarted, ctx, "sess_gov_3");
        REQUIRE(sm.getState() == CoordinatorState::Capturing);

        std::string r2 = sm.getRejectionReasonForAction("capture", ctx);
        REQUIRE(r2 == "Capturar: grabacion en curso");
    }

    SECTION("Profile change action reasons")
    {
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_4");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_4");
        ctx.isManualControlRequired = true;
        sm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx, "sess_gov_4");
        sm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "sess_gov_4");

        REQUIRE_FALSE(sm.isProfileChangeAllowed());
        std::string r = sm.getRejectionReasonForAction("change_profile", ctx);
        REQUIRE(r == "Cambiar perfil: no disponible durante la grabacion o medicion activa");
    }

    SECTION("Cancellation reasons")
    {
        // No session: cancellation blocked
        REQUIRE_FALSE(sm.isCancellationAllowed());
        std::string r1 = sm.getRejectionReasonForAction("cancel", ctx);
        REQUIRE(r1 == "Cancelar: no hay sesion activa que cancelar");

        // Session ready: cancellation allowed
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_5");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_5");
        REQUIRE(sm.isCancellationAllowed());
        std::string r2 = sm.getRejectionReasonForAction("cancel", ctx);
        REQUIRE(r2.empty());
    }
}

TEST_CASE("UI Governance: Free Mode Rigorous Unknown Control Semantics", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    core::HardwareContract contract;
    contract.id = "analogue_fuzz_pedal";
    contract.displayName = "Classic Fuzz Box";
    contract.deviceType = "ANALOGUE_PEDAL";

    coordinator.initializeMeasurementSession(contract, "fuzz_core", "a1b2c3d4e5f6");
    coordinator.setWorkspaceInteractionMode(WorkspaceInteractionMode::Free);

    SECTION("Ad-hoc capture without specifying controls produces unknown without inventing data")
    {
        // Operator clicks "Capturar toma libre" without declaring knob positions
        coordinator.triggerFreeCapture({});

        REQUIRE(coordinator.getCoordinatorState() == CoordinatorState::Capturing);
        const auto* sess = coordinator.getActiveMeasurementSession();
        REQUIRE(sess != nullptr);
        REQUIRE_FALSE(sess->controlStates.empty());

        const auto& snap = sess->controlStates.back();
        REQUIRE(snap.confirmationStatus == "unknown");
        REQUIRE(snap.displayValue == "Posición no declarada");
        REQUIRE_FALSE(snap.normalizedValue.has_value());
    }

    SECTION("Ad-hoc capture with operator declared controls preserves exact values")
    {
        ControlStateSnapshot knobSnap;
        knobSnap.controlId = "FUZZ_GAIN";
        knobSnap.confirmationStatus = "user_supplied";
        knobSnap.displayValue = "7.5 / 10.0";
        knobSnap.normalizedValue = 0.75f;

        coordinator.triggerFreeCapture({ knobSnap });

        const auto* sess = coordinator.getActiveMeasurementSession();
        REQUIRE(sess != nullptr);
        REQUIRE_FALSE(sess->controlStates.empty());

        const auto& snap = sess->controlStates.back();
        REQUIRE(snap.controlId == "FUZZ_GAIN");
        REQUIRE(snap.confirmationStatus == "user_supplied");
        REQUIRE(snap.displayValue == "7.5 / 10.0");
        REQUIRE(snap.normalizedValue.has_value());
        REQUIRE(*snap.normalizedValue == 0.75f);
    }
}

TEST_CASE("UI Governance: Keyboard Handshake & Modal Focus Guards", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    gui::OperatorStepModalDialog modal;
    modal.showStepPrompt(nullptr, "Alignment Step", 1, 5, {});

    bool acceptedFired = false;
    bool cancelFired = false;
    modal.onAccept = [&] { acceptedFired = true; };
    modal.onCancel = [&] { cancelFired = true; };

    SECTION("Space key does not confirm while measurement is active")
    {
        modal.setMeasuringState(true);
        juce::KeyPress space(juce::KeyPress::spaceKey);
        bool handled = modal.keyPressed(space, &modal);

        REQUIRE_FALSE(handled);
        REQUIRE_FALSE(acceptedFired);
    }

    SECTION("Space key confirms when awaiting manual confirmation and modal is idle")
    {
        modal.setMeasuringState(false);
        modal.setAutomatedMode(false);

        juce::KeyPress space(juce::KeyPress::spaceKey);
        bool handled = modal.keyPressed(space, &modal);

        REQUIRE(handled);
        REQUIRE(acceptedFired);
    }

    SECTION("Escape key triggers safe cancellation")
    {
        modal.setMeasuringState(false);
        juce::KeyPress esc(juce::KeyPress::escapeKey);
        bool handled = modal.keyPressed(esc, &modal);

        REQUIRE(handled);
        REQUIRE(cancelFired);
    }
}
