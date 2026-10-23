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
    sampleRate.store((newSampleRate > 0.0) ? newSampleRate : 96000.0, std::memory_order_release);
    reset();
}

void LabStimulusGenerator::reset()
{
    playing.store(false, std::memory_order_relaxed);
    finished.store(false, std::memory_order_relaxed);
    currentSampleIndex.store(0, std::memory_order_relaxed);
    totalSamples.store(0, std::memory_order_relaxed);
    pendingNoiseReseed.store(true, std::memory_order_release);
    pinkNoise.reseed(math::PinkNoiseGenerator::defaultSeed);
}

void LabStimulusGenerator::setStimulus(StimulusType type, double durationSeconds, float startFreqHz, float endFreqHz)
{
    currentType.store(type, std::memory_order_release);
    sweepDurationSec.store((durationSeconds > 0.01) ? durationSeconds : 2.0, std::memory_order_release);
    startFreq.store((startFreqHz > 1.0f) ? startFreqHz : 20.0f, std::memory_order_release);
    float sf = startFreq.load(std::memory_order_relaxed);
    double sr = sampleRate.load(std::memory_order_relaxed);
    endFreq.store((endFreqHz > sf) ? endFreqHz : static_cast<float>(sr * 0.49), std::memory_order_release);

    totalSamples.store(static_cast<int64_t>(std::lround(sweepDurationSec.load(std::memory_order_relaxed) * sr)), std::memory_order_relaxed);
    currentSampleIndex.store(0, std::memory_order_relaxed);
    finished.store(false, std::memory_order_relaxed);
    playing.store(true, std::memory_order_release);

    // Lock-free flag: el hilo de audio limpiara el filtro al entrar a su siguiente bloque
    // garantizando cero contencion de mutex en el audio callback de tiempo real.
    pendingNoiseFilterClear.store(true, std::memory_order_release);
    pinkNoise.clearFilterState();
}

void LabStimulusGenerator::processBlock(float* outputBuffer, int numSamples) noexcept
{
    if (!playing.load(std::memory_order_acquire) || finished.load(std::memory_order_acquire) || outputBuffer == nullptr)
    {
        if (outputBuffer != nullptr)
            std::fill_n(outputBuffer, numSamples, 0.0f);
        return;
    }

    // Procesa flags de reinicio de ruido de forma lock-free
    if (pendingNoiseReseed.exchange(false, std::memory_order_acq_rel))
        pinkNoise.reseed(math::PinkNoiseGenerator::defaultSeed);
    else if (pendingNoiseFilterClear.exchange(false, std::memory_order_acq_rel))
        pinkNoise.clearFilterState();

    // Snapshot atomic parameters at block start (lock-free)
    const double sr = sampleRate.load(std::memory_order_relaxed);
    const StimulusType type = currentType.load(std::memory_order_acquire);
    const double sf = startFreq.load(std::memory_order_relaxed);
    const double ef = endFreq.load(std::memory_order_relaxed);
    const double swDur = sweepDurationSec.load(std::memory_order_relaxed);

    const double twoPi = 2.0 * std::numbers::pi;
    const double w1 = twoPi * sf;
    const double w2 = twoPi * ef;
    const double T = swDur;
    const double logRatio = std::log(w2 / w1);
    const double K = (w1 * T) / logRatio;
    const double L = T / logRatio;

    for (int i = 0; i < numSamples; ++i)
    {
        int64_t currentIndex = currentSampleIndex.load(std::memory_order_acquire);
        int64_t total = totalSamples.load(std::memory_order_acquire);
        if (currentIndex >= total)
        {
            outputBuffer[i] = 0.0f;
            playing.store(false, std::memory_order_release);
            finished.store(true, std::memory_order_release);
            continue;
        }

        double t = static_cast<double>(currentIndex) / sr;
        float sampleVal = 0.0f;

        switch (type)
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

                // Hann window on first and last 2ms to prevent start/end click transients without attenuating sweep band
                double windowTime = 0.002; // 2ms
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
                sampleVal = pinkNoise.nextWhite() * 0.707f; // -3dB headroom
                break;
            }

            case StimulusType::PinkNoise:
            {
                sampleVal = pinkNoise.nextPink();
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
                    sampleVal = (step % 2 == 0 ? pinkNoise.nextPink() : (pinkNoise.nextWhite() * 0.707f)) * amp;
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

            case StimulusType::MetronomeTick:
            {
                // Rhythmic click for manual calibration: 800 Hz sinusoid with 15ms Hann window at -24 dBFS
                const double clickDurationSec = 0.015;
                if (t >= 0.0 && t < clickDurationSec)
                {
                    double phase = twoPi * 800.0 * t;
                    double hann = 0.5 * (1.0 - std::cos(twoPi * (t / clickDurationSec)));
                    sampleVal = static_cast<float>(std::sin(phase) * hann * 0.0630957); // -24 dBFS
                }
                else
                {
                    sampleVal = 0.0f;
                }
                break;
            }

            case StimulusType::LevelLadderSweep:
            {
                // 4-Stage Multi-Amplitude Swept Sine Ladder (-24 dBFS, -12 dBFS, -6 dBFS, -1 dBFS)
                const double stageDuration = T / 4.0;
                const int stage = juce::jlimit(0, 3, static_cast<int>(t / stageDuration));
                const double tInStage = std::fmod(t, stageDuration);

                // Gains for ladder: -24 dB, -12 dB, -6 dB, -1 dB
                constexpr float ladderGains[4] = { 0.0630957f, 0.2511886f, 0.5011872f, 0.8912509f };
                const float stageGain = ladderGains[stage];

                const double K_step = (w1 * stageDuration) / logRatio;
                const double L_step = stageDuration / logRatio;
                const double phase = K_step * (std::exp(tInStage / L_step) - 1.0);

                sampleVal = static_cast<float>(std::sin(phase) * stageGain);

                // 2ms Hann window at step boundaries to avoid clicks
                constexpr double winTime = 0.002;
                if (tInStage < winTime)
                {
                    const double win = 0.5 * (1.0 - std::cos(std::numbers::pi * (tInStage / winTime)));
                    sampleVal *= static_cast<float>(win);
                }
                else if (tInStage > (stageDuration - winTime))
                {
                    const double win = 0.5 * (1.0 - std::cos(std::numbers::pi * ((stageDuration - tInStage) / winTime)));
                    sampleVal *= static_cast<float>(win);
                }
                break;
            }

            case StimulusType::MultitoneIMD:
            {
                // 31 non-harmonic tones with Schroeder phase distribution for minimum crest factor
                static constexpr float multitoneFreqs[31] = {
                    23.0f, 31.0f, 43.0f, 59.0f, 83.0f, 113.0f, 157.0f, 223.0f,
                    311.0f, 439.0f, 617.0f, 863.0f, 1213.0f, 1699.0f, 2381.0f,
                    3329.0f, 4657.0f, 6521.0f, 9127.0f, 11447.0f, 13103.0f,
                    14557.0f, 15823.0f, 16901.0f, 17827.0f, 18587.0f, 19183.0f,
                    19609.0f, 19867.0f, 19961.0f, 19997.0f
                };

                double acc = 0.0;
                for (int k = 0; k < 31; ++k)
                {
                    // Schroeder phase: phi_k = pi * k^2 / M
                    const double schroederPhase = (std::numbers::pi * k * k) / 31.0;
                    acc += std::cos(twoPi * multitoneFreqs[k] * t + schroederPhase);
                }

                // Normalize: 0.707 / sqrt(31) to maintain ~ -3 dBFS peak without clipping
                sampleVal = static_cast<float>(acc * (0.70710678 / 5.56776436));

                // 5ms Hann window at start and end
                constexpr double winTime = 0.005;
                if (t < winTime)
                {
                    const double win = 0.5 * (1.0 - std::cos(std::numbers::pi * (t / winTime)));
                    sampleVal *= static_cast<float>(win);
                }
                else if (t > (T - winTime))
                {
                    const double win = 0.5 * (1.0 - std::cos(std::numbers::pi * ((T - t) / winTime)));
                    sampleVal *= static_cast<float>(win);
                }
                break;
            }
        }

        outputBuffer[i] = sampleVal;
        currentSampleIndex.fetch_add(1, std::memory_order_relaxed);
    }
}

