#include "synth/HoldoutStimulusGenerator.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace abdaudiolab::synth
{

std::vector<float> HoldoutStimulusGenerator::generateDynamicAmplitudeSteps(double sampleRate,
                                                                          double fundamentalHz,
                                                                          double stepDurationSec,
                                                                          double transitionSec)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 96000.0;
    const std::vector<double> levelsDbfs = { -30.0, -20.0, -12.0, -6.0, -3.0 };
    const size_t stepSamples = static_cast<size_t>(std::max(0.01, stepDurationSec) * sr);
    const size_t transSamples = static_cast<size_t>(std::max(0.001, transitionSec) * sr);
    const size_t totalSamples = stepSamples * levelsDbfs.size();

    std::vector<float> buffer(totalSamples, 0.0f);
    const double omega = 2.0 * std::numbers::pi * fundamentalHz / sr;
    double phase = 0.0;

    for (size_t s = 0; s < levelsDbfs.size(); ++s)
    {
        const double targetLinear = std::pow(10.0, levelsDbfs[s] / 20.0);
        const double prevLinear = (s > 0) ? std::pow(10.0, levelsDbfs[s - 1] / 20.0) : 0.0;
        const size_t offset = s * stepSamples;

        for (size_t i = 0; i < stepSamples; ++i)
        {
            double gain = targetLinear;
            if (i < transSamples)
            {
                // Rampa cosenoidal de Hann entre nivel anterior y nivel actual
                double tNorm = static_cast<double>(i) / static_cast<double>(transSamples);
                double w = 0.5 * (1.0 - std::cos(std::numbers::pi * tNorm));
                gain = prevLinear + (targetLinear - prevLinear) * w;
            }
            else if (s == levelsDbfs.size() - 1 && i >= (stepSamples - transSamples))
            {
                // Fade-out final al término del último escalón
                size_t fadeIdx = i - (stepSamples - transSamples);
                double tNorm = static_cast<double>(fadeIdx) / static_cast<double>(transSamples);
                double w = 0.5 * (1.0 + std::cos(std::numbers::pi * tNorm));
                gain = targetLinear * w;
            }

            buffer[offset + i] = static_cast<float>(gain * std::sin(phase));
            phase += omega;
            if (phase >= 2.0 * std::numbers::pi)
                phase -= 2.0 * std::numbers::pi;
        }
    }

    return buffer;
}

std::vector<float> HoldoutStimulusGenerator::generateTransientBursts(double sampleRate,
                                                                    int burstCount,
                                                                    double burstDurationSec,
                                                                    double restDurationSec,
                                                                    double carrierHz)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 96000.0;
    const int count = std::max(1, burstCount);
    const size_t burstSamples = static_cast<size_t>(std::max(0.005, burstDurationSec) * sr);
    const size_t restSamples = static_cast<size_t>(std::max(0.005, restDurationSec) * sr);
    const size_t periodSamples = burstSamples + restSamples;
    const size_t totalSamples = periodSamples * static_cast<size_t>(count);

    std::vector<float> buffer(totalSamples, 0.0f);
    const double omega = 2.0 * std::numbers::pi * carrierHz / sr;
    const double targetGain = std::pow(10.0, -3.0 / 20.0); // -3 dBFS
    const size_t attackSamples = static_cast<size_t>(0.001 * sr); // Ataque ultra-rápido de 1 ms
    const size_t releaseSamples = static_cast<size_t>(0.004 * sr); // Relajación de 4 ms

    for (int b = 0; b < count; ++b)
    {
        const size_t offset = static_cast<size_t>(b) * periodSamples;
        double phase = 0.0;

        for (size_t i = 0; i < burstSamples; ++i)
        {
            double env = 1.0;
            if (i < attackSamples)
            {
                double t = static_cast<double>(i) / static_cast<double>(attackSamples);
                env = 0.5 * (1.0 - std::cos(std::numbers::pi * t));
            }
            else if (i >= burstSamples - releaseSamples)
            {
                size_t relIdx = i - (burstSamples - releaseSamples);
                double t = static_cast<double>(relIdx) / static_cast<double>(releaseSamples);
                env = 0.5 * (1.0 + std::cos(std::numbers::pi * t));
            }

            buffer[offset + i] = static_cast<float>(targetGain * env * std::sin(phase));
            phase += omega;
            if (phase >= 2.0 * std::numbers::pi)
                phase -= 2.0 * std::numbers::pi;
        }
        // El periodo restSamples se mantiene en 0.0f por defecto
    }

    return buffer;
}

