/**
 * @file AnalogDutCharacterizer.h
 * @brief Unified metrological engine for Analog DUT Characterization:
 *        Farina frequency response adapter, parametric THD (IEEE/IEC), two-tone IMD,
 *        and level sweep / saturation threshold analysis.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "AnalogDutCharacterizationContracts.h"
#include "FineLatencyContracts.h"
#include "MeasurementDspUtils.h"
#include "math/FarinaDeconvolver.h"
#include "math/IntermodulationAnalyzer.h"
#include <span>
#include <vector>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class AnalogDutCharacterizer
 * @brief Orquestador metrológico de caracterización analógica.
 * Reutiliza estrictamente FineLatencyAnalyzer, MeasurementDspUtils,
 * math::FarinaDeconvolver y math::IntermodulationAnalyzer.
 */
class AnalogDutCharacterizer
{
public:
    /**
     * @brief Adapts math::FarinaDeconvolver result into canonical FrequencyResponseResult.
     */
    [[nodiscard]] static FrequencyResponseResult adaptFarinaResult(
        const math::DeconvolutionResult& decoResult,
        double sampleRate,
        double sweepDurationSec,
        float startFreqHz,
        float endFreqHz);

    /**
     * @brief Measures linear frequency response and harmonic impulse fractions using Farina swept-sine.
     */
    [[nodiscard]] static FrequencyResponseResult measureFrequencyResponse(
        std::span<const float> responseAudio,
        std::span<const float> inverseFilter,
        double sampleRate,
        double sweepDurationSec,
        float startFreqHz = 20.0f,
        float endFreqHz = 20000.0f);

    /**
     * @brief Performs unified parametric THD measurement resolving IEEE vs IEC conventions.
     * @param signal Input audio signal (aligned via FineLatencyAnalyzer / LatencyCompensationView).
     * @param sampleRate Sampling rate in Hz.
     * @param fundamentalHz Nominal fundamental frequency in Hz.
     * @param convention Denominator convention (FundamentalReferenced vs TotalRmsReferenced).
     * @param firstHarmonic First harmonic index (default: 2).
     * @param lastHarmonic Last harmonic index (default: 10).
     * @param window Windowing function (default: Hann).
     * @param fftSize FFT resolution size (default: 4096).
     * @return HarmonicDistortionResult with complete harmonic breakdown.
     */
    [[nodiscard]] static HarmonicDistortionResult measureHarmonicDistortion(
        std::span<const float> signal,
        double sampleRate,
        double fundamentalHz = 1000.0,
        ThdConvention convention = ThdConvention::FundamentalReferenced,
        int firstHarmonic = 2,
        int lastHarmonic = 10,
        MeasurementWindow window = MeasurementWindow::Hann,
        int fftSize = 4096);

    /**
     * @brief Analyzes two-tone Intermodulation Distortion according to declared convention.
     * @param signal Recorded response signal.
     * @param sampleRate Sampling rate in Hz.
     * @param convention ImdConvention (Smpte, Ccif, Din, ItuR).
     * @param f1Hz Lower tone frequency in Hz.
     * @param f2Hz Higher tone frequency in Hz.
     * @param fftSize FFT resolution size (default: 8192).
     * @return IntermodulationResult with discrete 2nd and 3rd order products.
     */
    [[nodiscard]] static IntermodulationResult measureIntermodulation(
        std::span<const float> signal,
        double sampleRate,
        ImdConvention convention,
        double f1Hz,
        double f2Hz,
        int fftSize = 8192);

    /**
     * @brief Analyzes a level sweep sequence to extract 1% THD, 3% THD, and P1dB compression thresholds.
     * @param sweepPoints Measured or simulated input-output sweep points.
     * @return ClippingThresholdResult with discrete threshold levels.
     */
    [[nodiscard]] static ClippingThresholdResult analyzeLevelSweep(
        const std::vector<LevelSweepPoint>& sweepPoints);

    /**
     * @brief Helper to detect hard clipping in the time-domain signal.
     */
    [[nodiscard]] static bool detectHardClipping(
        std::span<const float> signal,
        float threshold = 0.999f) noexcept;
};

} // namespace abdaudiolab::measurement
