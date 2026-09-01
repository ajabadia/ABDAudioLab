#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace abdaudiolab::math
{

struct DeconvolutionResult
{
    std::vector<float> fullDeconvolvedIR;
    std::vector<float> linearIR;
    std::vector<float> frequencyResponseMagnitudeDb;
    std::vector<float> frequenciesHz;
    float peakFrequencyHz { 0.0f };
    float resonancePeakDb { 0.0f };
    float thdPercent { 0.0f };
};

/**
 * @brief Angelo Farina (2000) Logarithmic Swept-Sine deconvolution and harmonic separation.
 */
class FarinaDeconvolver
{
public:
    FarinaDeconvolver();
    ~FarinaDeconvolver() = default;

    /**
     * @brief Generates the inverse filter f(t) for an exponential sine sweep.
     */
    static std::vector<float> generateInverseFilter(double sampleRate, double durationSec, float startFreqHz, float endFreqHz);

    /**
     * @brief Deconvolves recorded response with the inverse filter.
     */
    static DeconvolutionResult deconvolve(const std::vector<float>& recordedResponse,
                                         const std::vector<float>& inverseFilter,
                                         double sampleRate,
                                         double sweepDurationSec,
                                         float startFreqHz,
                                         float endFreqHz);

private:
    static void computeFrequencyResponse(const std::vector<float>& impulseResponse,
                                         double sampleRate,
                                         std::vector<float>& outFreqs,
                                         std::vector<float>& outMagsDb,
                                         float& outPeakFreq,
                                         float& outPeakDb);
};

} // namespace abdaudiolab::math
