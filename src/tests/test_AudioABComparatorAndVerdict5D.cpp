/**
 * @file test_AudioABComparatorAndVerdict5D.cpp
 * @brief 5D.7 — Suite hermética de fixtures para AudioABComparator / AudioABVerdictEngine.
 *
 * Ejercita el pipeline completo:
 *   Fixture → AudioABMetricsEvaluator → AudioABEvaluationResult
 *          → AudioABTolerancePolicy5D → AudioABToleranceVerdict
 *
 * RESTRICCIONES:
 *   - Cero I/O físico.
 *   - Cero MIDI (ni JuceMidi, ni LoopBe1).
 *   - Cero VES.
 *   - Cero ExportReadiness.
 *   - Cero hardware / ASIO / WASAPI.
 *   - Todos los buffers son std::vector<float> o juce::AudioBuffer<float> en memoria.
 *   - Seeds deterministas; resultados reproducibles.
 *
 * Tags: [audioab_5d][comparator][verdict][fixture][hermetic]
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../math/AudioABMetrics5D.h"
#include "../math/AudioABTolerancePolicy5D.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <numbers>
#include <vector>
#include <cstring>

using namespace abdaudiolab::math::qa5d;

// =============================================================================
// Helpers de fixtures
// =============================================================================

namespace {

/// Crea un buffer de 1 canal con silence en [0, warmupSamples) y onda senoidal de 440 Hz
/// desde warmupSamples en adelante. Por defecto warmup = 100 muestras.
juce::AudioBuffer<float> makeSine440(int numSamples, double sr = 48000.0, int warmupSamples = 100)
{
    juce::AudioBuffer<float> buf(1, numSamples);
    buf.clear(); // [0..warmupSamples-1] = 0.0f estricto
    float* p = buf.getWritePointer(0);
    for (int i = warmupSamples; i < numSamples; ++i)
        p[i] = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * static_cast<float>(i) / static_cast<float>(sr));
    return buf;
}

/// Copia `src` en `dst` con un offset positivo de `shift` samples (B = A desplazado).
juce::AudioBuffer<float> shiftBuffer(const juce::AudioBuffer<float>& src, int shift)
{
    const int n = src.getNumSamples();
    juce::AudioBuffer<float> dst(1, n);
    dst.clear();
    float* d = dst.getWritePointer(0);
    const float* s = src.getReadPointer(0);
    for (int i = 0; i < n; ++i)
    {
        const int srcIdx = i - shift;
        if (srcIdx >= 0 && srcIdx < n)
            d[i] = s[srcIdx];
    }
    return dst;
}

/// Verifica que dos buffers son idénticos muestra a muestra (no mutación).
bool buffersIdentical(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return false;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (a.getSample(ch, i) != b.getSample(ch, i))
                return false;
    return true;
}

/// Snapshot de un buffer (copia independiente para comprobación de no mutación).
juce::AudioBuffer<float> snapshot(const juce::AudioBuffer<float>& src)
{
    juce::AudioBuffer<float> copy(src.getNumChannels(), src.getNumSamples());
    for (int ch = 0; ch < src.getNumChannels(); ++ch)
        std::memcpy(copy.getWritePointer(ch), src.getReadPointer(ch),
                    static_cast<size_t>(src.getNumSamples()) * sizeof(float));
    return copy;
}

} // anonymous namespace


// =============================================================================
// FIXTURE 1 — FixtureBitExactIdentity
// A == B: correlationSigned = +1.0, correlationAbsolute = 1.0, lag = 0
// Resultado esperado: PASS / Accepted / DIAG_OK_IDENTITY
// =============================================================================
TEST_CASE("5D.7 Fixture1 - FixtureBitExactIdentity: PASS + DIAG_OK_IDENTITY",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800; // 100 ms a 48 kHz
    auto bufA = makeSine440(N);
    auto bufB = makeSine440(N); // copia exacta

    auto snapA = snapshot(bufA);
    auto snapB = snapshot(bufB);

    auto res = AudioABMetricsEvaluator::evaluate(
        "F1_IDENTITY", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        /*warmup*/100, /*noteOn*/100, /*noteOff*/4700, /*expectedGate*/4600,
        /*lag*/0, /*corrSigned*/+1.0f, /*spectralDelta*/0.0f, /*intraIdentical*/true);

    // --- Métricas temporales ---
    CHECK(res.temporal.alignmentLagSamples == 0);
    CHECK(res.temporal.correlationSigned == Catch::Approx(+1.0f).epsilon(1e-4));
    CHECK(res.temporal.correlationAbsolute == Catch::Approx(1.0f).epsilon(1e-4));
    // Invariante: correlationAbsolute == |correlationSigned|
    CHECK(res.temporal.correlationAbsolute == Catch::Approx(std::abs(res.temporal.correlationSigned)).epsilon(1e-6f));

    // --- Diagnóstico ---
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING));

    // --- Veredicto ---
    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isPass());
    CHECK(verdict.isAccepted());
    CHECK(verdict.reasonCode == "PASS_ALL_CRITERIA");

    // --- Serialización JSON ---
    auto j = verdict.toJson();
    CHECK(j["verdictLevel"] == "PASS");
    CHECK(j["acceptanceDisposition"] == "Accepted");

    auto jr = res.toJson();
    CHECK(jr["temporal"]["correlationSigned"].get<float>() == Catch::Approx(+1.0f).epsilon(1e-4));
    CHECK(jr["temporal"]["correlationAbsolute"].get<float>() == Catch::Approx(1.0f).epsilon(1e-4));

    // --- No mutación de buffers ---
    CHECK(buffersIdentical(bufA, snapA));
    CHECK(buffersIdentical(bufB, snapB));
}


