/**
 * @file DynamicsMeasurementAdapter.h
 * @brief Adapter coordinating MIDI dynamics response measurements into canonical response-measurement-1.0 contracts.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../MeasurementContracts.h"
#include "../MeasurementCaptureCoordinator.h"
#include "../../synth/ISynthTarget.h"
#include <vector>
#include <string>

namespace abdaudiolab::measurement
{

/**
 * @class DynamicsMeasurementAdapter
 * @brief Coordinates multi-velocity MIDI excitation series (Campaña 20.10.3-D).
 * 
 * Strict metrological principles:
 * - Executes sequential closed-loop cycle per velocity point:
 *     restore preset -> NoteOn(v) -> capture -> analyze -> NoteOff
 * - Velocity v=0 handled as explicit special case (skipped/silent, midi_note_on_velocity_zero).
 * - Distinguishes observed response from unobservable or silent signal (status: unreliable, no invented zeros).
 * - Evaluates empirical curve fitting models (linear, logarithmic, exponential, piecewise) without assuming
 *   that R^2 measures quality or linearity.
 * - Detects step/jump discontinuities as "discontinuity observed", strictly prohibiting false assertions of
 *   "layer switching confirmed" without independent multi-sample architectural evidence.
 * - Stores per-point and aggregated reproducibility metadata (preset hash, stimulus hash, sample rate,
 *   exact analysis window, spectral FFT metadata).
 */
class DynamicsMeasurementAdapter
{
public:
    static constexpr const char* kAnalyzerName = "DynamicsMeasurementAdapter";
    static constexpr const char* kAnalyzerVersion = "1.0.0";

    /**
     * @brief Structure holding a pre-captured velocity take for decoupled analysis and unit testing.
     */
    struct VelocityTake
    {
        int velocity { 0 };
        std::vector<float> audio;
        double sampleRateHz { 48000.0 };
        size_t noteOnSample { 0 };
        size_t noteOffSample { 0 };
        std::string presetStateHash;
        std::string audioArtifactHash;
    };

    /**
     * @brief Standard recommended discrete velocity grid: [0, 1, 8, 16, 24, 32, 48, 64, 80, 96, 112, 120, 127].
     */
    static std::vector<int> getDefaultVelocityGrid();

    /**
     * @brief Executes full live MIDI dynamic measurement campaign against an ISynthTarget.
     * 
     * @param target Pointer to initialized ISynthTarget (plugin, worker, or reference synth).
     * @param spec Measurement specification defining stimulus parameters, note, duration, and velocity grid.
     * @param state Optional preset state to restore before every note excitation.
     * @return MeasurementResult Fully populated canonical measurement result.
     */
    static MeasurementResult measure(synth::ISynthTarget* target,
                                     const MeasurementSpec& spec,
                                     const synth::SynthPresetState* state = nullptr);

    /**
     * @brief Analyzes pre-captured velocity takes (decoupled from live target execution).
     * 
     * @param spec Measurement specification.
     * @param takes Vector of pre-captured audio buffers across different velocities.
     * @return MeasurementResult Fully populated canonical measurement result.
     */
    static MeasurementResult analyzeTakes(const MeasurementSpec& spec,
                                          const std::vector<VelocityTake>& takes);

    /**
     * @brief Analyzes a single velocity audio buffer to compute DynamicPoint metrics.
     * 
     * @param velocity Discrete MIDI velocity (0..127).
     * @param audio Audio buffer.
     * @param sampleRate Operating sample rate in Hz.
     * @param noteOnSample Note-On sample offset.
     * @param noteOffSample Note-Off sample offset.
     * @param windowStartMs Declared measurement window start (ms). If <= 0, auto-computed.
     * @param windowEndMs Declared measurement window end (ms). If <= 0, auto-computed.
     * @param presetStateHash State hash restored before excitation.
     * @param audioArtifactHash SHA-256 of the audio buffer.
     * @return DynamicPoint Strongly typed dynamic point observation.
     */
    static DynamicPoint analyzeSinglePoint(int velocity,
                                           const std::vector<float>& audio,
                                           double sampleRate,
                                           size_t noteOnSample,
                                           size_t noteOffSample,
                                           double windowStartMs = 0.0,
                                           double windowEndMs = 0.0,
                                           const std::string& presetStateHash = "",
                                           const std::string& audioArtifactHash = "");

    /**
     * @brief Evaluates curve regression models and computes R^2 for declared models.
     * 
     * @param x Vector of x values (velocities).
     * @param y Vector of y values (amplitudes in dBFS or centroids in Hz).
     * @param xVar Name of x variable (e.g. "velocity").
     * @param yVar Name of y variable (e.g. "rmsDbfs" or "spectralCentroidHz").
     * @return CurveFitMetadata Declared best-fit model with goodness of fit R^2.
     */
    static CurveFitMetadata fitCurveModel(const std::vector<double>& x,
                                          const std::vector<double>& y,
                                          const std::string& xVar,
                                          const std::string& yVar);

    /**
     * @brief Detects empirical jump/step discontinuities in an observed series.
     * 
     * @param velocities Velocity coordinates.
     * @param values Observed values (e.g. RMS dBFS).
     * @param jumpThresholdDb Minimum step delta to register a discontinuity (default 6.0 dB).
     * @return DiscontinuityObservation Empirical discontinuity observation record.
     */
    static DiscontinuityObservation detectDiscontinuity(const std::vector<int>& velocities,
                                                        const std::vector<double>& values,
                                                        double jumpThresholdDb = 6.0);
};

} // namespace abdaudiolab::measurement