void LabStimulusGenerator::renderMetronomeTick(float* destinationBuffer, 
                                              int numSamples, 
                                              double sampleRate, 
                                              double clickDurationSec,
                                              float clickFreqHz,
                                              float gainLinear) noexcept
{
    if (destinationBuffer == nullptr || numSamples <= 0)
        return;

    const double sr = (sampleRate > 0.0) ? sampleRate : 96000.0;
    const double twoPi = 2.0 * std::numbers::pi;
    const int clickSamples = static_cast<int>(std::lround(clickDurationSec * sr));
    const int samplesToWrite = std::min(numSamples, clickSamples);

    for (int i = 0; i < samplesToWrite; ++i)
    {
        double t = static_cast<double>(i) / sr;
        double phase = twoPi * static_cast<double>(clickFreqHz) * t;
        double hann = 0.5 * (1.0 - std::cos(twoPi * (static_cast<double>(i) / static_cast<double>(clickSamples))));
        destinationBuffer[i] += static_cast<float>(std::sin(phase) * hann * static_cast<double>(gainLinear));
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

std::vector<float> LabStimulusGenerator::generateSyncPulses3Pattern(double sampleRate)
{
    const double sr = (sampleRate > 0.0) ? sampleRate : 96000.0;
    const double durationSeconds = 0.300; // 300ms total
    const int totalSamples = static_cast<int>(std::lround(durationSeconds * sr));
    std::vector<float> pattern(static_cast<size_t>(totalSamples), 0.0f);

    LabStimulusGenerator gen;
    gen.prepare(sr);
    gen.setStimulus(StimulusType::SyncPulses3, durationSeconds);

    const int blockSize = 512;
    int samplesGenerated = 0;
    while (samplesGenerated < totalSamples)
    {
        int toGen = std::min(blockSize, totalSamples - samplesGenerated);
        gen.processBlock(pattern.data() + samplesGenerated, toGen);
        samplesGenerated += toGen;
    }

    return pattern;
}

} // namespace abdaudiolab::audio