// =============================================================================
// FIXTURE 2 — FixtureKnownLag
// B = A desplazado −3 samples; clase AggressiveNonlinear
// Resultado esperado: WARN / AcceptableWithExpectedDispersion / DIAG_OK_CLASS_TOLERANCE
// (El evaluador detecta dispersión armónica/fase por lag y delta espectral; la policy escala a WARN)
// =============================================================================
TEST_CASE("5D.7 Fixture2 - FixtureKnownLag: WARN + DIAG_OK_CLASS_TOLERANCE (lag -3 visible)",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA    = makeSine440(N);
    auto bufB    = shiftBuffer(bufA, 3); // desplazado +3 (lag -3 desde perspectiva de A)

    auto snapA = snapshot(bufA);
    auto snapB = snapshot(bufB);

    // Lag registrado fielmente: −3. Los buffers originales NO se modifican.
    auto res = AudioABMetricsEvaluator::evaluate(
        "F2_KNOWN_LAG", "PRESET_AGGR_NONLIN_03", "AggressiveNonlinear", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/-3, /*corrSigned*/+0.9894f, /*spectralDelta*/0.39f, /*intraIdentical*/true);

    // El comparador mide el desplazamiento pero no reescribe A o B
    CHECK(res.temporal.alignmentLagSamples == -3);
    CHECK(res.temporal.correlationSigned == Catch::Approx(+0.9894f).epsilon(1e-3f));
    CHECK(res.temporal.correlationAbsolute == Catch::Approx(std::abs(res.temporal.correlationSigned)).epsilon(1e-6f));

    // No es identidad exacta (lag != 0)
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_OK_CLASS_TOLERANCE));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isWarn());
    CHECK(verdict.isAcceptableWithDispersion());
    CHECK(verdict.reasonCode == "WARN_TOLERANCE_RANGE");
    CHECK_FALSE(verdict.isFail());
    CHECK_FALSE(verdict.isRejected());

    // No mutación
    CHECK(buffersIdentical(bufA, snapA));
    CHECK(buffersIdentical(bufB, snapB));
}


