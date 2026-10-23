#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "synth/SyntheticGroundTruthFixtures.h"
#include <vector>
#include <cmath>
#include <numbers>

using namespace abdaudiolab::synth::ground_truth;
using Catch::Matchers::WithinAbs;

namespace
{

/**
 * @brief Helper para generar un seno puro continuo.
 */
std::vector<float> generateSineWave(double freqHz, double sampleRate, double durationSec, double amplitude = 1.0)
{
    const size_t numSamples = static_cast<size_t>(sampleRate * durationSec);
    std::vector<float> buffer(numSamples);
    const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;
    for (size_t n = 0; n < numSamples; ++n)
    {
        buffer[n] = static_cast<float>(amplitude * std::sin(omega * static_cast<double>(n)));
    }
    return buffer;
}

/**
 * @brief Calcula la amplitud estacionaria (pico) de un tono puro mediante correlación de Fourier.
 */
double measureToneAmplitude(const std::vector<float>& signal, double freqHz, double sampleRate, size_t skipStartSamples = 0)
{
    if (signal.size() <= skipStartSamples) return 0.0;

    const size_t nSamples = signal.size() - skipStartSamples;
    const double omega = 2.0 * std::numbers::pi * freqHz / sampleRate;

    double sumCos = 0.0;
    double sumSin = 0.0;

    for (size_t i = 0; i < nSamples; ++i)
    {
        double t = static_cast<double>(skipStartSamples + i);
        double s = static_cast<double>(signal[skipStartSamples + i]);
        sumCos += s * std::cos(omega * t);
        sumSin += s * std::sin(omega * t);
    }

    double rCos = (2.0 / static_cast<double>(nSamples)) * sumCos;
    double rSin = (2.0 / static_cast<double>(nSamples)) * sumSin;

    return std::sqrt(rCos * rCos + rSin * rSin);
}

} // namespace

// ==============================================================================
// 1. GroundTruthMetrics - Analytical Validation
// ==============================================================================

TEST_CASE("GroundTruthMetrics - Analytical Validation", "[ground_truth][math]")
{
    SECTION("Identical signals yield theoretical zero error and perfect R^2")
    {
        std::vector<float> target = { 0.1f, -0.5f, 0.8f, -0.2f, 0.4f };
        std::vector<float> candidate = target;

        double esr = GroundTruthMetrics::computeEsrDb(target, candidate);
        double rmse = GroundTruthMetrics::computeRmse(target, candidate);
        double r2 = GroundTruthMetrics::computeRSquared(target, candidate);

        REQUIRE(esr <= -180.0);
        REQUIRE_THAT(rmse, WithinAbs(0.0, 1e-7));
        REQUIRE_THAT(r2, WithinAbs(1.0, 1e-7));
    }

    SECTION("Known 10% proportional error matches exact closed-form ESR, RMSE, and R^2")
    {
        // Target: [1.0, 2.0, 3.0, 4.0] -> Target power = 1 + 4 + 9 + 16 = 30.0
        // Candidate: [1.1, 2.2, 3.3, 4.4] -> Diff: [0.1, 0.2, 0.3, 0.4] -> Error power = 0.30
        // ESR = 10 * log10(0.30 / 30.0) = 10 * log10(0.01) = -20.0 dB
        // RMSE = sqrt(0.30 / 4) = sqrt(0.075) = 0.27386127875
        // Mean target = 2.5, SS_tot = (1-2.5)^2 + (2-2.5)^2 + (3-2.5)^2 + (4-2.5)^2 = 5.0
        // R^2 = 1.0 - (0.30 / 5.0) = 1.0 - 0.06 = 0.94
        std::vector<float> target = { 1.0f, 2.0f, 3.0f, 4.0f };
        std::vector<float> candidate = { 1.1f, 2.2f, 3.3f, 4.4f };

        double esr = GroundTruthMetrics::computeEsrDb(target, candidate);
        double rmse = GroundTruthMetrics::computeRmse(target, candidate);
        double r2 = GroundTruthMetrics::computeRSquared(target, candidate);

        REQUIRE_THAT(esr, WithinAbs(-20.0, 1e-4));
        REQUIRE_THAT(rmse, WithinAbs(std::sqrt(0.075), 1e-5));
        REQUIRE_THAT(r2, WithinAbs(0.94, 1e-5));
    }

    SECTION("Empty or zero-energy signals handle edge cases gracefully without NaN or div-by-zero")
    {
        std::vector<float> empty1;
        std::vector<float> empty2;
        REQUIRE_THAT(GroundTruthMetrics::computeEsrDb(empty1, empty2), WithinAbs(0.0, 1e-7));
        REQUIRE_THAT(GroundTruthMetrics::computeRmse(empty1, empty2), WithinAbs(0.0, 1e-7));
        REQUIRE_THAT(GroundTruthMetrics::computeRSquared(empty1, empty2), WithinAbs(1.0, 1e-7));

        std::vector<float> zeros(10, 0.0f);
        REQUIRE_THAT(GroundTruthMetrics::computeEsrDb(zeros, zeros), WithinAbs(-180.0, 1e-7));
        REQUIRE_THAT(GroundTruthMetrics::computeRmse(zeros, zeros), WithinAbs(0.0, 1e-7));
    }
}

