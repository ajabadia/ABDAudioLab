/**
 * @file ModulationMeasurementAdapter.h
 * @brief Adapter coordinating cyclic modulation (LFO, vibrato, tremolo) into canonical response-measurement-1.0 contracts.
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
 * @class ModulationMeasurementAdapter
 * @brief Coordinates cyclic modulation measurements (Campaña 20.10.3-M).
 * 
 * Strict metrological principles:
 * - Demodulates temporal trajectories according to destination (pitch in cents, amplitude in dB, filter cutoff in Hz).
 * - Declares analytical method used for LFO rate estimation:
 *     "temporal_period" | "spectral_peak" | "pitch_tracking" | "amplitude_demodulation"
 * - Associates spectral sidebands with an explicitly observed carrier:
 *     carrierFrequencyHz, sidebandFrequencyHz, order, levelRelativeToCarrierDb
 * - Classifies waveform estimate distinguishing observed from inferred:
 *     status: "observed" | "inferred" | "not_observable", with confidence factor [0..1]
 * - Emits demodulated 2D curves (timeCurve in ms vs destination units, spectrumCurve in Hz vs dBFS).
 * - Persists FFT resolution and window metadata via SpectralAnalysisMetadata.
 * - Handles unmodulated or silent signals as status: unreliable, without inventing zeros.
 */
class ModulationMeasurementAdapter
{
public:
    static constexpr const char* kAnalyzerName = "ModulationMeasurementAdapter";
    static constexpr const char* kAnalyzerVersion = "1.0.0";

    /**
     * @brief Measures modulation response on a live ISynthTarget.
     * 
     * @param target Pointer to initialized ISynthTarget.
     * @param spec Measurement specification declaring destination, stimulus parameters and carrier note.
     * @param state Optional preset state to restore before capture.
     * @return MeasurementResult Fully populated canonical measurement result.
     */
    static MeasurementResult measure(synth::ISynthTarget* target,
                                     const MeasurementSpec& spec,
                                     const synth::SynthPresetState* state = nullptr);

    /**
     * @brief Analyzes a pre-captured audio buffer (decoupled from live target).
     * 
     * @param spec Measurement specification declaring destination and expected nominal parameters.
     * @param audio Recorded audio buffer.
     * @param sampleRate Operating sample rate in Hz.
     * @param audioArtifactPath Optional container relative path.
     * @param audioSha256 Optional audio hash.
     * @return MeasurementResult Fully populated canonical measurement result.
     */
    static MeasurementResult analyzeBuffer(const MeasurementSpec& spec,
                                           const std::vector<float>& audio,
                                           double sampleRate,
                                           const std::string& audioArtifactPath = "",
                                           const std::string& audioSha256 = "");

    /**
     * @brief Demodulates amplitude envelope from audio buffer (for tremolo / AM).
     * 
     * @param audio Audio buffer.
     * @param sampleRate Sample rate in Hz.
     * @param hopSamples Hop size for downsampled envelope.
     * @param outTimesMs Output time coordinates in milliseconds.
     * @param outEnvelopeDb Output envelope values in dBFS.
     */
    static void demodulateAmplitude(const std::vector<float>& audio,
                                    double sampleRate,
                                    size_t hopSamples,
                                    std::vector<double>& outTimesMs,
                                    std::vector<double>& outEnvelopeDb);

    /**
     * @brief Demodulates pitch trajectory from audio buffer (for vibrato / FM).
     * 
     * @param audio Audio buffer.
     * @param sampleRate Sample rate in Hz.
     * @param nominalCarrierHz Nominal carrier frequency in Hz.
     * @param hopSamples Hop size between pitch evaluation windows.
     * @param outTimesMs Output time coordinates in milliseconds.
     * @param outPitchDeltaCents Output pitch deviation in cents relative to carrier.
     */
    static void demodulatePitch(const std::vector<float>& audio,
                                double sampleRate,
                                double nominalCarrierHz,
                                size_t hopSamples,
                                std::vector<double>& outTimesMs,
                                std::vector<double>& outPitchDeltaCents);

    /**
     * @brief Estimates LFO rate (Hz) and rate estimation method from a demodulated curve.
     * 
     * @param timesMs Time points in ms.
     * @param values Demodulated curve values.
     * @param sampleRate Demodulated curve effective sample rate in Hz.
     * @param preferredMethod Method preference ("spectral_peak", "temporal_period", etc.).
     * @param outMethod Actual method used.
     * @return double Estimated LFO rate in Hz.
     */
    static double estimateLfoRate(const std::vector<double>& timesMs,
                                  const std::vector<double>& values,
                                  double sampleRate,
                                  const std::string& preferredMethod,
                                  std::string& outMethod);

    /**
     * @brief Identifies spectral sidebands around an observed carrier.
     * 
     * @param audio Audio buffer in steady-state.
     * @param sampleRate Sample rate in Hz.
     * @param carrierFreqHz Observed carrier frequency in Hz.
     * @param lfoRateHz Estimated LFO rate in Hz.
     * @param maxOrder Maximum sideband harmonic order (+/- 1, +/- 2, etc.).
     * @return std::vector<ModulationSideband> Identified sideband components.
     */
    static std::vector<ModulationSideband> extractSidebands(const std::vector<float>& audio,
                                                            double sampleRate,
                                                            double carrierFreqHz,
                                                            double lfoRateHz,
                                                            int maxOrder = 2);

    /**
     * @brief Classifies waveform shape by correlating against canonical geometric forms.
     * 
     * @param values One or more full cycles of normalized demodulated curve.
     * @return WaveformEstimate Inferred or observed waveform with confidence score.
     */
    static WaveformEstimate classifyWaveform(const std::vector<double>& values);
};

} // namespace abdaudiolab::measurement
