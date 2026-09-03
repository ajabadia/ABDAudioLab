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
    playing.store(false, std::memory_order_relaxed);
    finished.store(false, std::memory_order_relaxed);
    currentSampleIndex.store(0, std::memory_order_relaxed);
    totalSamples.store(0, std::memory_order_relaxed);
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    randomSeed = 0x48271983;
}

void LabStimulusGenerator::setStimulus(StimulusType type, double durationSeconds, float startFreqHz, float endFreqHz)
{
    currentType = type;
    sweepDurationSec = (durationSeconds > 0.01) ? durationSeconds : 2.0;
    startFreq = (startFreqHz > 1.0f) ? startFreqHz : 20.0f;
    endFreq = (endFreqHz > startFreq) ? endFreqHz : static_cast<float>(sampleRate * 0.49);

    totalSamples.store(static_cast<int64_t>(std::lround(sweepDurationSec * sampleRate)), std::memory_order_relaxed);
    currentSampleIndex.store(0, std::memory_order_relaxed);
    finished.store(false, std::memory_order_relaxed);
    playing.store(true, std::memory_order_release);
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
}

void LabStimulusGenerator::processBlock(float* outputBuffer, int numSamples) noexcept
{
    if (!playing.load(std::memory_order_relaxed) || finished.load(std::memory_order_relaxed) || outputBuffer == nullptr)
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
        int64_t currentIndex = currentSampleIndex.load(std::memory_order_relaxed);
        int64_t total = totalSamples.load(std::memory_order_relaxed);
        if (currentIndex >= total)
        {
            outputBuffer[i] = 0.0f;
            playing.store(false, std::memory_order_relaxed);
            finished.store(true, std::memory_order_release);
            continue;
        }

        double t = static_cast<double>(currentIndex) / sampleRate;
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
                sampleVal = (currentIndex == 0) ? 1.0f : 0.0f;
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

            case StimulusType::NamCalibration:
            {
                // Multi-Stage NAM / RTNeural Calibration Sequence
                if (t < 0.24)
                {
                    // 1. Pre-roll 3 sync pulses (1kHz 40ms tone, 40ms silence)
                    double cycleTime = 0.080;
                    double tInCycle = std::fmod(t, cycleTime);
                    if (tInCycle < 0.040)
                    {
                        double phase = twoPi * 1000.0 * t;
                        float win = 1.0f;
                        if (tInCycle < 0.005)
                            win = static_cast<float>(0.5 * (1.0 - std::cos(std::numbers::pi * (tInCycle / 0.005))));
                        else if (tInCycle > 0.035)
                            win = static_cast<float>(0.5 * (1.0 - std::cos(std::numbers::pi * ((0.040 - tInCycle) / 0.005))));
                        sampleVal = static_cast<float>(std::sin(phase) * 0.707 * win);
                    }
                }
                else if (t < 0.40)
                {
                    // 2. Pre-roll silence
                    sampleVal = 0.0f;
                }
                else if (t < 3.0)
                {
                    // 3. Logarithmic Sine Sweep (20Hz to 20kHz)
                    double sweepT = 2.6; // 0.4 to 3.0
                    double sweepElapsed = t - 0.4;
                    double f1 = 20.0;
                    double f2 = 20000.0;
                    double namK = (sweepT * twoPi * f1) / std::log(f2 / f1);
                    double namL = sweepT / std::log(f2 / f1);
                    double phase = namK * (std::exp(sweepElapsed / namL) - 1.0);
                    sampleVal = static_cast<float>(std::sin(phase) * 0.707);
                }
                else if (t < 5.5)
                {
                    // 4. Stepped Noise Bursts
                    double noiseElapsed = t - 3.0;
                    int step = static_cast<int>(noiseElapsed / 0.625);
                    float amp = (step == 0) ? 0.125f : ((step == 1) ? 0.25f : ((step == 2) ? 0.5f : 0.85f));
                    sampleVal = (step % 2 == 0 ? getNextPinkNoise() : (getNextWhiteNoise() * 0.707f)) * amp;
                }
                else if (t < 8.0)
                {
                    // 5. Rich Multi-Harmonics & Pulse Trains (110Hz, 220Hz, 440Hz with drive)
                    double harmElapsed = t - 5.5;
                    double baseFreq = (harmElapsed < 0.8) ? 110.0 : ((harmElapsed < 1.6) ? 220.0 : 440.0);
                    double p = std::fmod(harmElapsed * baseFreq, 1.0);
                    // Band-limited pseudo-saw/pulse with soft saturation
                    float saw = static_cast<float>(2.0 * p - 1.0);
                    float pulse = (p < 0.3) ? 0.8f : -0.8f;
                    float combined = (saw + pulse) * 0.6f;
                    sampleVal = std::tanh(combined * 1.5f) * 0.707f;
                }
                else if (t < 8.3)
                {
                    // 6. Post-roll 1kHz alignment marker
                    double postT = t - 8.0;
                    if (postT < 0.050)
                    {
                        double phase = twoPi * 1000.0 * postT;
                        sampleVal = static_cast<float>(std::sin(phase) * 0.707);
                    }
                }
                else
                {
                    sampleVal = 0.0f;
                }
                break;
            }
        }

        outputBuffer[i] = sampleVal;
        currentSampleIndex.fetch_add(1, std::memory_order_relaxed);
    }
}

juce::AudioBuffer<float> LabStimulusGenerator::generateNamCalibrationBuffer(double sampleRate, double durationSeconds)
{
    const int totalSamples = static_cast<int>(sampleRate * durationSeconds);
    juce::AudioBuffer<float> buffer(1, totalSamples);

    LabStimulusGenerator gen;
    gen.prepare(sampleRate);
    gen.setStimulus(StimulusType::NamCalibration, durationSeconds);

    const int blockSize = 512;
    int samplesGenerated = 0;
    while (samplesGenerated < totalSamples)
    {
        int toGen = std::min(blockSize, totalSamples - samplesGenerated);
        gen.processBlock(buffer.getWritePointer(0, samplesGenerated), toGen);
        samplesGenerated += toGen;
    }

    return buffer;
}

} // namespace abdaudiolab::audio