// ==============================================================================
// 2. PureLinearBypassFixture - Analytic Gain and Delay
// ==============================================================================

TEST_CASE("PureLinearBypassFixture - Analytic Gain and Delay", "[ground_truth][bypass]")
{
    PureLinearBypassFixture fixture;
    fixture.prepare(96000.0);

    SECTION("Default unit gain and zero delay produces bitwise identity")
    {
        std::vector<float> input = { 0.1f, -0.4f, 0.75f, -0.9f, 0.0f, 0.33f };
        std::vector<float> output = fixture.process(input);

        REQUIRE(output.size() == input.size());
        for (size_t i = 0; i < input.size(); ++i)
        {
            REQUIRE(output[i] == input[i]);
        }

        double esr = GroundTruthMetrics::computeEsrDb(input, output);
        REQUIRE(esr <= -180.0);
    }

    SECTION("Configured gain scales output precisely with zero residual error")
    {
        fixture.setGain(0.5);
        fixture.setDelaySamples(0);

        std::vector<float> input = { 1.0f, -2.0f, 0.5f, 0.0f };
        std::vector<float> output = fixture.process(input);

        std::vector<float> expected = { 0.5f, -1.0f, 0.25f, 0.0f };
        REQUIRE(output.size() == expected.size());
        for (size_t i = 0; i < expected.size(); ++i)
        {
            REQUIRE_THAT(output[i], WithinAbs(expected[i], 1e-6f));
        }

        REQUIRE(GroundTruthMetrics::computeEsrDb(expected, output) <= -180.0);
    }

    SECTION("Configured sample delay shifts impulse response exactly tau samples")
    {
        const int delay = 4;
        fixture.setGain(1.0);
        fixture.setDelaySamples(delay);
        fixture.reset();

        std::vector<float> impulse(10, 0.0f);
        impulse[0] = 1.0f; // Delta[n]

        std::vector<float> output = fixture.process(impulse);

        for (int i = 0; i < 10; ++i)
        {
            if (i == delay)
                REQUIRE_THAT(output[static_cast<size_t>(i)], WithinAbs(1.0f, 1e-6f));
            else
                REQUIRE_THAT(output[static_cast<size_t>(i)], WithinAbs(0.0f, 1e-6f));
        }
    }

    SECTION("Chunked block processing matches continuous processing identically")
    {
        fixture.setGain(0.85);
        fixture.setDelaySamples(3);
        fixture.reset();

        auto sine = generateSineWave(440.0, 96000.0, 0.02, 0.9);
        std::vector<float> continuous = fixture.process(sine);

        fixture.reset();
        std::vector<float> chunked(sine.size());
        const size_t blockSize = 64;
        for (size_t offset = 0; offset < sine.size(); offset += blockSize)
        {
            size_t n = std::min(blockSize, sine.size() - offset);
            fixture.process(sine.data() + offset, chunked.data() + offset, n);
        }

        REQUIRE(GroundTruthMetrics::computeEsrDb(continuous, chunked) <= -180.0);
        REQUIRE_THAT(GroundTruthMetrics::computeRmse(continuous, chunked), WithinAbs(0.0, 1e-7));
    }
}