// =============================================================================
// FIXTURE 3 — FixtureLagOutOfBounds
// B = A desplazado +135 samples → lag > 128 → Hard FAIL
// Resultado: FAIL / Rejected / DIAG_FAIL_TEMPORAL_DESYNC
// =============================================================================
TEST_CASE("5D.7 Fixture3 - FixtureLagOutOfBounds: Hard FAIL / DIAG_FAIL_TEMPORAL_DESYNC",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 9600;
    auto bufA = makeSine440(N);
    auto bufB = shiftBuffer(bufA, 135);

    auto snapA = snapshot(bufA);
    auto snapB = snapshot(bufB);

    auto res = AudioABMetricsEvaluator::evaluate(
        "F3_LAG_OOB", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, 100, 9500, 9400,
        /*lag*/+135, /*corrSigned*/+0.97f, /*spectralDelta*/0.0f, /*intraIdentical*/true);

    CHECK(res.temporal.alignmentLagSamples == 135);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");

    auto j = verdict.toJson();
    CHECK(j["verdictLevel"] == "FAIL");
    CHECK(j["acceptanceDisposition"] == "Rejected");

    CHECK(buffersIdentical(bufA, snapA));
    CHECK(buffersIdentical(bufB, snapB));
}


// =============================================================================
// FIXTURE 4 — FixtureInvertedPolarity
// B = −A: correlationSigned = −1.0, correlationAbsolute = 1.0
// CRÍTICO: correlationAbsolute = 1.0 NO debe producir PASS.
// Resultado: FAIL / Rejected / DIAG_FAIL_POLARITY_INVERSION
// =============================================================================
TEST_CASE("5D.7 Fixture4 - FixtureInvertedPolarity: FAIL / DIAG_FAIL_POLARITY_INVERSION",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA = makeSine440(N);

    juce::AudioBuffer<float> bufB(1, N);
    {
        const float* a = bufA.getReadPointer(0);
        float* b = bufB.getWritePointer(0);
        for (int i = 0; i < N; ++i)
            b[i] = -a[i]; // polaridad invertida
    }

    auto snapA = snapshot(bufA);
    auto snapB = snapshot(bufB);

    // correlationSigned = -1.0, correlationAbsolute = 1.0
    auto res = AudioABMetricsEvaluator::evaluate(
        "F4_POLARITY_INV", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/0, /*corrSigned*/-1.0f, /*spectralDelta*/0.0f, /*intraIdentical*/true);

    // La invariante signed/absolute debe mantenerse
    CHECK(res.temporal.correlationSigned == Catch::Approx(-1.0f).epsilon(1e-4f));
    CHECK(res.temporal.correlationAbsolute == Catch::Approx(1.0f).epsilon(1e-4f));
    CHECK(res.temporal.correlationAbsolute == Catch::Approx(std::abs(res.temporal.correlationSigned)).epsilon(1e-6f));

    // La correlación absoluta de 1.0 NO debe enmascarar la inversión
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");

    auto j = verdict.toJson();
    CHECK(j["verdictLevel"] == "FAIL");
    CHECK(j["acceptanceDisposition"] == "Rejected");

    // No mutación
    CHECK(buffersIdentical(bufA, snapA));
    CHECK(buffersIdentical(bufB, snapB));
}


