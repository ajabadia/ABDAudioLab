#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include "ReferenceSynthGroundTruth.h"

namespace abdaudiolab::fixtures
{

/**
 * @class ReferenceSynthProcessor
 * @brief Sintetizador VST3 de referencia de Nivel 1 para validación física en disco de ABDAudioLab.
 *
 * Implementa una arquitectura analógica virtual canónica:
 * - Oscilador monofónico/polifónico con respuesta MIDI.
 * - Filtro TPT Lowpass de 2 polos con resonancia.
 * - Envolvente de amplitud ADSR.
 * - Parámetros de infraestructura de test explícitamente etiquetados.
 * - Estado binario 100% serializable y reproducible.
 */
class ReferenceSynthProcessor : public juce::AudioProcessor
{
public:
    ReferenceSynthProcessor();
    ~ReferenceSynthProcessor() override = default;

    // Métodos obligatorios de juce::AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    /**
     * @brief Canal exclusivo de verificación de caja blanca para tests unitarios.
     * Invisible para el flujo de introspección VST3 estándar de ABDAudioLab.
     */
    [[nodiscard]] ReferenceSynthGroundTruth getPrivateGroundTruthForTesting() const noexcept;

private:
    // Parámetros musicales (AudioControl)
    juce::AudioParameterFloat* cutoffParam_ { nullptr };
    juce::AudioParameterFloat* resonanceParam_ { nullptr };
    juce::AudioParameterFloat* attackParam_ { nullptr };
    juce::AudioParameterFloat* gainParam_ { nullptr };
    juce::AudioParameterFloat* pitchBendParam_ { nullptr };

    // Parámetros de infraestructura de pruebas (TestInfrastructure)
    juce::AudioParameterChoice* controlModeParam_ { nullptr };
    juce::AudioParameterFloat* smoothingTimeParam_ { nullptr };
    juce::AudioParameterInt* latencyParam_ { nullptr };

    // Estado interno del motor DSP
    double sampleRate_ { 96000.0 };
    double currentPhase_ { 0.0 };
    double phaseIncrement_ { 0.0 };
    bool isNoteActive_ { false };
    int activeMidiNote_ { 60 };
    float activeVelocity_ { 0.0f };

    // Envolvente ADSR simple
    double envLevel_ { 0.0 };

    // Filtro TPT pasobajo de 2 polos
    double s1_ { 0.0 };
    double s2_ { 0.0 };

    // Filtro unipolar de suavizado para cutoff
    double smoothedCutoff_ { 1.0 };

    // Estadísticas internas para el oráculo de ground truth
    ReferenceSynthGroundTruth groundTruthState_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferenceSynthProcessor)
};

} // namespace abdaudiolab::fixtures
