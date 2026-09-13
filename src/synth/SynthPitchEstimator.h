#pragma once

#include <vector>
#include <cstdint>
#include "SynthObservation.h"

namespace abdaudiolab::synth
{

/**
 * @brief Estimador desacoplado de frecuencia fundamental f0 y error en cents.
 */
class SynthPitchEstimator
{
public:
    SynthPitchEstimator() = default;
    ~SynthPitchEstimator() = default;

    /**
     * @brief Estima el tono de una señal monofónica en una región estacionaria conocida.
     */
    static PitchEstimate estimatePitch(const std::vector<float>& audioBuffer,
                                       double sampleRate,
                                       double nominalFrequencyHz,
                                       size_t searchStartSample,
                                       size_t searchWindowSamples);

    [[nodiscard]] static double midiNoteToFrequencyHz(int midiNoteNumber) noexcept;
};

} // namespace abdaudiolab::synth
