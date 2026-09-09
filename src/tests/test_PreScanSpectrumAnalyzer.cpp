#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../math/PreScanSpectrumAnalyzer.h"
#include "../math/LabAnalyticEngine.h"
#include <juce_core/juce_core.h>
#include <cmath>

TEST_CASE ("PreScanSpectrumAnalyzer Noise Sweep Peak Detection", "[math][prescan]")
{
    using namespace abdaudiolab::math;
    using Catch::Matchers::WithinAbs;

    const double sampleRate = 48000.0;
    const float durationSec = 2.0f;
    const int totalSamples = static_cast<int>(durationSec * sampleRate);
    std::vector<float> testBuffer(totalSamples, 0.0f);

    // Simular un barrido con resonancia: un pico dominante que sube de 300 Hz a 4000 Hz
    // con un codo marcado (frecuencia exponencial que se dispara a mitad de tiempo)
    double phase = 0.0;
    for (int i = 0; i < totalSamples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float progress = t / durationSec; // 0.0 a 1.0

        // Curva con codo: primero plana (300 a 600 Hz), luego rápida (600 a 4000 Hz)
        float currentHz = 300.0f + 3700.0f * std::pow(progress, 3.0f);

        double freqPerSample = currentHz / sampleRate;
        phase += 2.0 * juce::MathConstants<double>::pi * freqPerSample;
        if (phase >= 2.0 * juce::MathConstants<double>::pi) phase -= 2.0 * juce::MathConstants<double>::pi;

        // Tono resonante simulado sobre ruido leve
        testBuffer[i] = 0.8f * static_cast<float>(std::sin(phase));
    }

    PreScanResult result = PreScanSpectrumAnalyzer::analyzeFilterNoiseSweep(
        testBuffer, sampleRate, 0.0f, 127.0f, 11, 50.0f
    );

    SECTION ("Trajectory extraction sanity")
    {
        REQUIRE(!result.trajectory.empty());
        REQUIRE(result.trajectory.size() >= 20);

        // Control values must span roughly from 0 to 127
        REQUIRE_THAT(result.trajectory.front().controlValue, WithinAbs(0.0f, 1.0f));
        REQUIRE_THAT(result.trajectory.back().controlValue, WithinAbs(127.0f, 5.0f));

        // Peak frequency must increase monotonically
        REQUIRE(result.trajectory.back().primaryMetric > result.trajectory.front().primaryMetric);
        REQUIRE(result.trajectory.front().primaryMetric >= 200.0f);
        REQUIRE(result.trajectory.back().primaryMetric <= 5000.0f);
    }

    SECTION ("Adaptive roadmap densification at the knee")
    {
        LabAnalyticEngine::computeAdaptiveRoadmap(result, 0.05f);

        REQUIRE(!result.recommendedSteps.empty());
        REQUIRE(result.recommendedSteps.front() == 0);
        REQUIRE(result.recommendedSteps.back() == 127);

        // Como la curva es cúbica, la mayor variación está en el último tercio
        // Contamos cuántos pasos caen en la segunda mitad (> 64) vs primera mitad
        int stepsFirstHalf = 0;
        int stepsSecondHalf = 0;
        for (int step : result.recommendedSteps)
        {
            if (step <= 64) stepsFirstHalf++;
            else stepsSecondHalf++;
        }

        // Debe haber más densidad en la zona del codo no lineal
        REQUIRE(stepsSecondHalf >= stepsFirstHalf);
    }
}

TEST_CASE ("PreScanSpectrumAnalyzer Saturation Tone Sweep THD Detection", "[math][prescan]")
{
    using namespace abdaudiolab::math;

    const double sampleRate = 48000.0;
    const float durationSec = 2.0f;
    const int totalSamples = static_cast<int>(durationSec * sampleRate);
    const float fundamentalHz = 1000.0f;
    std::vector<float> testBuffer(totalSamples, 0.0f);

    // Generar tono 1 kHz que permanece lineal hasta el 50% y luego satura intensamente (soft-clipping)
    double phase = 0.0;
    double freqPerSample = fundamentalHz / sampleRate;

    for (int i = 0; i < totalSamples; ++i)
    {
        phase += 2.0 * juce::MathConstants<double>::pi * freqPerSample;
        if (phase >= 2.0 * juce::MathConstants<double>::pi) phase -= 2.0 * juce::MathConstants<double>::pi;

        float rawSine = 0.5f * static_cast<float>(std::sin(phase));
        float progress = static_cast<float>(i) / static_cast<float>(totalSamples);

        // A partir de progress > 0.5f, amplificamos fuertemente y pasamos por std::tanh
        float drive = 1.0f;
        if (progress > 0.5f)
        {
            drive = 1.0f + 10.0f * (progress - 0.5f) * 2.0f; // Sube de 1.0 a 11.0
        }

        testBuffer[i] = std::tanh(rawSine * drive);
    }

    PreScanResult result = PreScanSpectrumAnalyzer::analyzeSaturationToneSweep(
        testBuffer, sampleRate, fundamentalHz, 0.0f, 127.0f, 11, 50.0f
    );

    SECTION ("THD extraction detects saturation onset")
    {
        REQUIRE(!result.trajectory.empty());

        float initialThd = result.trajectory.front().thdPercent;
        float finalThd = result.trajectory.back().thdPercent;

        // La señal final saturada debe tener sustancialmente mayor THD que la inicial
        REQUIRE(initialThd < 10.0f);
        REQUIRE(finalThd > 20.0f);
        REQUIRE(finalThd > initialThd * 2.0f);
    }

    SECTION ("Adaptive roadmap densifies in saturation knee")
    {
        LabAnalyticEngine::computeAdaptiveRoadmap(result, 0.05f);

        REQUIRE(!result.recommendedSteps.empty());
        REQUIRE(result.recommendedSteps.front() == 0);
        REQUIRE(result.recommendedSteps.back() == 127);

        // Verificar que los pasos adaptativos capturan la zona donde nace la distorsión
        bool hasStepAroundMid = false;
        for (int step : result.recommendedSteps)
        {
            if (step >= 50 && step <= 90)
            {
                hasStepAroundMid = true;
                break;
            }
        }
        REQUIRE(hasStepAroundMid);
    }
}
