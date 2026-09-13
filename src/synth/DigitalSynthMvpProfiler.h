#pragma once

#include <vector>
#include <functional>
#include <memory>
#include "SynthPresetState.h"
#include "MidiExcitationSequence.h"
#include "SynthObservation.h"
#include "MidiAudioSynchronizer.h"
#include "SynthPitchEstimator.h"
#include "SynthEnvelopeAnalyzer.h"
#include "SyntheticSynthFixture.h"
#include "ISynthTarget.h"

namespace abdaudiolab::synth
{

/**
 * @brief Orquestador del Vertical Slice 1 del experimento factorial de perfilado de sintetizadores.
 * 
 * Desacoplado de la procedencia del sonido: ejecuta el protocolo contra cualquier ISynthTarget
 * (SyntheticSynthTarget, PluginSynthTarget o HardwareSynthTarget).
 */
class DigitalSynthMvpProfiler
{
public:
    explicit DigitalSynthMvpProfiler(double sampleRate = 96000.0);
    ~DigitalSynthMvpProfiler() = default;

    using TrialAudioRenderer = std::function<std::vector<float>(const MidiExcitationSequence& sequence, int repetitionIndex)>;

    /**
     * @brief Ejecuta el protocolo factorial completo contra un ISynthTarget polimórfico.
     */
    SynthProfileReport runTargetSession(const SynthPresetState& preset,
                                       ISynthTarget& target,
                                       int noteNumber = 60,
                                       int numPassesPerCondition = 3);

    /**
     * @brief Ejecuta la sesión directamente contra una instancia de SyntheticSynthFixture.
     */
    SynthProfileReport runFixtureSession(const SynthPresetState& preset,
                                        SyntheticSynthFixture& fixture,
                                        int noteNumber = 60,
                                        int numPassesPerCondition = 3);

    /**
     * @brief Ejecuta la sesión usando un proveedor de audio genérico en lambda (retrocompatibilidad).
     */
    SynthProfileReport runMvpSession(const SynthPresetState& preset,
                                    const TrialAudioRenderer& audioSource,
                                    int noteNumber = 60,
                                    int numPassesPerCondition = 3);

private:
    double sampleRate_ { 96000.0 };

    std::vector<int> velocityGrid_ { 40, 64, 110 };
    std::vector<double> durationGrid_ { 0.05, 0.25, 2.00 }; // 50ms, 250ms, 2.0s
};

} // namespace abdaudiolab::synth
