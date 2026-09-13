#pragma once

#include "ISynthTarget.h"
#include "ExternalPluginTypes.h"
#include "TargetContract.h"
#include "TargetContractDiscovery.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>
#include <vector>

namespace abdaudiolab::synth
{

/**
 * @class ExternalPluginFixture
 * @brief Adaptador ISynthTarget que carga y aloja un binario VST3 real desde disco.
 *
 * Garantiza:
 * 1. Carga física real mediante juce::AudioPluginFormatManager (InProcessExternalBinary).
 * 2. Manifiesto criptográfico inmutable del bundle/archivo (.vst3) con hashes SHA-256.
 * 3. Ejecución determinista de render con inyección de eventos MIDI y automatización de parámetros.
 * 4. Aislamiento de estado y ciclo de vida de instancia independiente.
 */
class ExternalPluginFixture : public ISynthTarget
{
public:
    explicit ExternalPluginFixture(juce::AudioPluginFormatManager& formatManager);
    ~ExternalPluginFixture() override;

    /**
     * @brief Carga e inicializa el plugin VST3 desde la ruta física en disco.
     * @param pluginFile Archivo o bundle .vst3.
     * @param sampleRate Frecuencia de muestreo.
     * @param blockSize Tamaño de bloque de audio.
     * @param errorMessage Cadena donde se reportan errores de carga o formato.
     * @return true si la carga e instanciación fue exitosa.
     */
    bool loadPluginFromDisk(const juce::File& pluginFile,
                            double sampleRate,
                            int blockSize,
                            std::string& errorMessage);

    // --- Implementación de ISynthTarget ---
    bool loadState(const SynthPresetState& state) override;
    [[nodiscard]] StateAppliedStatus verifyState() const override;
    void prepare(const ProcessingSpec& spec) override;
    void resetState() override;

    void render(const MidiExcitationSequence& sequence,
                std::vector<float>& destinationAudio,
                int repetitionIndex = 0) override;

    [[nodiscard]] TargetTimingInfo timingInfo() const override;
    [[nodiscard]] bool supportsBinaryState() const override;
    StateTransferResult getState(std::vector<uint8_t>& stateData) const override;
    StateTransferResult setState(const std::vector<uint8_t>& stateData) override;

    // --- Metadatos e Introspección ---
    [[nodiscard]] const PluginIdentity& getIdentity() const noexcept { return identity_; }
    [[nodiscard]] juce::AudioPluginInstance* getPluginInstance() noexcept { return instance_.get(); }
    [[nodiscard]] TargetContract discoverContract() const;

private:
    void computeBundleManifest(const juce::File& pluginFile);

    juce::AudioPluginFormatManager& formatManager_;
    std::unique_ptr<juce::AudioPluginInstance> instance_;
    PluginIdentity identity_;
    ProcessingSpec spec_ { 96000.0, 512, 2 };

    bool isStateVerified_ { false };
    double lastAppliedTimestampMs_ { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExternalPluginFixture)
};

} // namespace abdaudiolab::synth