// =============================================================================
// FIXTURE 5 — FixtureUnexpectedClipping
// B contiene muestras |y| > 1.0 → Hard FAIL absoluto
// Clase y correlación: irrelevantes
// =============================================================================
TEST_CASE("5D.7 Fixture5 - FixtureUnexpectedClipping: Hard FAIL / DIAG_FAIL_UNEXPECTED_CLIPPING",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA = makeSine440(N);

    juce::AudioBuffer<float> bufB(1, N);
    {
        const float* a = bufA.getReadPointer(0);
        float* b = bufB.getWritePointer(0);
        for (int i = 0; i < N; ++i)
            b[i] = a[i] * 3.0f; // amplitud mayor que 1.0 → clipping
    }

    auto res = AudioABMetricsEvaluator::evaluate(
        "F5_CLIPPING", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/0, /*corrSigned*/+0.999f, /*spectralDelta*/0.5f, /*intraIdentical*/true);

    CHECK(res.amplitude.clippingA == false);
    CHECK(res.amplitude.clippingB == true);
    CHECK(res.amplitude.unexpectedClipping == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");
}


// =============================================================================
// FIXTURE 6 — FixtureEnvelopeCollapse
// A tiene señal activa; B es silencio digital en el región de sustain
// Warm-up inicial (100 muestras) NO debe confundirse con colapso
// =============================================================================
TEST_CASE("5D.7 Fixture6 - FixtureEnvelopeCollapse: Hard FAIL / DIAG_FAIL_ENVELOPE_COLLAPSE",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear(); // B permanece en silencio digital (colapso de motor)

    {
        float* a = bufA.getWritePointer(0);
        // Warm-up: muestras 0-99 en silencio (correcto)
        // Señal activa desde muestra 100 en adelante
        for (int i = 100; i < N; ++i)
            a[i] = 0.5f;
    }

    auto res = AudioABMetricsEvaluator::evaluate(
        "F6_ENV_COLLAPSE", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/0, /*corrSigned*/0.0f, /*spectralDelta*/15.0f, /*intraIdentical*/true);

    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");
}


// =============================================================================
// FIXTURE 7a — FixtureEventOrderViolation (NoteOff < NoteOn)
// =============================================================================
TEST_CASE("5D.7 Fixture7a - FixtureEventOrderViolation (NoteOff < NoteOn): FAIL / DIAG_FAIL_TEMPORAL_DESYNC",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear();

    // NoteOff (200) < NoteOn (500) → violación de orden
    auto res = AudioABMetricsEvaluator::evaluate(
        "F7A_EVENT_ORDER", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, /*noteOn*/500, /*noteOff*/200, /*expectedGate*/4300,
        /*lag*/0, /*corrSigned*/+1.0f, /*spectralDelta*/0.0f, /*intraIdentical*/true);

    CHECK(res.events.noteOffOrderValid == false);
    CHECK(res.events.eventOrderViolation == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
}


// =============================================================================
// FIXTURE 7b — FixtureEventOrderViolation (NoteOff == NoteOn, gate == 0)
// =============================================================================
TEST_CASE("5D.7 Fixture7b - FixtureEventOrderViolation (NoteOff == NoteOn): FAIL / DIAG_FAIL_TEMPORAL_DESYNC",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear();

    // NoteOff == NoteOn → gate de duración cero, también inválido
    auto res = AudioABMetricsEvaluator::evaluate(
        "F7B_EVENT_ORDER_EQ", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, /*noteOn*/300, /*noteOff*/300, /*expectedGate*/4300,
        /*lag*/0, /*corrSigned*/+1.0f, /*spectralDelta*/0.0f, /*intraIdentical*/true);

    CHECK(res.events.noteOffOrderValid == false); // 300 > 300 es false
    CHECK(res.events.eventOrderViolation == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
}


// =============================================================================
// FIXTURE 8 — FixtureNonDeterministicRender
// A1 != A2 → comparación invalidada, veredicto FAIL absoluto
// =============================================================================
TEST_CASE("5D.7 Fixture8 - FixtureNonDeterministicRender: Hard FAIL / DIAG_FAIL_NONDETERMINISTIC_RENDER",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear();

    // intraEngineBitIdentical = false → A1 != A2, render no determinista
    auto res = AudioABMetricsEvaluator::evaluate(
        "F8_NONDET", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/0, /*corrSigned*/+1.0f, /*spectralDelta*/0.0f,
        /*intraIdentical*/false);

    CHECK(res.intraEngineBitIdentical == false);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_NONDETERMINISTIC_RENDER));

    // El comparador no debe haber emitido DIAG_OK_IDENTITY (comparación abortada)
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");
}


// =============================================================================
// FIXTURE 9 — FixtureMetricNotApplicable
// THD y SNR son MetricNotApplicable → no produce falso FAIL ni falso 0.0
// =============================================================================
TEST_CASE("5D.7 Fixture9 - FixtureMetricNotApplicable: sin falso FAIL, JSON correcto",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA = makeSine440(N);
    auto bufB = makeSine440(N);

    auto res = AudioABMetricsEvaluator::evaluate(
        "F9_NA", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/-2, /*corrSigned*/+0.835f, /*spectralDelta*/0.50f, /*intraIdentical*/true,
        /*thdDelta*/std::nullopt, /*snrDelta*/std::nullopt);

    // Las métricas no aplicables deben permanecer vacías
    CHECK_FALSE(res.spectral.thdDeltaDb.has_value());
    CHECK_FALSE(res.spectral.snrDeltaDb.has_value());

    // Serialización: deben aparecer como "MetricNotApplicable", nunca como 0.0
    auto j = res.toJson();
    CHECK(j["spectral"]["thdDeltaDb"] == "MetricNotApplicable");
    CHECK(j["spectral"]["snrDeltaDb"] == "MetricNotApplicable");
    CHECK(j["spectral"]["thdDeltaDb"] != 0.0);
    CHECK(j["spectral"]["snrDeltaDb"] != 0.0);

    // No debe producir FAIL por ausencia de métricas
    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK_FALSE(verdict.isFail());

    // Verificar que la key correlationSigned aparece en el JSON temporal
    CHECK(j["temporal"].contains("correlationSigned"));
    CHECK(j["temporal"].contains("correlationAbsolute"));
    CHECK(j["temporal"]["correlationSigned"].get<float>() == Catch::Approx(+0.835f).epsilon(1e-3f));
}


// =============================================================================
// FIXTURE 10 — FixtureHighDensityDispersion
// HighDensitySpectral con dispersión esperada:
// corr ≈ 0.835, delta espectral ≈ 0.50 dB, lag ≈ -2
// Resultado: WARN / AcceptableWithExpectedDispersion (no PASS silencioso, no FAIL)
// =============================================================================
TEST_CASE("5D.7 Fixture10 - FixtureHighDensityDispersion: WARN + AcceptableWithExpectedDispersion",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear();

    {
        float* a = bufA.getWritePointer(0);
        float* b = bufB.getWritePointer(0);
        for (int i = 100; i < N; ++i)
        {
            a[i] = 0.4f;
            b[i] = 0.38f; // ligera variación que produce dispersión espectral
        }
    }

    auto res = AudioABMetricsEvaluator::evaluate(
        "F10_HIGH_DENSITY", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/-2, /*corrSigned*/+0.835f, /*spectralDelta*/0.50f, /*intraIdentical*/true);

    // Sin hard fails
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION));

    // Warnings de dispersión esperados
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_WARN_PHASE_DISPERSION));
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_WARN_HARMONIC_SPREAD));
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_OK_CLASS_TOLERANCE));

    // correlationSigned positivo (sin inversión de polaridad)
    CHECK(res.temporal.correlationSigned == Catch::Approx(+0.835f).epsilon(1e-3f));
    CHECK(res.temporal.correlationAbsolute == Catch::Approx(0.835f).epsilon(1e-3f));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);

    // WARN: visible en QA, no es una regresión
    CHECK(verdict.isWarn());
    CHECK(verdict.isAcceptableWithDispersion());
    CHECK(verdict.reasonCode == "WARN_TOLERANCE_RANGE");
    CHECK_FALSE(verdict.isPass());
    CHECK_FALSE(verdict.isFail());

    // Serialización JSON completa
    auto jv = verdict.toJson();
    CHECK(jv["verdictLevel"] == "WARN");
    CHECK(jv["acceptanceDisposition"] == "AcceptableWithExpectedDispersion");

    auto jr = res.toJson();
    CHECK(jr["temporal"]["correlationSigned"].get<float>() == Catch::Approx(+0.835f).epsilon(1e-3f));
    CHECK(jr["temporal"]["correlationAbsolute"].get<float>() == Catch::Approx(0.835f).epsilon(1e-3f));
    // Diagnósticos serializados como strings
    const auto& diagArr = jr["diagnostics"];
    bool hasPhase    = false;
    bool hasHarmonic = false;
    bool hasClass    = false;
    for (const auto& d : diagArr)
    {
        if (d == "DIAG_WARN_PHASE_DISPERSION")  hasPhase    = true;
        if (d == "DIAG_WARN_HARMONIC_SPREAD")   hasHarmonic = true;
        if (d == "DIAG_OK_CLASS_TOLERANCE")      hasClass    = true;
    }
    CHECK(hasPhase);
    CHECK(hasHarmonic);
    CHECK(hasClass);
}


