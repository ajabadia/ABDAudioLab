#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../math/AudioABMetrics5D.h"
#include <juce_audio_basics/juce_audio_basics.h>

using namespace abdaudiolab::math::qa5d;

TEST_CASE("5D.5 - 1. Exact Lag 0 and Identity Diagnostic", "[audioab_5d][metrics][identity]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    float* a = bufA.getWritePointer(0);
    float* b = bufB.getWritePointer(0);
    for (int i = 100; i < 900; ++i)
    {
        a[i] = 0.5f;
        b[i] = 0.5f;
    }

    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_01", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        0, 1.0f, 0.0f, true);

    CHECK(res.temporal.alignmentLagSamples == 0);
    CHECK(res.temporal.normalizedCrossCorrelation == 1.0f);
    CHECK(res.temporal.warmupAIsDigitalSilence == true);
    CHECK(res.temporal.warmupBIsDigitalSilence == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));
    CHECK(!res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
}

TEST_CASE("5D.5 - 2. Lag Within Allowed Window (<= 128 samples)", "[audioab_5d][metrics][lag_window]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    float* a = bufA.getWritePointer(0);
    float* b = bufB.getWritePointer(0);
    for (int i = 100; i < 900; ++i)
    {
        a[i] = 0.5f;
        b[i] = 0.49f;
    }

    // Lag de -3 muestras registrado sin ocultarlo
    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_02", "PRESET_AGGR_NONLIN_03", "AggressiveNonlinear", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        -3, 0.989f, 0.38f, true);

    CHECK(res.temporal.alignmentLagSamples == -3);
    CHECK(!res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_OK_CLASS_TOLERANCE));
}

TEST_CASE("5D.5 - 3. Lag Beyond Window (> 128 samples) Triggers Temporal Desync", "[audioab_5d][metrics][lag_fail]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_03", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        135, 0.75f, 1.2f, true);

    CHECK(res.temporal.alignmentLagSamples == 135);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
}

TEST_CASE("5D.5 - 4. Digital Silence is Never Serialized as 0 dBFS", "[audioab_5d][metrics][digital_silence]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear(); // Magnitud lineal estricta 0.0f
    bufB.clear();

    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_04", "PRESET_LOW_LEVEL_04", "LowLevelDynamic", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        0, 1.0f, 0.0f, true);

    CHECK(bufA.getMagnitude(0, 0, 1000) == 0.0f);
    CHECK(!res.amplitude.peakA_dbfs.has_value());
    CHECK(!res.amplitude.rmsA_dbfs.has_value());

    // Verificar serialización a JSON: debe ser "DigitalSilence", nunca 0 o 0.0
    auto j = res.toJson();
    CHECK(j["amplitude"]["peakA_dbfs"] == "DigitalSilence");
    CHECK(j["amplitude"]["rmsA_dbfs"] == "DigitalSilence");
    CHECK(j["amplitude"]["peakA_dbfs"] != 0.0);
    CHECK(j["amplitude"]["peakA_dbfs"] != "0 dBFS");
}

TEST_CASE("5D.5 - 5. Unexpected Clipping in B Triggers DIAG_FAIL_UNEXPECTED_CLIPPING", "[audioab_5d][metrics][clipping]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    float* a = bufA.getWritePointer(0);
    float* b = bufB.getWritePointer(0);
    for (int i = 100; i < 900; ++i)
    {
        a[i] = 0.8f;   // Sin clipping
        b[i] = 1.15f;  // Clipping inesperado (> 1.0f)
    }

    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_05", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        0, 0.95f, 0.5f, true);

    CHECK(res.amplitude.clippingA == false);
    CHECK(res.amplitude.clippingB == true);
    CHECK(res.amplitude.unexpectedClipping == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING));
}

TEST_CASE("5D.5 - 6. Non-Deterministic Render (A1 != A2) Aborts Comparison", "[audioab_5d][metrics][nondeterministic]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_06", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        0, 1.0f, 0.0f, false); // intraEngineBitIdentical = false

    CHECK(res.intraEngineBitIdentical == false);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_NONDETERMINISTIC_RENDER));
    CHECK(!res.hasDiagnostic(DiagnosticCode::DIAG_OK_IDENTITY));
}

TEST_CASE("5D.5 - 7. Event Order Violation (NoteOff Before NoteOn)", "[audioab_5d][metrics][events]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    // NoteOff a sample 200, NoteOn a sample 500 (secuencia corrupta)
    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_07", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB, 100, 500, 200, 800,
        0, 1.0f, 0.0f, true);

    CHECK(res.events.noteOffOrderValid == false);
    CHECK(res.events.eventOrderViolation == true);
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
}

TEST_CASE("5D.5 - 8. Inapplicable Metrics Serialize as MetricNotApplicable", "[audioab_5d][metrics][not_applicable]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    // Sin métricas de THD ni SNR (no aplicables a señales complejas multivoz)
    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_08", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        -2, 0.835f, 0.50f, true,
        std::nullopt, std::nullopt);

    CHECK(!res.spectral.thdDeltaDb.has_value());
    CHECK(!res.spectral.snrDeltaDb.has_value());

    auto j = res.toJson();
    CHECK(j["spectral"]["thdDeltaDb"] == "MetricNotApplicable");
    CHECK(j["spectral"]["snrDeltaDb"] == "MetricNotApplicable");
    CHECK(j["spectral"]["thdDeltaDb"] != 0.0);
}

TEST_CASE("5D.5 - 9. Envelope Collapse in B Triggers DIAG_FAIL_ENVELOPE_COLLAPSE", "[audioab_5d][metrics][collapse]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    float* a = bufA.getWritePointer(0);
    for (int i = 100; i < 900; ++i) a[i] = 0.5f; // A tiene señal
    // B permanece completamente silenciado (colapso de motor)

    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_09", "PRESET_CLEAN_REF_01", "CleanReference", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        0, 0.0f, 15.0f, true);

    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE));
}

TEST_CASE("5D.5 - 10. High Density Dispersion Yields Phase and Harmonic Warnings Without Premature Failure", "[audioab_5d][metrics][dispersion]")
{
    juce::AudioBuffer<float> bufA(1, 1000);
    juce::AudioBuffer<float> bufB(1, 1000);
    bufA.clear();
    bufB.clear();

    float* a = bufA.getWritePointer(0);
    float* b = bufB.getWritePointer(0);
    for (int i = 100; i < 900; ++i)
    {
        a[i] = 0.4f;
        b[i] = 0.38f; // Ligera variación
    }

    // Correlación 0.835, delta espectral 0.50 dB
    auto res = AudioABMetricsEvaluator::evaluate(
        "RUN_TEST_10", "PRESET_HIGH_DENSITY_05", "HighDensitySpectral", 48000.0, 256,
        bufA, bufB, 100, 100, 900, 800,
        -2, 0.835f, 0.50f, true);

    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_WARN_PHASE_DISPERSION));
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_WARN_HARMONIC_SPREAD));
    CHECK(res.hasDiagnostic(DiagnosticCode::DIAG_OK_CLASS_TOLERANCE));
    CHECK(!res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_UNEXPECTED_CLIPPING));
    CHECK(!res.hasDiagnostic(DiagnosticCode::DIAG_FAIL_TEMPORAL_DESYNC));
}
