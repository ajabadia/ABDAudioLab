/**
 * @file test_CoordinatorStateMachine.cpp
 * @brief Pure unit test suite for CoordinatorStateMachine:
 *        Guided vs Free workflows, Guard validation, Transition rejections,
 *        Transactional cancellation, Idempotency, and Audit logging.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "measurement/CoordinatorStateMachine.h"

using namespace abdaudiolab::measurement;

TEST_CASE("CoordinatorStateMachine - Complete Guided Workflow", "[coordinator_state][guided]")
{
    CoordinatorStateMachine fsm;
    REQUIRE(fsm.getState() == CoordinatorState::NoSession);

    CoordinatorContext ctx;
    ctx.mode = WorkspaceInteractionMode::Guided;
    ctx.actor = "operator";

    // 1. SelectProfile sin verificar hash -> RECHAZADO
    ctx.profileSha256Verified = false;
    bool ok = fsm.dispatch(CoordinatorEvent::SelectProfile, ctx, "test_session_1");
    REQUIRE_FALSE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::NoSession);

    // 2. SelectProfile verificado -> ACEPTADO
    ctx.profileSha256Verified = true;
    ok = fsm.dispatch(CoordinatorEvent::SelectProfile, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::ProfileSelected);
    REQUIRE(fsm.isProfileChangeAllowed());

    // 3. PrepareSession sin sesión creada -> RECHAZADO
    ctx.hasActiveSession = false;
    ok = fsm.dispatch(CoordinatorEvent::PrepareSession, ctx, "test_session_1");
    REQUIRE_FALSE(ok);

    // 4. PrepareSession con sesión activa -> ACEPTADO
    ctx.hasActiveSession = true;
    ok = fsm.dispatch(CoordinatorEvent::PrepareSession, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::SessionReady);
    REQUIRE_FALSE(fsm.isProfileChangeAllowed());

    // 5. Prompt Manual
    ctx.isManualControlRequired = true;
    ok = fsm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::AwaitingManualConfirmation);
    REQUIRE(fsm.isManualConfirmationAllowed());
    REQUIRE(fsm.isCancellationAllowed());

    // 6. Confirmación mientras capturando -> RECHAZADO por guarda
    ctx.isCapturingActive = true;
    ok = fsm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "test_session_1");
    REQUIRE_FALSE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::AwaitingManualConfirmation);

    // Confirmación válida
    ctx.isCapturingActive = false;
    ok = fsm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Capturing);
    REQUIRE_FALSE(fsm.isManualConfirmationAllowed());

    // 7. Fin de Captura -> Validando
    ok = fsm.dispatch(CoordinatorEvent::CaptureFinished, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Validating);

    // 8. Validación Aprobada -> Persistiendo
    ok = fsm.dispatch(CoordinatorEvent::ValidationPassed, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Persisting);

    // 9. Persistencia sin hash verificado de disco -> RECHAZADO por guarda
    ctx.rawSha256Verified = false;
    ok = fsm.dispatch(CoordinatorEvent::PersistenceSucceeded, ctx, "test_session_1");
    REQUIRE_FALSE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Persisting);

    // Persistencia con hash verificado
    ctx.rawSha256Verified = true;
    ok = fsm.dispatch(CoordinatorEvent::PersistenceSucceeded, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::PointCompleted);

    // 10. Siguiente Punto (Fin de la campaña)
    ctx.hasRemainingPoints = false;
    ok = fsm.dispatch(CoordinatorEvent::NextPointOrFinish, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::SessionCompleted);
    REQUIRE(fsm.isReanalysisAllowed());

    // 11. Reanálisis Histórico
    ctx.allTakesValidForReanalysis = true;
    ok = fsm.dispatch(CoordinatorEvent::ReanalysisRequested, ctx, "test_session_1");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::ReanalysisAvailable);
}

TEST_CASE("CoordinatorStateMachine - Automation Flow and Failure Handling", "[coordinator_state][automation]")
{
    CoordinatorStateMachine fsm;
    CoordinatorContext ctx;
    ctx.profileSha256Verified = true;
    ctx.hasActiveSession = true;
    ctx.isManualControlRequired = false; // Parámetro automatizado MIDI/VST3

    fsm.dispatch(CoordinatorEvent::SelectProfile, ctx);
    fsm.dispatch(CoordinatorEvent::PrepareSession, ctx);
    REQUIRE(fsm.getState() == CoordinatorState::SessionReady);

    // Intento de AwaitingManualPrompt cuando no es manual -> RECHAZADO
    bool ok = fsm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx);
    REQUIRE_FALSE(ok);

    // Iniciar Automatización
    ok = fsm.dispatch(CoordinatorEvent::ApplyAutomation, ctx);
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::ApplyingAutomation);

    // Fallo de transporte MIDI / VST3
    ok = fsm.dispatch(CoordinatorEvent::AutomationFailed, ctx);
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Error);
}

TEST_CASE("CoordinatorStateMachine - Free Mode Direct Capture", "[coordinator_state][free_mode]")
{
    CoordinatorStateMachine fsm;
    CoordinatorContext ctx;
    ctx.mode = WorkspaceInteractionMode::Free;
    ctx.profileSha256Verified = true;
    ctx.hasActiveSession = true;

    fsm.dispatch(CoordinatorEvent::SelectProfile, ctx);
    fsm.dispatch(CoordinatorEvent::PrepareSession, ctx);
    REQUIRE(fsm.getState() == CoordinatorState::SessionReady);

    // Captura directa sin estímulo listo -> RECHAZADO
    ctx.stimulusReady = false;
    bool ok = fsm.dispatch(CoordinatorEvent::CaptureStarted, ctx);
    REQUIRE_FALSE(ok);

    // Captura directa con estímulo listo -> ACEPTADO
    ctx.stimulusReady = true;
    ok = fsm.dispatch(CoordinatorEvent::CaptureStarted, ctx);
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Capturing);

    // Completar toma en modo Free
    fsm.dispatch(CoordinatorEvent::CaptureFinished, ctx);
    fsm.dispatch(CoordinatorEvent::ValidationPassed, ctx);
    ctx.rawSha256Verified = true;
    fsm.dispatch(CoordinatorEvent::PersistenceSucceeded, ctx);
    REQUIRE(fsm.getState() == CoordinatorState::PointCompleted);

    // Próxima acción en Free mode vuelve a SessionReady para permitir nuevas tomas ad-hoc
    ctx.hasRemainingPoints = true;
    ok = fsm.dispatch(CoordinatorEvent::NextPointOrFinish, ctx);
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::SessionReady);
}

TEST_CASE("CoordinatorStateMachine - Transactional Cancellation and Idempotency", "[coordinator_state][cancellation]")
{
    CoordinatorStateMachine fsm;
    CoordinatorContext ctx;
    ctx.profileSha256Verified = true;
    ctx.hasActiveSession = true;
    ctx.isManualControlRequired = true;

    fsm.dispatch(CoordinatorEvent::SelectProfile, ctx);
    fsm.dispatch(CoordinatorEvent::PrepareSession, ctx);
    fsm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx);
    REQUIRE(fsm.getState() == CoordinatorState::AwaitingManualConfirmation);

    // Cancelar durante prompt manual
    bool ok = fsm.dispatch(CoordinatorEvent::CancelRequested, ctx, "session_cancel_test");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Aborted);

    // Cancelar por segunda vez es idempotente y seguro
    ok = fsm.dispatch(CoordinatorEvent::CancelRequested, ctx, "session_cancel_test");
    REQUIRE(ok);
    REQUIRE(fsm.getState() == CoordinatorState::Aborted);

    // Verificar que el historial de auditoría registró ambas transiciones
    const auto& history = fsm.getTransitionHistory();
    REQUIRE(history.size() >= 4);
    REQUIRE(history.back().toState == "Aborted");
}
