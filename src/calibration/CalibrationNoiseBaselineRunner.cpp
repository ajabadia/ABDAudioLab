/**
 * @file CalibrationNoiseBaselineRunner.cpp
 * @brief Implementation of diagnostic physical noise baseline runner.
 * @author ABDSynths
 * @date 2026
 */

#include "CalibrationNoiseBaselineRunner.h"
#include "../math/NoiseFloorTracker.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::calibration
{

CalibrationNoiseBaseline CalibrationNoiseBaselineRunner::analyze(
    const float* buffer,
    int numSamples,
    double sampleRate,
    bool outputMuted,
    float contaminationThresholdDbfs,
    float floorThresholdDbfs)
{
    CalibrationNoiseBaseline base;
    base.outputMuted = outputMuted;

    if (buffer == nullptr || numSamples <= 0 || sampleRate <= 0.0)
    {
        base.status = NoiseBaselineStatus::DeviceStopped;
        return base;
    }

    base.durationSamples = numSamples;
    base.durationMs = (static_cast<double>(numSamples) * 1000.0) / sampleRate;

    // Check for clipping and compute peak / rms
    double sumSq = 0.0;
    float peakVal = 0.0f;
    bool clipped = false;

    for (int i = 0; i < numSamples; ++i)
    {
        float absVal = std::abs(buffer[i]);
        if (absVal >= 0.999f)
            clipped = true;
        if (absVal > peakVal)
            peakVal = absVal;
        sumSq += static_cast<double>(absVal * absVal);
    }

    base.inputWasClipped = clipped;
    if (clipped)
    {
        base.status = NoiseBaselineStatus::Clipped;
        base.peakDbfs = (peakVal > 1e-6f) ? (20.0f * std::log10(peakVal)) : 0.0f;
        base.rmsDbfs = static_cast<float>(20.0 * std::log10(std::max(1e-6, std::sqrt(sumSq / numSamples))));
        return base;
    }

    float rmsLinear = static_cast<float>(std::sqrt(sumSq / static_cast<double>(numSamples)));
    float peakDb = (peakVal > 1e-6f) ? (20.0f * std::log10(peakVal)) : -120.0f;
    float rmsDb = (rmsLinear > 1e-6f) ? (20.0f * std::log10(rmsLinear)) : -120.0f;

    base.peakDbfs = peakDb;
    base.rmsDbfs = rmsDb;

    // Check for digital zero or below floor threshold
    if (peakVal <= 0.0f || rmsDb <= floorThresholdDbfs)
    {
        base.status = NoiseBaselineStatus::BelowMeasurementFloor;
        base.hasSpectralBands = false;
        return base;
    }

    // Check for contamination
    if (rmsDb > contaminationThresholdDbfs)
    {
        base.status = NoiseBaselineStatus::Contaminated;
        return base;
    }

    base.status = NoiseBaselineStatus::Valid;

    // Compute 32-band spectral fingerprint using NoiseFloorTracker
    math::NoiseFloorTracker tracker;
    tracker.recordNoiseSnapshot(0.0, buffer, numSamples, sampleRate);
    const auto* snap = tracker.getLatestSnapshot();
    if (snap != nullptr)
    {
        base.hasSpectralBands = true;
        base.spectralBandDbfs = snap->spectralBands;
    }

    return base;
}

} // namespace abdaudiolab::calibration