// ==============================================================================
// 3. TptZdfFilterFixture - LTI Behavior and Exact Analytical Response
// ==============================================================================

TEST_CASE("TptZdfFilterFixture - LTI Behavior and Exact Analytical Response", "[ground_truth][filter]")
{
    const double sampleRate = 96000.0;
    const double cutoff = 1000.0;
    const double q = 0.70710678; // Butterworth Q

    SECTION("Lowpass 2nd Order matches analytical magnitude at passband, cutoff, and stopband")
    {
        TptZdfFilterFixture filter(cutoff, q, TptFilterType::Lowpass, TptFilterSlope::Order2_12dB);
        filter.prepare(sampleRate);

        // 1. Passband: 100 Hz
        {
            double expectedMag = filter.getAnalyticalMagnitude(100.0);
            REQUIRE(expectedMag > 0.99); // Muy cercano a 1.0 (0 dB)

            auto testSine = generateSineWave(100.0, sampleRate, 0.15, 1.0);
            auto filtered = filter.process(testSine);

            // Omitir transitorio inicial (primeros 50 ms = 4800 muestras)
            double measuredMag = measureToneAmplitude(filtered, 100.0, sampleRate, 4800);
            REQUIRE_THAT(measuredMag, WithinAbs(expectedMag, 0.015));
        }

        // 2. Cutoff: 1000 Hz (-3.01 dB = 0.7071)
        {
            filter.reset();
            double expectedMag = filter.getAnalyticalMagnitude(1000.0);
            REQUIRE_THAT(expectedMag, WithinAbs(0.7071, 0.005));

            auto testSine = generateSineWave(1000.0, sampleRate, 0.10, 1.0);
            auto filtered = filter.process(testSine);

            double measuredMag = measureToneAmplitude(filtered, 1000.0, sampleRate, 4800);
            REQUIRE_THAT(measuredMag, WithinAbs(expectedMag, 0.015));
        }

        // 3. Stopband: 10000 Hz (~ -40 dB = ~0.010)
        {
            filter.reset();
            double expectedMag = filter.getAnalyticalMagnitude(10000.0);
            REQUIRE(expectedMag < 0.015);

            auto testSine = generateSineWave(10000.0, sampleRate, 0.05, 1.0);
            auto filtered = filter.process(testSine);

            double measuredMag = measureToneAmplitude(filtered, 10000.0, sampleRate, 2400);
            REQUIRE_THAT(measuredMag, WithinAbs(expectedMag, 0.005));
        }
    }

    SECTION("Lowpass 4th Order demonstrates exact cascaded magnitude response (24 dB/oct)")
    {
        TptZdfFilterFixture filter4(cutoff, q, TptFilterType::Lowpass, TptFilterSlope::Order4_24dB);
        filter4.prepare(sampleRate);

        // A la frecuencia de corte, 4º orden debe atenuar (1/sqrt(2))^2 = 0.50 (-6.02 dB)
        double expectedMag = filter4.getAnalyticalMagnitude(1000.0);
        REQUIRE_THAT(expectedMag, WithinAbs(0.50, 0.005));

        auto testSine = generateSineWave(1000.0, sampleRate, 0.10, 1.0);
        auto filtered = filter4.process(testSine);

        double measuredMag = measureToneAmplitude(filtered, 1000.0, sampleRate, 4800);
        REQUIRE_THAT(measuredMag, WithinAbs(expectedMag, 0.02));
    }

    SECTION("Highpass and Bandpass modes conform to respective transfer functions")
    {
        // Highpass: rechaza 100 Hz, deja pasar 10 kHz
        TptZdfFilterFixture hp(cutoff, q, TptFilterType::Highpass, TptFilterSlope::Order2_12dB);
        hp.prepare(sampleRate);
        REQUIRE(hp.getAnalyticalMagnitude(100.0) < 0.02);
        REQUIRE(hp.getAnalyticalMagnitude(10000.0) > 0.98);

        // Bandpass: pico en 1000 Hz, atenúa en 100 Hz y en 10 kHz
        TptZdfFilterFixture bp(cutoff, q, TptFilterType::Bandpass, TptFilterSlope::Order2_12dB);
        bp.prepare(sampleRate);
        REQUIRE_THAT(bp.getAnalyticalMagnitude(1000.0), WithinAbs(1.0, 0.01));
        REQUIRE(bp.getAnalyticalMagnitude(100.0) < 0.15);
        REQUIRE(bp.getAnalyticalMagnitude(10000.0) < 0.15);
    }

    SECTION("LTI Spectral Purity: Zero spurious harmonics (THD < -100 dB)")
    {
        TptZdfFilterFixture filter(1000.0, 1.5, TptFilterType::Lowpass, TptFilterSlope::Order2_12dB);
        filter.prepare(sampleRate);

        auto sine1k = generateSineWave(1000.0, sampleRate, 0.10, 1.0);
        auto filtered = filter.process(sine1k);

        const size_t skipTransient = 4800;
        double fundamentalAmp = measureToneAmplitude(filtered, 1000.0, sampleRate, skipTransient);
        double h2Amp = measureToneAmplitude(filtered, 2000.0, sampleRate, skipTransient);
        double h3Amp = measureToneAmplitude(filtered, 3000.0, sampleRate, skipTransient);

        REQUIRE(fundamentalAmp > 0.5);
        // Armónicos H2 y H3 deben ser nulos a nivel de precisión numérica de punto flotante
        REQUIRE(h2Amp < 1e-5);
        REQUIRE(h3Amp < 1e-5);

        double thdDb = 20.0 * std::log10(std::sqrt(h2Amp * h2Amp + h3Amp * h3Amp) / fundamentalAmp);
        REQUIRE(thdDb < -100.0);
    }
}