std::vector<float> HoldoutStimulusGenerator::generateDenseMultitone(double sampleRate,
                                                                   double durationSec,
                                                                   const std::vector<double>& frequencies,
                                                                   double targetPeakDbfs)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 96000.0;
    const size_t totalSamples = static_cast<size_t>(std::max(0.05, durationSec) * sr);
    if (frequencies.empty())
        return std::vector<float>(totalSamples, 0.0f);

    std::vector<float> buffer(totalSamples, 0.0f);
    const size_t numTones = frequencies.size();

    // Distribución de fases según Schroeder para minimización del factor de cresta:
    // theta_k = pi * k^2 / M
    std::vector<double> phases(numTones);
    for (size_t k = 0; k < numTones; ++k)
    {
        phases[k] = (std::numbers::pi * static_cast<double>(k * k)) / static_cast<double>(numTones);
    }

    double maxAbs = 0.0;
    for (size_t n = 0; n < totalSamples; ++n)
    {
        double t = static_cast<double>(n) / sr;
        double sampleSum = 0.0;
        for (size_t k = 0; k < numTones; ++k)
        {
            double angle = 2.0 * std::numbers::pi * frequencies[k] * t + phases[k];
            sampleSum += std::cos(angle);
        }
        buffer[n] = static_cast<float>(sampleSum);
        double absVal = std::abs(sampleSum);
        if (absVal > maxAbs)
            maxAbs = absVal;
    }

    // Normalizar al nivel de pico objetivo (evitando clipping)
    const double targetPeakLinear = std::pow(10.0, targetPeakDbfs / 20.0);
    const double normScale = (maxAbs > 1e-12) ? (targetPeakLinear / maxAbs) : 0.0;

    // Ventana de fundido en extremos de 5 ms para arranque y parada sin transitorios de corte
    const size_t fadeSamples = static_cast<size_t>(0.005 * sr);
    for (size_t n = 0; n < totalSamples; ++n)
    {
        double env = 1.0;
        if (n < fadeSamples)
        {
            double t = static_cast<double>(n) / static_cast<double>(fadeSamples);
            env = 0.5 * (1.0 - std::cos(std::numbers::pi * t));
        }
        else if (n >= totalSamples - fadeSamples)
        {
            size_t relIdx = n - (totalSamples - fadeSamples);
            double t = static_cast<double>(relIdx) / static_cast<double>(fadeSamples);
            env = 0.5 * (1.0 + std::cos(std::numbers::pi * t));
        }

        buffer[n] = static_cast<float>(buffer[n] * normScale * env);
    }

    return buffer;
}

std::vector<float> HoldoutStimulusGenerator::generatePseudoMusicalSequence(double sampleRate,
                                                                          double durationSec,
                                                                          double rootHz)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 96000.0;
    const size_t totalSamples = static_cast<size_t>(std::max(0.1, durationSec) * sr);
    std::vector<float> buffer(totalSamples, 0.0f);

    // Acorde compuesto armónico con fundamental, tercera menor, quinta y octava
    const double f0 = rootHz > 20.0 ? rootHz : 220.0;
    const std::vector<double> partialFreqs = { f0, f0 * 1.20, f0 * 1.50, f0 * 2.0, f0 * 3.0 };
    const std::vector<double> partialDecays = { 0.25, 0.18, 0.15, 0.10, 0.06 }; // segundos
    const std::vector<double> partialGains  = { 1.00, 0.60, 0.50, 0.35, 0.20 };

    const size_t attackSamples = static_cast<size_t>(0.002 * sr); // Ataque de 2 ms
    double maxAbs = 0.0;

    for (size_t n = 0; n < totalSamples; ++n)
    {
        double t = static_cast<double>(n) / sr;
        double envAttack = 1.0;
        if (n < attackSamples)
        {
            double ta = static_cast<double>(n) / static_cast<double>(attackSamples);
            envAttack = 0.5 * (1.0 - std::cos(std::numbers::pi * ta));
        }

        double val = 0.0;
        for (size_t p = 0; p < partialFreqs.size(); ++p)
        {
            double decayRate = 1.0 / partialDecays[p];
            double envDecay = std::exp(-decayRate * t);
            double angle = 2.0 * std::numbers::pi * partialFreqs[p] * t;
            val += partialGains[p] * envAttack * envDecay * std::sin(angle);
        }

        buffer[n] = static_cast<float>(val);
        double absVal = std::abs(val);
        if (absVal > maxAbs)
            maxAbs = absVal;
    }

    const double targetPeakLinear = std::pow(10.0, -3.0 / 20.0);
    const double normScale = (maxAbs > 1e-12) ? (targetPeakLinear / maxAbs) : 0.0;

    for (size_t n = 0; n < totalSamples; ++n)
        buffer[n] = static_cast<float>(buffer[n] * normScale);

    return buffer;
}

StimulusAcousticProperties HoldoutStimulusGenerator::analyzeAcousticProperties(const std::vector<float>& buffer,
                                                                               double sampleRate)
{
    StimulusAcousticProperties props;
    props.sampleCount = buffer.size();
    const double sr = sampleRate > 0.0 ? sampleRate : 96000.0;
    props.durationSeconds = static_cast<double>(props.sampleCount) / sr;

    if (buffer.empty())
        return props;

    double sumSq = 0.0;
    double maxAbs = 0.0;

    for (float sample : buffer)
    {
        double s = static_cast<double>(sample);
        double absVal = std::abs(s);
        if (absVal > maxAbs)
            maxAbs = absVal;
        sumSq += s * s;
    }

    props.peakAmplitude = maxAbs;
    props.peakDbfs = (maxAbs > 1e-12) ? (20.0 * std::log10(maxAbs)) : -120.0;

    double meanSq = sumSq / static_cast<double>(props.sampleCount);
    props.rmsAmplitude = std::sqrt(meanSq);
    props.rmsDbfs = (props.rmsAmplitude > 1e-12) ? (20.0 * std::log10(props.rmsAmplitude)) : -120.0;

    if (props.rmsAmplitude > 1e-12)
    {
        props.crestFactor = props.peakAmplitude / props.rmsAmplitude;
        props.crestFactorDb = 20.0 * std::log10(props.crestFactor);
    }

    return props;
}

} // namespace abdaudiolab::synth