// =============================================================================
// TESTS TRANSVERSALES: Precedencia de Hard Limits
// =============================================================================

TEST_CASE("5D.7 Cross - HighDensitySpectral con clipping inesperado: FAIL, no WARN",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear();

    {
        float* a = bufA.getWritePointer(0);
        float* b = bufB.getWritePointer(0);
        for (int i = 100; i < N; ++i)
        {
            a[i] = 0.8f;
            b[i] = 1.5f; // clipping inesperado en B
        }
    }

    auto res = AudioABMetricsEvaluator::evaluate(
        "CROSS_CLIP", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/-2, /*corrSigned*/+0.835f, /*spectralDelta*/0.50f, /*intraIdentical*/true);

    CHECK(res.amplitude.unexpectedClipping == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    // No puede ser WARN aunque la clase tenga tolerancias permisivas
    CHECK_FALSE(verdict.isWarn());
}


TEST_CASE("5D.7 Cross - GentleModulation con NoteOff antes de NoteOn: FAIL, no WARN",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear();

    auto res = AudioABMetricsEvaluator::evaluate(
        "CROSS_EVENT_GM", "PRESET_GENTLE_MOD_02", "GentleModulation", 48000.0, 256,
        bufA, bufB,
        100, /*noteOn*/500, /*noteOff*/200, /*expectedGate*/4300,
        /*lag*/0, /*corrSigned*/+0.9975f, /*spectralDelta*/0.16f, /*intraIdentical*/true);

    CHECK(res.events.eventOrderViolation == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    CHECK_FALSE(verdict.isWarn());
}


TEST_CASE("5D.7 Cross - CleanReference con B = -A: FAIL por polaridad aunque correlationAbsolute = 1.0",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA = makeSine440(N);

    juce::AudioBuffer<float> bufB(1, N);
    {
        const float* a = bufA.getReadPointer(0);
        float* b = bufB.getWritePointer(0);
        for (int i = 0; i < N; ++i)
            b[i] = -a[i];
    }

    auto res = AudioABMetricsEvaluator::evaluate(
        "CROSS_POLAR_CLEAN", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/0, /*corrSigned*/-1.0f, /*spectralDelta*/0.0f, /*intraIdentical*/true);

    // correlationAbsolute == 1.0 pero signed == -1.0
    CHECK(res.temporal.correlationSigned == Catch::Approx(-1.0f).epsilon(1e-4f));
    CHECK(res.temporal.correlationAbsolute == Catch::Approx(1.0f).epsilon(1e-4f));
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_POLARITY_INVERSION));
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    // No puede ser PASS aunque la correlación absoluta sea perfecta
    CHECK_FALSE(verdict.isPass());
}


