/**
 * @file NamDatasetExporter.h
 * @brief Exporter for Neural Amp Modeler (NAM) and RTNeural training datasets.
 * @details Aligns excitation stimulus and recorded hardware response with sample-level
 *          accuracy using cross-correlation, and exports (input.wav, target.wav, manifest.json).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <string>
#include <map>
#include <vector>

namespace abdaudiolab::exporting
{

/**
 * @struct NamDatasetManifest
 * @brief Metadata accompanying a NAM training calibration dataset.
 */
struct NamDatasetManifest
{
    std::string hardwareId;
    std::string hardwareDisplayName;
    std::string functionId;
    std::string captureMode { "NAM_NEURAL_CALIBRATION" };
    std::string timestampUtc;

    double sampleRate { 96000.0 };
    int bitDepth { 24 };
    int totalSamples { 0 };
    int latencyOffsetSamples { 0 };

    float inputPeakDb { 0.0f };
    float targetPeakDb { 0.0f };
    float estimatedThdPercent { 0.0f };

    std::map<std::string, float> controlPositions; // Param name -> normalized [0.0, 1.0]
};

/**
 * @class NamDatasetExporter
 * @brief Handles latency alignment and WAV/JSON serialization of neural calibration pairs.
 */
class NamDatasetExporter
{
public:
    NamDatasetExporter() = default;
    ~NamDatasetExporter() = default;

    /**
     * @brief Finds time-alignment lag (in samples) between input and response using cross-correlation on the sync pulses.
     * @param input Signal containing sync markers.
     * @param recorded Signal captured from hardware.
     * @param numSamplesToSearch Window of samples to analyze (typically pre-roll, ~0.4s).
     * @param maxLagSamples Maximum expected latency lag in samples (e.g. 50ms = 4800 samples at 96kHz).
     * @return Lag in samples (positive means recorded is delayed relative to input).
     */
    static int findLatencyOffsetSamples(const float* input,
                                        const float* recorded,
                                        int numSamplesToSearch,
                                        int maxLagSamples);

    /**
     * @brief Aligns input and recorded buffers, and writes input.wav, target.wav, and nam_dataset_manifest.json.
     * @param targetDirectory Folder to write the dataset files into.
     * @param inputBuffer Excitation audio buffer (mono).
     * @param recordedBuffer Raw recorded audio buffer from hardware (mono).
     * @param sampleRate Sampling rate.
     * @param manifest Dataset metadata to serialize.
     * @return True if all files were written successfully.
     */
    static bool exportDataset(const juce::File& targetDirectory,
                              const juce::AudioBuffer<float>& inputBuffer,
                              const juce::AudioBuffer<float>& recordedBuffer,
                              double sampleRate,
                              NamDatasetManifest manifest);
};

} // namespace abdaudiolab::exporting
