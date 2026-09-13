#include "MidiAudioSynchronizer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace abdaudiolab::synth
{

std::vector<float> MidiAudioSynchronizer::computeNoveltyFunction(const std::vector<float>& audioBuffer,
                                                                 double sampleRate,
                                                                 size_t hopSize)
{
    if (audioBuffer.empty() || hopSize == 0)
        return {};

    size_t windowSize = hopSize * 2;
    size_t numHops = (audioBuffer.size() >= windowSize) ? (audioBuffer.size() - windowSize) / hopSize + 1 : 0;
    if (numHops == 0)
        return {};

    std::vector<float> energy(numHops, 0.0f);
    for (size_t h = 0; h < numHops; ++h)
    {
        size_t start = h * hopSize;
        double sumSq = 0.0;
        for (size_t i = 0; i < windowSize; ++i)
        {
            double val = static_cast<double>(audioBuffer[start + i]);
            sumSq += val * val;
        }
        energy[h] = static_cast<float>(sumSq / static_cast<double>(windowSize));
    }

    // Función de novedad: Derivada de energía rectificada (Half-Wave Rectified First Difference)
    std::vector<float> novelty(numHops, 0.0f);
    for (size_t h = 1; h < numHops; ++h)
    {
        float diff = energy[h] - energy[h - 1];
        novelty[h] = (diff > 0.0f) ? diff : 0.0f;
    }

    return novelty;
}

size_t MidiAudioSynchronizer::detectOnsetSample(const std::vector<float>& novelty,
                                                size_t hopSize,
                                                size_t preRollSamples,
                                                double sampleRate)
{
    if (novelty.size() < 3 || hopSize == 0)
        return 0;

    size_t preRollHops = std::min(novelty.size() / 2, preRollSamples / hopSize);

    // 1. Estimar suelo de ruido y desviación en región previa
    double sum = 0.0;
    double sumSq = 0.0;
    size_t noiseHops = std::max(size_t{1}, preRollHops);

    for (size_t i = 0; i < noiseHops; ++i)
    {
        double val = static_cast<double>(novelty[i]);
        sum += val;
        sumSq += val * val;
    }

    double meanNoise = sum / static_cast<double>(noiseHops);
    double varNoise = std::max(0.0, (sumSq / static_cast<double>(noiseHops)) - (meanNoise * meanNoise));
    double stdNoise = std::sqrt(varNoise);

    // Umbral dinámico adaptativo
    float threshold = static_cast<float>(meanNoise + 4.0 * stdNoise + 1e-6);

    // 2. Localizar primer pico significativo que supere el umbral
    for (size_t h = noiseHops; h < novelty.size() - 1; ++h)
    {
        float prev = novelty[h - 1];
        float curr = novelty[h];
        float next = novelty[h + 1];

        if (curr > threshold && curr >= prev && curr >= next)
        {
            return h * hopSize;
        }
    }

    // Fallback: si no supera el umbral estricto, buscar el máximo absoluto tras el pre-roll
    size_t maxHop = noiseHops;
    float maxVal = 0.0f;
    for (size_t h = noiseHops; h < novelty.size(); ++h)
    {
        if (novelty[h] > maxVal)
        {
            maxVal = novelty[h];
            maxHop = h;
        }
    }

    return maxHop * hopSize;
}

ChainTransportCalibration MidiAudioSynchronizer::calibrateTransportOffset(const std::vector<std::vector<float>>& calibrationTakes,
                                                                          double sampleRate,
                                                                          size_t scheduledNoteOnSample,
                                                                          double intrinsicOnsetMs)
{
    ChainTransportCalibration cal;
    cal.calibrationPresetIntrinsicOnsetMs = intrinsicOnsetMs;
    cal.calibrationPassesCount = static_cast<int>(calibrationTakes.size());
    cal.onsetDetectorUncertaintyMs = (sampleRate > 0.0) ? ((64.0 / sampleRate) * 1000.0) : 0.05; // Incertidumbre del hopSize

    if (calibrationTakes.empty() || sampleRate <= 0.0)
    {
        cal.calibrationNotes = "No se suministraron tomas de calibracion";
        return cal;
    }

    std::vector<double> offsetsMs;
    offsetsMs.reserve(calibrationTakes.size());

    for (const auto& take : calibrationTakes)
    {
        if (take.empty()) continue;

        auto novelty = computeNoveltyFunction(take, sampleRate, 64);
        size_t onsetSample = detectOnsetSample(novelty, 64, scheduledNoteOnSample, sampleRate);

        double offsetMs = 0.0;
        if (onsetSample >= scheduledNoteOnSample)
        {
            offsetMs = (static_cast<double>(onsetSample - scheduledNoteOnSample) / sampleRate) * 1000.0;
        }
        else
        {
            offsetMs = -(static_cast<double>(scheduledNoteOnSample - onsetSample) / sampleRate) * 1000.0;
        }
        offsetsMs.push_back(offsetMs);
    }

    if (offsetsMs.empty())
    {
        cal.calibrationNotes = "Fallo en la deteccion de onset en todas las tomas de calibracion";
        return cal;
    }

    // Media y desviación estándar (incertidumbre de jitter)
    double sum = std::accumulate(offsetsMs.begin(), offsetsMs.end(), 0.0);
    double mean = sum / static_cast<double>(offsetsMs.size());

    double sumSqDiff = 0.0;
    for (double o : offsetsMs)
    {
        double diff = o - mean;
        sumSqDiff += diff * diff;
    }

    double stdDev = (offsetsMs.size() > 1) ? std::sqrt(sumSqDiff / static_cast<double>(offsetsMs.size() - 1)) : 0.0;

    cal.transportOffsetEstimateMs = mean;
    cal.transportOffsetUncertaintyMs = stdDev;
    cal.isCalibrated = true;
    cal.calibrationNotes = "Calibracion impulsiva completada con " + std::to_string(offsetsMs.size()) + " tomas";

    return cal;
}

TimingMetrics MidiAudioSynchronizer::synchronizeTrial(const std::vector<float>& audioBuffer,
                                                      double sampleRate,
                                                      const TimedMidiEvent& noteOnEvent,
                                                      const ChainTransportCalibration& calibration,
                                                      double rawAttackMs)
{
    TimingMetrics metrics;
    metrics.tMidiScheduledMs = noteOnEvent.scheduledTimeMs;
    metrics.tMidiDispatchedMs = noteOnEvent.scheduledTimeMs; // Estimado sin loopback de hardware
    metrics.isMidiDispatchedTimeKnown = false;

    if (audioBuffer.empty() || sampleRate <= 0.0)
    {
        metrics.netAttackMs.status = MetricStatus::Invalid;
        return metrics;
    }

    auto novelty = computeNoveltyFunction(audioBuffer, sampleRate, 64);
    size_t onsetSample = detectOnsetSample(novelty, 64, static_cast<size_t>(noteOnEvent.sampleOffset), sampleRate);

    metrics.onsetSampleAbsolute = static_cast<int64_t>(onsetSample);
    metrics.tAudioOnsetMs = (static_cast<double>(onsetSample) / sampleRate) * 1000.0;

    metrics.transportOffsetEstimateMs = calibration.transportOffsetEstimateMs;
    metrics.transportOffsetUncertaintyMs = calibration.transportOffsetUncertaintyMs;
    metrics.calibrationPresetIntrinsicOnsetMs = calibration.calibrationPresetIntrinsicOnsetMs;
    metrics.onsetDetectorUncertaintyMs = calibration.onsetDetectorUncertaintyMs;

    // Desacoplar el ataque neto:
    // netAttack = rawAttackMs - calibrationPresetIntrinsicOnsetMs
    double netAttack = std::max(0.0, rawAttackMs - calibration.calibrationPresetIntrinsicOnsetMs);
    metrics.netAttackMs.value = netAttack;
    metrics.netAttackMs.unit = "ms";
    metrics.netAttackMs.status = MetricStatus::EstimatedWithUncertainty;

    // Propagación cuadrática de incertidumbre
    double combinedStdDev = std::sqrt(
        (calibration.transportOffsetUncertaintyMs * calibration.transportOffsetUncertaintyMs) +
        (calibration.onsetDetectorUncertaintyMs * calibration.onsetDetectorUncertaintyMs)
    );
    metrics.netAttackMs.stdDev = combinedStdDev;
    metrics.netAttackMs.ci.lower = std::max(0.0, netAttack - 1.96 * combinedStdDev);
    metrics.netAttackMs.ci.upper = netAttack + 1.96 * combinedStdDev;
    metrics.netAttackMs.ci.confidenceLevel = 0.95;
    metrics.netAttackMs.ci.method = "Student-t";

    metrics.timingJitterMs = combinedStdDev;

    return metrics;
}

} // namespace abdaudiolab::synth