TEST_CASE("5D.7 Cross - A1 != A2: FAIL aunque A contra B parezca equivalente",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA = makeSine440(N);
    auto bufB = makeSine440(N);

    // Render no determinista: intraIdentical = false
    auto res = AudioABMetricsEvaluator::evaluate(
        "CROSS_NONDET2", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/0, /*corrSigned*/+1.0f, /*spectralDelta*/0.0f,
        /*intraIdentical*/false);

    CHECK(res.intraEngineBitIdentical == false);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_NONDETERMINISTIC_RENDER));
    // No debe producir PASS residual
    CHECK_FALSE(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));

    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.isRejected());
    CHECK_FALSE(verdict.isPass());
}


TEST_CASE("5D.7 Cross - MetricNotApplicable no influye en policy y se conserva en JSON",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA = makeSine440(N);
    auto bufB = makeSine440(N);

    auto res = AudioABMetricsEvaluator::evaluate(
        "CROSS_NA_POLICY", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB,
        100, 100, 4700, 4600,
        /*lag*/-2, /*corrSigned*/+0.835f, /*spectralDelta*/0.50f, /*intraIdentical*/true,
        /*thdDelta*/std::nullopt, /*snrDelta*/std::nullopt);

    // La ausencia de THD y SNR no genera FAIL artificial
    auto policy  = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(res);
    CHECK_FALSE(verdict.isFail());

    // Pero se conserva en JSON para trazabilidad QA
    auto j = res.toJson();
    CHECK(j["spectral"]["thdDeltaDb"] == "MetricNotApplicable");
    CHECK(j["spectral"]["snrDeltaDb"] == "MetricNotApplicable");
    CHECK(j["spectral"]["thdDeltaDb"] != 0.0);
}


