#include "LabStimulusGenerator.h"
#include <cmath>
#include <numbers>

namespace abdaudiolab::audio
{

LabStimulusGenerator::LabStimulusGenerator()
{
    reset();
}

void LabStimulusGenerator::prepare(double newSampleRate)
{
    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 96000.0;
    reset();
}

void LabStimulusGenerator::reset()
{
    playing = false;
    finished = false;
    currentSampleIndex = 0;
    totalSamples = 0;
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    randomSeed = 0x48271983;
}

void LabStimulusGenerator::setStimulus(StimulusType type, double durationSeconds, float startFreqHz, float endFreqHz)
{
    currentType = type;
    sweepDurationSec = (durationSeconds > 0.01) ? durationSeconds : 2.0;
    startFreq = (startFreqHz > 1.0f) ? startFreqHz : 20.0f;
    endFreq = (endFreqHz > startFreq) ? endFreqHz : static_cast<float>(sampleRate * 0.49);

    totalSamples = static_cast<int64_t>(std::lround(sweepDurationSec * sampleRate));
    currentSampleIndex = 0;
    finished = false;
    playing = true;
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
}

void LabStimulusGenerator::processBlock(float* outputBuffer, int numSamples) noexcept
{
    if (!playing || finished || outputBuffer == nullptr)
    {
        if (outputBuffer != nullptr)
            std::fill_n(outputBuffer, numSamples, 0.0f);
        return;
    }

    const double twoPi = 2.0 * std::numbers::pi;
    const double w1 = twoPi * startFreq;
    const double w2 = twoPi * endFreq;
    const double T = sweepDurationSec;
    const double logRatio = std::log(w2 / w1);
    const double K = (w1 * T) / logRatio;
    const double L = T / logRatio;

    for (int i = 0; i < numSamples; ++i)
    {
        if (currentSampleIndex >= totalSamples)
        {
            outputBuffer[i] = 0.0f;
            playing = false;
            finished = true;
            continue;
        }

        double t = static_cast<double>(currentSampleIndex) / sampleRate;
        float sampleVal = 0.0f;

        switch (currentType)
        {
            case StimulusType::Silence:
            {
                sampleVal = 0.0f;
                break;
            }

            case StimulusType::DiracDelta:
            {
                // Single 0 dBfs impulse at sample 0, followed by absolute silence
                sampleVal = (currentSampleIndex == 0) ? 1.0f : 0.0f;
                break;
            }

            case StimulusType::SyncPulses3:
            {
                // Pre-Roll Calibration & Alignment (3 synchronized 1kHz tone bursts of 40ms)
                // Period = 80ms (40ms tone + 40ms silence), 3 cycles = 240ms, total pre-roll = 300ms
                double cycleTime = 0.080; // 80ms
                int cycleIdx = static_cast<int>(t / cycleTime);
                double tInCycle = std::fmod(t, cycleTime);

                if (cycleIdx < 3 && tInCycle < 0.040)
                {
                    double phase = twoPi * 1000.0 * t;
                    // Apply smooth 5ms Hann envelope on each burst to avoid clicks
                    float win = 1.0f;
                    if (tInCycle < 0.005)
                        win = static_cast<float>(0.5 * (1.0 - std::cos(std::numbers::pi * (tInCycle / 0.005))));
                    else if (tInCycle > 0.035)
                        win = static_cast<float>(0.5 * (1.0 - std::cos(std::numbers::pi * ((0.040 - tInCycle) / 0.005))));

                    sampleVal = static_cast<float>(std::sin(phase) * 0.707 * win); // -3 dBfs
                }
                else
                {
                    sampleVal = 0.0f;
                }
                break;
            }

            case StimulusType::SineWave1kHz:
            {
                double phase = twoPi * 1000.0 * t;
                sampleVal = static_cast<float>(std::sin(phase));
                break;
            }

            case StimulusType::SquareWave1kHz:
            {
                double phase = twoPi * 1000.0 * t;
                sampleVal = (std::sin(phase) >= 0.0) ? 0.707f : -0.707f;
                break;
            }

            case StimulusType::LogFarinaSweep:
            {
                // Exact Farina (2000) Logarithmic Swept-Sine formula:
                // x(t) = sin( K * ( exp(t / L) - 1 ) )
                double exponent = t / L;
                double phase = K * (std::exp(exponent) - 1.0);
                sampleVal = static_cast<float>(std::sin(phase));

                // Optional Hann window on first and last 20ms to prevent start/end click transients
                double windowTime = 0.02; // 20ms
                if (t < windowTime)
                {
                    double win = 0.5 * (1.0 - std::cos(std::numbers::pi * (t / windowTime)));
                    sampleVal *= static_cast<float>(win);
                }
                else if (t > (T - windowTime))
                {
                    double win = 0.5 * (1.0 - std::cos(std::numbers::pi * ((T - t) / windowTime)));
                    sampleVal *= static_cast<float>(win);
                }
                break;
            }

            case StimulusType::WhiteNoise:
            {
                sampleVal = getNextWhiteNoise() * 0.707f; // -3dB headroom
                break;
            }

            case StimulusType::PinkNoise:
            {
                sampleVal = getNextPinkNoise();
                break;
            }

            case StimulusType::AmplitudeRamp:
            {
                // 1 kHz sine wave scaled by linear amplitude ramp (0.0 to 1.0)
                float amp = static_cast<float>(t / T);
                double phase = twoPi * 1000.0 * t;
                sampleVal = amp * static_cast<float>(std::sin(phase));
                break;
            }
        }

        outputBuffer[i] = sampleVal;
        currentSampleIndex++;
    }
}

} // namespace abdaudiolab::audio