// ==============================================================================
// 4. StaticPolynomialSaturatorFixture - Memoryless Transfer and Harmonic Analysis
// ==============================================================================

TEST_CASE("StaticPolynomialSaturatorFixture - Memoryless Transfer and Harmonic Analysis", "[ground_truth][saturator]")
{
    StaticPolynomialSaturatorFixture saturator(0.15, 0.02, PolynomialSaturatorMode::CubicOnly);
    saturator.prepare(96000.0);

    SECTION("Strict memoryless property: output is independent of execution order or block size")
    {
        float x1 = 0.5f;
        float x2 = -0.7f;
        float y1 = saturator.evaluateTransfer(x1);
        float y2 = saturator.evaluateTransfer(x2);

        // En bloque
        std::vector<float> block = { x1, x2 };
        std::vector<float> blockOut = saturator.process(block);

        REQUIRE_THAT(blockOut[0], WithinAbs(y1, 1e-7f));
        REQUIRE_THAT(blockOut[1], WithinAbs(y2, 1e-7f));

        // En orden invertido
        std::vector<float> reversed = { x2, x1 };
        std::vector<float> reversedOut = saturator.process(reversed);

        REQUIRE_THAT(reversedOut[0], WithinAbs(y2, 1e-7f));
        REQUIRE_THAT(reversedOut[1], WithinAbs(y1, 1e-7f));
    }

    SECTION("Odd symmetry f(-x) == -f(x) holds for all modes")
    {
        for (auto mode : { PolynomialSaturatorMode::CubicOnly,
                           PolynomialSaturatorMode::CubicQuintic,
                           PolynomialSaturatorMode::NormalizedTanh })
        {
            saturator.setMode(mode);
            for (float x : { 0.1f, 0.35f, 0.7f, 0.95f })
            {
                float pos = saturator.evaluateTransfer(x);
                float neg = saturator.evaluateTransfer(-x);
                REQUIRE_THAT(pos, WithinAbs(-neg, 1e-6f));
            }
        }
    }

    SECTION("Wiener-Hammerstein LNL Upper Bound: exact recovery of coefficient a with R^2 > 0.999")
    {
        const double trueA = 0.18;
        saturator.setCoeffA(trueA);
        saturator.setMode(PolynomialSaturatorMode::CubicOnly);

        std::vector<float> testInputs = { 0.2f, 0.4f, 0.6f, 0.8f };
        std::vector<float> groundTruth = saturator.process(testInputs);

        // Despeje analítico directo: y = x - a * x^3  =>  a = (x - y) / (x^3)
        for (size_t i = 0; i < testInputs.size(); ++i)
        {
            double x = testInputs[i];
            double y = groundTruth[i];
            double x3 = x * x * x;
            double recoveredA = (x - y) / x3;
            REQUIRE_THAT(recoveredA, WithinAbs(trueA, 1e-6));
        }

        // Modelo candidato evaluado con el coeficiente recuperado
        std::vector<float> candidate(testInputs.size());
        for (size_t i = 0; i < testInputs.size(); ++i)
        {
            double x = testInputs[i];
            candidate[i] = static_cast<float>(x - trueA * x * x * x);
        }

        double r2 = GroundTruthMetrics::computeRSquared(groundTruth, candidate);
        double esr = GroundTruthMetrics::computeEsrDb(groundTruth, candidate);

        REQUIRE(r2 > 0.999999);
        REQUIRE(esr <= -180.0);
    }

    SECTION("Closed-form harmonic signature matches Chebyshev/trigonometric identities")
    {
        // Para x(t) = A * sin(wt):
        // x^3 = A^3 * (3/4 * sin(wt) - 1/4 * sin(3wt))
        // y(t) = (A - 3/4 * a * A^3) * sin(wt) + (1/4 * a * A^3) * sin(3wt)
        const double a = 0.20;
        const double A = 0.80;
        saturator.setCoeffA(a);
        saturator.setMode(PolynomialSaturatorMode::CubicOnly);

        const double sampleRate = 96000.0;
        const double freqHz = 1000.0;
        auto sine = generateSineWave(freqHz, sampleRate, 0.05, A);
        auto saturated = saturator.process(sine);

        double theoreticalFundamental = A - (0.75 * a * (A * A * A));
        double theoreticalH3 = 0.25 * a * (A * A * A);

        double measuredFundamental = measureToneAmplitude(saturated, 1000.0, sampleRate, 0);
        double measuredH2 = measureToneAmplitude(saturated, 2000.0, sampleRate, 0);
        double measuredH3 = measureToneAmplitude(saturated, 3000.0, sampleRate, 0);

        REQUIRE_THAT(measuredFundamental, WithinAbs(theoreticalFundamental, 0.002));
        REQUIRE_THAT(measuredH3, WithinAbs(theoreticalH3, 0.001));

        // Armónico par H2 debe ser exactamente cero
        REQUIRE(measuredH2 < 1e-5);
    }
}

