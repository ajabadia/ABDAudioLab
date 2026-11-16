/**
 * @file CalibrationNoiseBaselineRunner.h
 * @brief Diagnostic runner measuring physical noise floor baseline prior to loopback sweep.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "CalibrationSnapshot.h"

namespace abdaudiolab::calibration
{

class CalibrationNoiseBaselineRunner
{
public:
    /**
     * @brief Analyzes a buffer of raw ADC silence captured while stimulus output is muted.
     * 
     * @param buffer Raw ADC input audio samples (pre-trim).
     * @param numSamples Number of captured samples (e.g. 300ms - 500ms).
     * @param sampleRate Sampling rate in Hz.
     * @param outputMuted Whether stimulus generator was confirmed muted during baseline capture.
     * @param contaminationThresholdDbfs Maximum acceptable noise floor before flagging as contaminated.
     * @param floorThresholdDbfs Threshold below which noise is indistinguishable from digital zero.
     */
    static CalibrationNoiseBaseline analyze(
        const float* buffer,
        int numSamples,
        double sampleRate,
        bool outputMuted = true,
        float contaminationThresholdDbfs = -45.0f,
        float floorThresholdDbfs = -115.0f);
};

} // namespace abdaudiolab::calibration
