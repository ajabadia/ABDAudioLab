#pragma once

#include "ISynthTargetLifecycleAdapter.h"
#include "SyntheticSynthFixture.h"
#include "SyntheticSynthTarget.h"
#include "ExternalPluginFixture.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>

namespace abdaudiolab::synth
{

/**
 * @brief Adaptador de ciclo de vida para targets sintéticos deterministas (Demo / Fixture).
 */
class SyntheticTargetLifecycleAdapter : public ISynthTargetLifecycleAdapter
{
public:
    SyntheticTargetLifecycleAdapter(double defaultSampleRate = 48000.0, uint32_t seed = 42);
    ~SyntheticTargetLifecycleAdapter() override = default;

    bool initializeTarget(const gui::session::TargetSelectionState& targetState,
                          double sampleRate,
                          int blockSize,
                          std::string& outErrorMessage) override;

    [[nodiscard]] ISynthTarget* getTarget() noexcept override;
    [[nodiscard]] const ISynthTarget* getTarget() const noexcept override;

    void resetForTrial() override;
    void releaseTarget() override;

    [[nodiscard]] bool isReady() const noexcept override;
    [[nodiscard]] EvaluationOrigin getEvaluationOrigin() const noexcept override;
    [[nodiscard]] std::string getExecutionMode() const override;
    [[nodiscard]] TargetFingerprint getFingerprint() const override;
    [[nodiscard]] bool requiresUnblockedMessageThread() const noexcept override { return false; }

    [[nodiscard]] SyntheticSynthFixture& getFixture() noexcept { return fixture_; }

private:
    SyntheticSynthFixture fixture_;
    SyntheticSynthTarget synthTarget_;
    TargetFingerprint fingerprint_;
    bool isReady_ { false };
    double sampleRate_ { 48000.0 };
    int blockSize_ { 256 };
};

/**
 * @brief Adaptador de ciclo de vida para VST3 cargado en el proceso host (In-process VST3 test target).
 *
 * ADVERTENCIA DE ARQUITECTURA:
 * Un plugin cargado in-process no está aislado del host: fallos de memoria, excepciones
 * o cuelgues del plugin pueden corromper o congelar el proceso de ABDAudioLab.
 * Esta clase implementa las salvaguardas de nivel de hilo (watchdog de bloque,
 * validación de cancelación, captura de excepciones y aislamiento de estado).
 * La interfaz ISynthTargetLifecycleAdapter está preparada para un futuro
 * OutOfProcessVst3LifecycleAdapter (ABDAudioLab <-> IPC / shared memory <-> VST3 worker process).
 */
class InProcessVst3LifecycleAdapter : public ISynthTargetLifecycleAdapter
{
public:
    explicit InProcessVst3LifecycleAdapter(double watchdogMaxBlockMs = 500.0);
    ~InProcessVst3LifecycleAdapter() override;

    bool initializeTarget(const gui::session::TargetSelectionState& targetState,
                          double sampleRate,
                          int blockSize,
                          std::string& outErrorMessage) override;

    [[nodiscard]] ISynthTarget* getTarget() noexcept override;
    [[nodiscard]] const ISynthTarget* getTarget() const noexcept override;

    void resetForTrial() override;
    void releaseTarget() override;

    [[nodiscard]] bool isReady() const noexcept override;
    [[nodiscard]] EvaluationOrigin getEvaluationOrigin() const noexcept override;
    [[nodiscard]] std::string getExecutionMode() const override;
    [[nodiscard]] TargetFingerprint getFingerprint() const override;
    [[nodiscard]] bool requiresUnblockedMessageThread() const noexcept override;

    void setWatchdogMaxBlockDurationMs(double maxMs) noexcept;
    [[nodiscard]] double getWatchdogMaxBlockDurationMs() const noexcept { return watchdogMaxBlockMs_; }
    [[nodiscard]] bool isCorrupted() const noexcept { return isCorrupted_; }
    void markCorrupted(const std::string& reason) noexcept;

    /**
     * @brief Resuelve la ubicación del binario VST3 para un target dado.
     * Permite localización controlada por entorno o rutas estándar de CI.
     */
    static juce::File resolveVst3File(const gui::session::TargetSelectionState& targetState);

private:
    void completePreparationSequence(double sampleRate, int blockSize);

    juce::AudioPluginFormatManager formatManager_;
    std::unique_ptr<ExternalPluginFixture> fixture_;
    TargetFingerprint fingerprint_;

    double sampleRate_ { 96000.0 };
    int blockSize_ { 256 };
    double watchdogMaxBlockMs_ { 500.0 };

    bool isReady_ { false };
    bool isCorrupted_ { false };
    std::string corruptionReason_;
    std::thread::id creationThreadId_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InProcessVst3LifecycleAdapter)
};

} // namespace abdaudiolab::synth