// =============================================================================
// TESTS DE NO MUTACIÓN: Los buffers de entrada no se modifican
// =============================================================================

TEST_CASE("5D.7 NonMutation - Los buffers A y B no se modifican durante evaluate()",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 4800;
    auto bufA = makeSine440(N);
    auto bufB = makeSine440(N);

    // Snapshot antes de evaluate
    auto snapA_before = snapshot(bufA);
    auto snapB_before = snapshot(bufB);

    // Múltiples evaluaciones con distintas configuraciones
    auto res1 = AudioABMetricsEvaluator::evaluate(
        "NM_01", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB, 100, 100, 4700, 4600, 0, +1.0f, 0.0f, true);

    auto res2 = AudioABMetricsEvaluator::evaluate(
        "NM_02", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB, 100, 100, 4700, 4600, -2, +0.835f, 0.50f, true);

    auto res3 = AudioABMetricsEvaluator::evaluate(
        "NM_03", "PRESET_AGGR_NONLIN_03", "AggressiveNonlinear", 48000.0, 256,
        bufA, bufB, 100, 100, 4700, 4600, -3, +0.9894f, 0.39f, true);

    (void)res1; (void)res2; (void)res3;

    // Los buffers deben ser idénticos al estado previo
    CHECK(buffersIdentical(bufA, snapA_before));
    CHECK(buffersIdentical(bufB, snapB_before));
}


// =============================================================================
// TEST DE INVARIANTE signed/absolute
// =============================================================================

TEST_CASE("5D.7 Invariant - correlationAbsolute == |correlationSigned| en todos los casos",
          "[audioab_5d][comparator][verdict][fixture][hermetic]")
{
    const int N = 1000;
    juce::AudioBuffer<float> bufA(1, N);
    juce::AudioBuffer<float> bufB(1, N);
    bufA.clear();
    bufB.clear();

    // Correlaciones representativas a verificar
    const std::vector<float> testCorrs = { +1.0f, -1.0f, +0.835f, -0.835f, 0.0f, +0.5f, -0.5f };

    for (float corr : testCorrs)
    {
        auto res = AudioABMetricsEvaluator::evaluate(
            "INV_CORR", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
            bufA, bufB, 100, 100, 900, 800,
            0, corr, 0.5f, true);

        const float expectedAbs = std::abs(corr);
        INFO("correlationSigned = " << corr << " -> expected correlationAbsolute = " << expectedAbs);
        CHECK(res.temporal.correlationAbsolute == Catch::Approx(expectedAbs).epsilon(1e-6f));
        CHECK(res.temporal.correlationSigned == Catch::Approx(corr).epsilon(1e-6f));
        CHECK(res.temporal.normalizedCrossCorrelation == Catch::Approx(expectedAbs).epsilon(1e-6f));
    }
}
