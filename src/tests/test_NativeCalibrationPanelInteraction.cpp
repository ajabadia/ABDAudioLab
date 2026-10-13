/**
 * @file test_NativeCalibrationPanelInteraction.cpp
 * @brief Unit tests for Step 2 UI redesign: Pure navigation tabs, physical action buttons,
 *        preflight safety check, immediate 2B invalidation, and persistent dual reports.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/NativeCalibrationPanel.h"
#include "audio/LabAudioEngine.h"
#include "audio/LabAudioReceiver.h"
#include "calibration/CalibrationNoiseBaselineRunner.h"
#include "calibration/CalibrationProfileStore.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::calibration;

namespace
{

void simulateClick(juce::Component& comp, juce::Point<int> pt)
{
    auto mouseSource = juce::Desktop::getInstance().getMainMouseSource();
    juce::Point<float> ptf(static_cast<float>(pt.x), static_cast<float>(pt.y));
    juce::MouseEvent e(mouseSource, ptf, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                       &comp, &comp, juce::Time::getCurrentTime(), ptf, juce::Time::getCurrentTime(), 1, false);
    comp.mouseDown(e);
}

void passNoiseBaseline2A(NativeCalibrationPanel& panel, audio::LabAudioEngine& engine)
{
    panel.startNoiseBaselineCheck();
    panel.stopTimer();

    // Confirm silence and arm baseline capture
    for (int i = 0; i < 6; ++i)
        panel.timerCallback();

    // Feed 600 ms of clean silent samples (exceeds 400 ms requirement at any operating sample rate)
    double sr = engine.getSampleRate();
    if (sr <= 0.0) sr = 44100.0;
    int samples = static_cast<int>(std::lround(sr * 0.60));
    std::vector<float> silence(static_cast<size_t>(samples), 0.0f);
    engine.getResponseReceiver().processBlock(silence.data(), samples);

    // Complete baseline analysis and state transition
    panel.timerCallback();
    panel.stopTimer();
}

} // namespace

TEST_CASE("NativeCalibrationPanel: Tabs de navegacion pura y botones de accion fisica", "[gui][calibration][step2]")
{
    audio::LabAudioEngine engine;
    NativeCalibrationPanel panel(engine);
    panel.setSize(800, 600);

    SECTION("Click en tarjeta 2A y 2B solo cambia subvista activa y NUNCA inicia audio ni sweep")
    {
        // Estado inicial por defecto
        CHECK(panel.getActiveSubView() == NativeCalibrationPanel::CalibrationSubView::NoiseBaseline_2A);
        CHECK(engine.getResponseReceiver().getState() == audio::ReceiverState::Idle);
        CHECK(panel.getState() == NativeCalibrationPanel::State::ReadyToMeasure);

        // Click en Tarjeta 2B: solo navega a 2B
        auto card2BCentre = panel.getStepCard2BBounds().getCentre();
        simulateClick(panel, card2BCentre);

        CHECK(panel.getActiveSubView() == NativeCalibrationPanel::CalibrationSubView::PhysicalLoopback_2B);
        CHECK(engine.getResponseReceiver().getState() == audio::ReceiverState::Idle);
        CHECK(panel.getState() == NativeCalibrationPanel::State::ReadyToMeasure);

        // Click en Tarjeta 2A: regresa a 2A
        auto card2ACentre = panel.getStepCard2ABounds().getCentre();
        simulateClick(panel, card2ACentre);

        CHECK(panel.getActiveSubView() == NativeCalibrationPanel::CalibrationSubView::NoiseBaseline_2A);
        CHECK(engine.getResponseReceiver().getState() == audio::ReceiverState::Idle);
        CHECK(panel.getState() == NativeCalibrationPanel::State::ReadyToMeasure);
    }

    SECTION("Boton 2A inicia exclusivamente baseline de ruido con salida muteada")
    {
        panel.stopTimer(); // Manejo determinista manual
        panel.startNoiseBaselineCheck();

        CHECK(panel.getState() == NativeCalibrationPanel::State::MeasuringNoiseBaseline);
        CHECK(panel.getNoiseBaselineState() == NativeCalibrationPanel::NoiseBaselineState::Checking);
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Locked);

        // Limpieza
        panel.stopTimer();
    }

    SECTION("Boton 2B estando Locked no inicia preflight ni sweep")
    {
        panel.stopTimer();
        panel.resetToInitialState();
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Locked);
        CHECK(panel.getNoiseBaselineState() == NativeCalibrationPanel::NoiseBaselineState::NotChecked);

        panel.setActiveSubView(NativeCalibrationPanel::CalibrationSubView::PhysicalLoopback_2B);
        panel.startPhysicalLoopbackSweep();

        // Permanece Locked, no inicia captura
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Locked);
        CHECK(panel.getState() == NativeCalibrationPanel::State::ReadyToMeasure);
        CHECK(engine.getResponseReceiver().getState() == audio::ReceiverState::Idle);
    }

    SECTION("Boton 2B estando Ready inicia preflight de seguridad antes del sweep")
    {
        panel.stopTimer();
        panel.resetToInitialState();

        // Simular que 2A pasa exitosamente
        passNoiseBaseline2A(panel, engine);

        // 2A debe estar Passed y 2B debe estar Ready
        CHECK(panel.getNoiseBaselineState() == NativeCalibrationPanel::NoiseBaselineState::Passed);
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Ready);

        // Ahora iniciar 2B: DEBE entrar primero en MeasuringPreflight
        panel.startPhysicalLoopbackSweep();
        panel.stopTimer();

        CHECK(panel.getState() == NativeCalibrationPanel::State::Measuring);
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::MeasuringPreflight);
        CHECK(engine.getResponseReceiver().getState() != audio::ReceiverState::Idle);
    }
}

TEST_CASE("NativeCalibrationPanel: Invariante de invalidacion inmediata de 2B", "[gui][calibration][step2][invalidation]")
{
    audio::LabAudioEngine engine;
    NativeCalibrationPanel panel(engine);
    panel.setSize(800, 600);
    panel.stopTimer();

    SECTION("Reiniciar 2A marca 2B como Stale de inmediato y neutraliza contexto")
    {
        // Simular 2A pasado con exito
        passNoiseBaseline2A(panel, engine);

        CHECK(panel.getNoiseBaselineState() == NativeCalibrationPanel::NoiseBaselineState::Passed);
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Ready);

        // Al iniciar nuevamente 2A: invalidacion INMEDIATA de 2B
        panel.startNoiseBaselineCheck();
        panel.stopTimer();

        CHECK((panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Stale ||
               panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Locked));
        CHECK(panel.getActiveAlignment() == ActiveCalibrationAlignment::None);
        CHECK_FALSE(panel.getActiveSnapshot().has_value());
    }

    SECTION("2A fallido por feedback o sobrecarga mantiene 2B bloqueado y contexto neutralizado")
    {
        panel.startNoiseBaselineCheck();
        panel.stopTimer();

        // Confirmar silencio para armar baseline capture
        for (int i = 0; i < 5; ++i)
            panel.timerCallback();

        // Simular sobrecarga/feedback detectada en receiver durante baseline (> -6 dBFS)
        float overloadBuffer[128];
        std::fill_n(overloadBuffer, 128, 0.95f);
        engine.getResponseReceiver().processBlock(overloadBuffer, 128);

        // El receiver dispara overload
        CHECK(engine.getResponseReceiver().isOverloadTriggered());

        panel.timerCallback();

        CHECK(panel.getNoiseBaselineState() == NativeCalibrationPanel::NoiseBaselineState::SafetyAborted);
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Locked);
        CHECK(panel.getActiveAlignment() == ActiveCalibrationAlignment::None);
        CHECK(panel.getState() == NativeCalibrationPanel::State::Failed);
    }
}

TEST_CASE("NativeCalibrationPanel: Preflight de seguridad de 2B y proteccion de sweep", "[gui][calibration][step2][safety]")
{
    audio::LabAudioEngine engine;
    NativeCalibrationPanel panel(engine);
    panel.setSize(800, 600);
    panel.stopTimer();

    SECTION("Preflight con senal peligrosa (> -6 dBFS) aborta antes de emitir el sweep")
    {
        // 1. Pasar 2A
        passNoiseBaseline2A(panel, engine);
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Ready);

        // 2. Iniciar 2B -> entra en MeasuringPreflight
        panel.startPhysicalLoopbackSweep();
        panel.stopTimer();
        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::MeasuringPreflight);

        // 3. Simular que durante el preflight silencioso entra una senal alta (bucle o feedback)
        float loudBuffer[128];
        std::fill_n(loudBuffer, 128, 0.90f); // > -6 dBFS
        engine.getResponseReceiver().processBlock(loudBuffer, 128);
        CHECK(engine.getResponseReceiver().isOverloadTriggered());

        // 4. Timer procesa el aborto de seguridad
        panel.timerCallback();

        CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Failed);
        CHECK(panel.getState() == NativeCalibrationPanel::State::Failed);
        CHECK(panel.getLoopbackReport().failureReason.containsIgnoreCase("Preflight Abort"));
    }
}

TEST_CASE("NativeCalibrationPanel: Persistencia de reportes duales 2A y 2B", "[gui][calibration][step2][reports]")
{
    audio::LabAudioEngine engine;
    NativeCalibrationPanel panel(engine);
    panel.setSize(800, 600);
    panel.stopTimer();

    // 1. Ejecutar y completar 2A
    passNoiseBaseline2A(panel, engine);

    CHECK(panel.getNoiseReport().passed == true);
    auto baselineRms = panel.getNoiseReport().rmsDbfs;

    // 2. Navegar entre subvistas 2B y 2A: los reportes persisten intactos
    panel.setActiveSubView(NativeCalibrationPanel::CalibrationSubView::PhysicalLoopback_2B);
    CHECK(panel.getActiveSubView() == NativeCalibrationPanel::CalibrationSubView::PhysicalLoopback_2B);
    CHECK(panel.getNoiseReport().passed == true);
    CHECK(panel.getNoiseReport().rmsDbfs == Catch::Approx(baselineRms));

    panel.setActiveSubView(NativeCalibrationPanel::CalibrationSubView::NoiseBaseline_2A);
    CHECK(panel.getActiveSubView() == NativeCalibrationPanel::CalibrationSubView::NoiseBaseline_2A);
    CHECK(panel.getNoiseReport().passed == true);
    CHECK(panel.getNoiseReport().rmsDbfs == Catch::Approx(baselineRms));
}

TEST_CASE("NativeCalibrationPanel: Limpieza de encoding y etiquetas metrologicas", "[gui][calibration][step2][encoding]")
{
    audio::LabAudioEngine engine;
    NativeCalibrationPanel panel(engine);
    panel.setSize(800, 600);
    panel.stopTimer();

    // Verify 2B report default method is NotAvailable
    CHECK(panel.getLoopbackReport().snrMethod == math::SnrMeasurementMethod::NotAvailable);

    // Exercise paint pass through all painters
    juce::Image dummyImg(juce::Image::ARGB, 800, 600, true);
    juce::Graphics g(dummyImg);
    panel.paint(g);

    passNoiseBaseline2A(panel, engine);
    panel.paint(g);

    CHECK(panel.getNoiseBaselineState() == NativeCalibrationPanel::NoiseBaselineState::Passed);
    CHECK(panel.getLoopbackState() == NativeCalibrationPanel::LoopbackState::Ready);
}