// ==============================================================================
// 5. DynamicMemorySystemFixture - Dynamic State Memory and Hysteresis
// ==============================================================================

TEST_CASE("DynamicMemorySystemFixture - Dynamic State Memory and Hysteresis", "[ground_truth][memory]")
{
    DynamicMemorySystemFixture system(0.005, 0.080, 1.5, 0.25);
    system.prepare(96000.0);

    SECTION("Static condition (beta = 0.0) yields near-zero hysteresis loop area")
    {
        system.setFeedbackBeta(0.0); // Sin retroalimentación dependiente de estado

        double area = system.measureHysteresisArea(100.0, 0.8, 10);
        // Cuando beta = 0, la relación es monótona estática y la integral de contorno cerrado colapsa a 0
        REQUIRE(area < 1e-4);
    }

    SECTION("Dynamic state memory (beta = 1.5) produces significant open hysteresis loop area")
    {
        system.setFeedbackBeta(1.5);

        double area = system.measureHysteresisArea(100.0, 0.8, 10);
        // El bucle de histéresis debe estar claramente abierto y medible (> 0.0015 vs < 1e-4 en estático)
        REQUIRE(area > 0.0015);
    }

    SECTION("Asymmetric dynamics: fast attack, slow release verified on transient envelope")
    {
        system.setAttackSec(0.002); // 2 ms
        system.setReleaseSec(0.050); // 50 ms
        system.prepare(96000.0);

        // Pulso rectangular de 10 ms seguido de 50 ms de silencio
        const size_t attackSamples = static_cast<size_t>(96000.0 * 0.010);
        const size_t releaseSamples = static_cast<size_t>(96000.0 * 0.050);
        std::vector<float> pulse(attackSamples + releaseSamples, 0.0f);
        std::fill_n(pulse.begin(), attackSamples, 0.9f);

        auto output = system.process(pulse);

        // Durante el pulso, la respuesta se comprime rápidamente hacia un régimen cuasi-estacionario
        float initialPeak = output[10];
        float lateInPulse = output[attackSamples - 10];
        REQUIRE(lateInPulse < initialPeak); // Compresión de estado activa

        // Tras cesar el pulso, la salida regresa a cero suavemente
        REQUIRE_THAT(output.back(), WithinAbs(0.0f, 1e-4f));
    }
}

