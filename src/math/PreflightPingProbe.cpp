#include "PreflightPingProbe.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace abdaudiolab::math
{

std::vector<float> PreflightPingProbe::generateStimulus (double sampleRate,
                                                         float durationSec,
                                                         float freqHz,
                                                         float targetDbfs,
                                                         float rampMs)
{
    if (sampleRate <= 0.0 || durationSec <= 0.0f)
        return {};

    const size_t totalSamples = static_cast<size_t>(sampleRate * durationSec);
    const size_t rampSamples = static_cast<size_t>(sampleRate * (rampMs / 1000.0f));
    const float amplitude = std::pow (10.0f, targetDbfs / 20.0f);

    std::vector<float> stimulus (totalSamples, 0.0f);
    const double twoPiFreq = 2.0 * M_PI * static_cast<double>(freqHz);

    for (size_t i = 0; i < totalSamples; ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const double rawSine = std::sin (twoPiFreq * t) * static_cast<double>(amplitude);

        double window = 1.0;
        if (rampSamples > 0 && i < rampSamples)
        {
            // Raised-cosine fade-in
            window = 0.5 * (1.0 - std::cos (M_PI * static_cast<double>(i) / static_cast<double>(rampSamples)));
        }
        else if (rampSamples > 0 && i >= (totalSamples - rampSamples))
        {
            // Raised-cosine fade-out
            const size_t tailIdx = totalSamples - 1 - i;
            window = 0.5 * (1.0 - std::cos (M_PI * static_cast<double>(tailIdx) / static_cast<double>(rampSamples)));
        }

        stimulus[i] = static_cast<float>(rawSine * window);
    }

    return stimulus;
}

double PreflightPingProbe::computeGoertzelPower (const float* buffer,
                                                 size_t numSamples,
                                                 double sampleRate,
                                                 double targetFreqHz)
{
    if (buffer == nullptr || numSamples < 4 || sampleRate <= 0.0 || targetFreqHz <= 0.0)
        return 0.0;

    const double n = static_cast<double>(numSamples);
    const double k = std::round (n * targetFreqHz / sampleRate);
    const double omega = (2.0 * M_PI * k) / n;
    const double coeff = 2.0 * std::cos (omega);

    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;

    const double denom = (n > 1.0) ? (n - 1.0) : 1.0;

    for (size_t i = 0; i < numSamples; ++i)
    {
        // Standard Hann window for side-lobe suppression
        const double w = 0.5 * (1.0 - std::cos (2.0 * M_PI * static_cast<double>(i) / denom));
        const double xw = static_cast<double>(buffer[i]) * w;

        s0 = xw + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return std::max (0.0, power);
}

PreflightMetrics PreflightPingProbe::analyzeResponse (const float* buffer,
                                                      size_t numSamples,
                                                      double sampleRate,
                                                      float expectedFreqHz)
{
    PreflightMetrics metrics;
    metrics.fundamentalFreqHz = expectedFreqHz;

    if (buffer == nullptr || numSamples < 64 || sampleRate <= 0.0)
    {
        metrics.status = PreflightStatus::Fail_NoSignal;
        metrics.peakDbfs = -100.0f;
        metrics.rmsDbfs = -100.0f;
        metrics.humEnergyRatioDbc = -100.0f;
        metrics.diagnosticMessage = "Insufficient sample data recorded for preflight diagnostic.";
        return metrics;
    }

    float peakVal = 0.0f;
    double sumSquares = 0.0;

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float absVal = std::abs (buffer[i]);
        if (absVal > peakVal)
            peakVal = absVal;

        sumSquares += static_cast<double>(absVal) * static_cast<double>(absVal);
    }

    const double rmsVal = std::sqrt (sumSquares / static_cast<double>(numSamples));
    metrics.peakDbfs = 20.0f * std::log10 (std::max (peakVal, 1e-7f));
    metrics.rmsDbfs = 20.0f * std::log10 (std::max (static_cast<float>(rmsVal), 1e-7f));

    // 1. Continuity Check (Missing Signal)
    if (metrics.rmsDbfs <= -60.0f)
    {
        metrics.status = PreflightStatus::Fail_NoSignal;
        metrics.humEnergyRatioDbc = -100.0f;
        metrics.diagnosticMessage = "No signal detected (RMS <= -60 dBFS). Verify cables, power, and gain settings.";
        return metrics;
    }

    // 2. Headroom & Saturation Check (Clipping)
    if (metrics.peakDbfs >= -0.5f)
    {
        metrics.status = PreflightStatus::Fail_Clipping;
        metrics.diagnosticMessage = "Clipping / saturation detected (Peak >= -0.5 dBFS). Reduce interface output gain or hardware input trim.";
        return metrics;
    }

    // 3. Spectral Hum & Ground Loop Check (50 Hz / 60 Hz coherence)
    const double fundamentalPower = computeGoertzelPower (buffer, numSamples, sampleRate, expectedFreqHz);
    const double hum50Power = computeGoertzelPower (buffer, numSamples, sampleRate, 50.0);
    const double hum60Power = computeGoertzelPower (buffer, numSamples, sampleRate, 60.0);
    const double maxHumPower = std::max (hum50Power, hum60Power);

    if (fundamentalPower > 1e-12 && maxHumPower > 1e-12)
    {
        metrics.humEnergyRatioDbc = static_cast<float>(10.0 * std::log10 (maxHumPower / fundamentalPower));
    }
    else
    {
        metrics.humEnergyRatioDbc = -100.0f;
    }

    if (metrics.humEnergyRatioDbc > -24.0f)
    {
        metrics.status = PreflightStatus::Fail_GroundLoopHum;
        metrics.diagnosticMessage = "Severe ground loop / mains hum detected (50/60 Hz ratio > -24 dBc). Check grounding, power supplies, or use balanced cables.";
        return metrics;
    }

    // 4. Optimal Dynamic Window Check (-24 dBFS .. -6 dBFS)
    if (metrics.peakDbfs < -24.0f)
    {
        metrics.status = PreflightStatus::Warning_LowLevel;
        metrics.diagnosticMessage = "Signal level is low (Peak < -24 dBFS). Consider increasing hardware or preamp gain for optimal signal-to-noise ratio.";
    }
    else if (metrics.peakDbfs > -6.0f)
    {
        metrics.status = PreflightStatus::Warning_HighLevel;
        metrics.diagnosticMessage = "Signal level is high (Peak > -6 dBFS). Consider attenuating slightly to ensure sufficient dynamic headroom.";
    }
    else
    {
        metrics.status = PreflightStatus::Pass;
        metrics.diagnosticMessage = "Preflight signal verified. Headroom, continuity, and hum checks passed.";
    }

    return metrics;
}

} // namespace abdaudiolab::math
