#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../math/AudioABTolerancePolicy5D.h"
#include <juce_audio_basics/juce_audio_basics.h>

using namespace abdaudiolab::math::qa5d;

namespace {

AudioABEvaluationResult createDummyResult(
    const std::string& presetClass,
    float correlation = 1.0f,
    float spectralDeltaDb = 0.0f,
    float rmsDeltaDb = 0.0f,
    int lagSamples = 0,
    bool intraIdentical = true,
    bool unexpectedClipping = false,
    bool eventViolation = false,
    bool envelopeCollapse = false)
{
    AudioABEvaluationResult r;
    r.runId = "RUN_POLICY_TEST";
    r.presetId = "TEST_PRESET";
    r.presetClass = presetClass;
    r.sampleRate = 48000.0;
    r.blockSize = 256;
    r.intraEngineBitIdentical = intraIdentical;

    r.temporal.alignmentLagSamples = lagSamples;
    r.temporal.normalizedCrossCorrelation = correlation;
    r.temporal.warmupAIsDigitalSilence = true;
    r.temporal.warmupBIsDigitalSilence = true;

    r.amplitude.peakA_dbfs = -3.0f;
    r.amplitude.peakB_dbfs = -3.0f;
    r.amplitude.rmsA_dbfs = -12.0f;
    r.amplitude.rmsB_dbfs = -12.0f;
    r.amplitude.rmsDeltaDb = rmsDeltaDb;
    r.amplitude.unexpectedClipping = unexpectedClipping;

    r.spectral.meanSpectralDeltaDb = spectralDeltaDb;
    r.events.eventOrderViolation = eventViolation;

    if (envelopeCollapse)
    {
        r.diagnostics.push_back(DiagnosticCode::DIAG_FAIL_ENVELOPE_COLLAPSE);
    }

    return r;
}

} // namespace

TEST_CASE("5D.6 - 1. CleanReference Observed Baseline Yields PASS", "[audioab_5d][policy][clean]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto res = createDummyResult("CleanReference", 1.0f, 0.0000f, 0.000f, 0);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isPass());
    CHECK(verdict.reasonCode == "PASS_ALL_CRITERIA");
    CHECK(verdict.triggeredFails.empty());
    CHECK(verdict.triggeredWarns.empty());
}

TEST_CASE("5D.6 - 2. CleanReference with Correlation 0.9980 Yields Class FAIL", "[audioab_5d][policy][clean_fail]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    // Umbral WARN es 0.9990; 0.9980 queda por debajo de WARN -> Class FAIL
    auto res = createDummyResult("CleanReference", 0.9980f, 0.0000f, 0.000f, 0);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.reasonCode == "CLASS_TOLERANCE_EXCEEDED");
}

TEST_CASE("5D.6 - 3. GentleModulation with Moderate Dispersion Yields PASS", "[audioab_5d][policy][modulation]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    // Resultados observados: Corr 0.9975, Spectral 0.16 dB, Lag 0, RMS 0.033 dB
    auto res = createDummyResult("GentleModulation", 0.9975f, 0.16f, 0.033f, 0);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isPass());
}

TEST_CASE("5D.6 - 4. GentleModulation in WARN Zone (Corr 0.9920) Yields WARN", "[audioab_5d][policy][modulation_warn]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    // Entre 0.9900 y 0.9950 produce WARN
    auto res = createDummyResult("GentleModulation", 0.9920f, 0.16f, 0.033f, 0);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isWarn());
    CHECK(verdict.reasonCode == "WARN_TOLERANCE_RANGE");
}

TEST_CASE("5D.6 - 5. AggressiveNonlinear with Lag -3 and Spectral 0.39 dB Yields PASS", "[audioab_5d][policy][nonlinear]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    // Observado: Corr 0.9894, Spectral 0.39 dB, Lag -3, RMS 0.009 dB
    auto res = createDummyResult("AggressiveNonlinear", 0.9894f, 0.39f, 0.009f, -3);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isPass());
}

TEST_CASE("5D.6 - 6. AggressiveNonlinear with Lag > 16 Samples Yields Class FAIL", "[audioab_5d][policy][nonlinear_lag_fail]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto res = createDummyResult("AggressiveNonlinear", 0.9894f, 0.39f, 0.009f, 24);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.reasonCode == "CLASS_TOLERANCE_EXCEEDED");
}

TEST_CASE("5D.6 - 7. LowLevelDynamic Envelope Collapse Triggers Hard FAIL", "[audioab_5d][policy][collapse]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto res = createDummyResult("LowLevelDynamic", 0.0f, 10.0f, 20.0f, 0, true, false, false, true);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");
}

TEST_CASE("5D.6 - 8. HighDensitySpectral Observed Baseline Yields PASS", "[audioab_5d][policy][high_density]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    // Observado: Corr 0.835, Spectral 0.50 dB, Lag -2, RMS 0.22 dB
    auto res = createDummyResult("HighDensitySpectral", 0.835f, 0.50f, 0.22f, -2);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isPass());
}

TEST_CASE("5D.6 - 9. HighDensitySpectral with Correlation < 0.75 Yields Class FAIL", "[audioab_5d][policy][high_density_fail]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto res = createDummyResult("HighDensitySpectral", 0.720f, 0.50f, 0.22f, -2);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.reasonCode == "CLASS_TOLERANCE_EXCEEDED");
}

TEST_CASE("5D.6 - 10. Hard Limits Precedence: Unexpected Clipping Overrides High Correlation", "[audioab_5d][policy][precedence]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    // Señal con correlación perfecta 1.0, pero con clipping inesperado en B
    auto res = createDummyResult("CleanReference", 1.0f, 0.0f, 0.0f, 0, true, true, false, false);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");
}

TEST_CASE("5D.6 - 11. Hard Limits Precedence: Non-Deterministic Render (A1 != A2) Overrides All", "[audioab_5d][policy][nondet]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto res = createDummyResult("HighDensitySpectral", 0.99f, 0.1f, 0.0f, 0, false, false, false, false);

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isFail());
    CHECK(verdict.reasonCode == "HARD_LIMIT_VIOLATION");
}

TEST_CASE("5D.6 - 12. Inapplicable Metrics Excluded from Evaluation Gates", "[audioab_5d][policy][metric_na]")
{
    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto res = createDummyResult("HighDensitySpectral", 0.835f, 0.50f, 0.22f, -2);
    res.spectral.thdDeltaDb = std::nullopt; // MetricNotApplicable
    res.spectral.snrDeltaDb = std::nullopt; // MetricNotApplicable

    auto verdict = policy.evaluate(res);
    CHECK(verdict.isPass()); // No participa en evaluación, no genera falso FAIL
}