// ==============================================================================
// 6. Benchmarking Protocol Simulation
// ==============================================================================

TEST_CASE("Ground Truth Fixtures - Benchmarking Protocol Simulation", "[ground_truth][benchmark]")
{
    const double sampleRate = 96000.0;

    SECTION("PureLinearBypass accepts exact linear candidate and rejects perturbed candidate")
    {
        PureLinearBypassFixture bypass(1.0, 0);
        bypass.prepare(sampleRate);

        auto testSine = generateSineWave(440.0, sampleRate, 0.02, 0.8);
        auto groundTruthAudio = bypass.process(testSine);

        // Candidato 1: Modelo lineal ideal
        std::vector<float> idealCandidate = testSine;
        double esrIdeal = GroundTruthMetrics::computeEsrDb(groundTruthAudio, idealCandidate);
        REQUIRE(esrIdeal <= -180.0);

        // Candidato 2: Modelo imperfecto con 3% de error de ganancia (g = 0.97)
        std::vector<float> badCandidate(testSine.size());
        for (size_t i = 0; i < testSine.size(); ++i)
            badCandidate[i] = testSine[i] * 0.97f;

        double esrBad = GroundTruthMetrics::computeEsrDb(groundTruthAudio, badCandidate);
        // Error de 3% => ESR ~ 10 * log10(0.03^2) = 10 * log10(0.0009) = -30.4 dB
        REQUIRE_THAT(esrBad, WithinAbs(-30.45, 0.5));
        REQUIRE(esrBad > -40.0); // No supera el umbral estricto de -60 dB
    }

    SECTION("StaticPolynomialSaturator establishes deterministic baseline for memoryless identification")
    {
        StaticPolynomialSaturatorFixture saturator(0.12, 0.0, PolynomialSaturatorMode::CubicOnly);
        saturator.prepare(sampleRate);

        auto testSine = generateSineWave(500.0, sampleRate, 0.02, 0.7);
        auto groundTruthAudio = saturator.process(testSine);

        // Simulamos un estimador polinómico que identifica a = 0.12
        std::vector<float> fittedModel(testSine.size());
        for (size_t i = 0; i < testSine.size(); ++i)
        {
            float x = testSine[i];
            fittedModel[i] = x - 0.12f * (x * x * x);
        }

        double esr = GroundTruthMetrics::computeEsrDb(groundTruthAudio, fittedModel);
        double r2 = GroundTruthMetrics::computeRSquared(groundTruthAudio, fittedModel);

        // En aritmética float32 el error residual alcanza el piso de precisión (~ -151.8 dB)
        REQUIRE(esr <= -140.0);
        REQUIRE(r2 > 0.999999);
    }
}
