#pragma once

#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../../audio/LabAudioEngine.h"
#include "../../synth/Sha256.h"

namespace abdaudiolab::test::support
{

/**
 * @struct RecordedMidiEvent
 * @brief Registro determinista de un evento MIDI procesado durante la ejecución.
 */
struct RecordedMidiEvent
{
    int64_t sampleOffset { 0 };
    int channel { 1 };
    int noteNumber { 0 };
    float velocity { 0.0f };
    bool isNoteOn { false };
    bool isNoteOff { false };
    bool isAllNotesOff { false };

    [[nodiscard]] bool matches(const RecordedMidiEvent& other) const noexcept
    {
        return channel == other.channel &&
               noteNumber == other.noteNumber &&
               std::abs(velocity - other.velocity) < 1e-5f &&
               isNoteOn == other.isNoteOn &&
               isNoteOff == other.isNoteOff &&
               isAllNotesOff == other.isAllNotesOff;
    }
};

/**
 * @class SyntheticAudioFixture
 * @brief Plugin sintético determinista in-memory para tests headless.
 *
 * Implementa juce::AudioPluginInstance para ser alojado directamente en LabAudioEngine.
 * Genera una onda senoidal pura y matemáticamente perfecta en respuesta a NoteOn,
 * y se silencia con NoteOff. Libre de ruido térmico, jitter o deriva analógica.
 */
class SyntheticAudioFixture : public juce::AudioPluginInstance
{
public:
    SyntheticAudioFixture()
    {
        desc_.name = "SyntheticAudioFixture";
        desc_.pluginFormatName = "InternalTestFixture";
        desc_.category = "Synth";
        desc_.isInstrument = true;
        desc_.fileOrIdentifier = "internal://SyntheticAudioFixture";
    }

    ~SyntheticAudioFixture() override = default;

    void prepareToPlay(double newSampleRate, int /*samplesPerBlock*/) override
    {
        sampleRate_ = (newSampleRate > 1000.0) ? newSampleRate : 48000.0;
        resetFixture();
    }

    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;

        // 1. Procesar eventos MIDI entrantes con sample offset exacto
        for (const auto meta : midiMessages)
        {
            auto msg = meta.getMessage();
            int samplePos = meta.samplePosition;

            RecordedMidiEvent rec;
            rec.sampleOffset = currentLogicalSample_ + samplePos;
            rec.channel = msg.getChannel();
            rec.noteNumber = msg.getNoteNumber();
            rec.velocity = msg.getFloatVelocity();
            rec.isNoteOn = msg.isNoteOn();
            rec.isNoteOff = msg.isNoteOff();
            rec.isAllNotesOff = msg.isAllNotesOff();
            midiTrace_.push_back(rec);

            if (msg.isNoteOn() && msg.getFloatVelocity() > 0.0f)
            {
                activeNote_ = msg.getNoteNumber();
                activeVelocity_ = msg.getFloatVelocity();
                frequencyHz_ = 440.0 * std::pow(2.0, (static_cast<double>(activeNote_) - 69.0) / 12.0);
                phaseDelta_ = (2.0 * 3.14159265358979323846 * frequencyHz_) / sampleRate_;
                isPlaying_ = true;
            }
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getFloatVelocity() == 0.0f))
            {
                if (msg.getNoteNumber() == activeNote_ || activeNote_ < 0)
                {
                    isPlaying_ = false;
                }
            }
            else if (msg.isAllNotesOff())
            {
                isPlaying_ = false;
            }
        }

        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        // 2. Renderizar muestras senoidales deterministas
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;
            if (isPlaying_)
            {
                s = static_cast<float>(std::sin(currentPhase_)) * (activeVelocity_ * 0.5f);
                currentPhase_ += phaseDelta_;
                if (currentPhase_ >= 2.0 * 3.14159265358979323846)
                    currentPhase_ -= 2.0 * 3.14159265358979323846;
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.setSample(ch, i, s);
            }
        }

        currentLogicalSample_ += numSamples;
    }

    void resetFixture() noexcept
    {
        currentPhase_ = 0.0;
        phaseDelta_ = 0.0;
        frequencyHz_ = 440.0;
        activeNote_ = 60;
        activeVelocity_ = 0.8f;
        isPlaying_ = false;
        currentLogicalSample_ = 0;
        midiTrace_.clear();
    }

    [[nodiscard]] const std::vector<RecordedMidiEvent>& getMidiTrace() const noexcept { return midiTrace_; }
    [[nodiscard]] int64_t getProcessedSamples() const noexcept { return currentLogicalSample_; }

    // Stubs obligatorios de juce::AudioPluginInstance
    const juce::String getName() const override { return "SyntheticAudioFixture"; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init Sine"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    void fillInPluginDescription(juce::PluginDescription& desc) const override { desc = desc_; }

private:
    juce::PluginDescription desc_;
    double sampleRate_ { 48000.0 };
    double currentPhase_ { 0.0 };
    double phaseDelta_ { 0.0 };
    double frequencyHz_ { 440.0 };
    int activeNote_ { 60 };
    float activeVelocity_ { 0.8f };
    bool isPlaying_ { false };
    int64_t currentLogicalSample_ { 0 };
    std::vector<RecordedMidiEvent> midiTrace_;
};

/**
 * @class MockAudioEngine
 * @brief Conductor determinista de callbacks para tests headless (INTEGRATION-01).
 *
 * Conduce audioDeviceIOCallbackWithContext() sobre LabAudioEngine sin WASAPI,
 * sin hilos de audio de sistema operativo, y sin usar reloj de pared ni sleep().
 * El avance temporal se calcula estrictamente por muestras: t = bloque * 256.
 */
class MockAudioEngine
{
public:
    struct PumpResult
    {
        bool success { false };
        int pumpedBlocks { 0 };
        int64_t processedSamples { 0 };
        std::string stopReason;
        std::string diagnosticReport;
    };

    explicit MockAudioEngine(audio::LabAudioEngine& engine)
        : engine_(engine)
    {
        prepare(48000.0, 256, 2);
    }

    ~MockAudioEngine() = default;

    /**
     * @brief Prepara buffers preasignados con cero asignaciones en el ciclo de bombeo.
     */
    void prepare(double sampleRate = 48000.0, int blockSize = 256, int channels = 2)
    {
        sampleRate_ = sampleRate;
        blockSize_ = blockSize;
        channels_ = std::clamp(channels, 1, 2);
        currentBlock_ = 0;

        inputBufferL_.assign(static_cast<size_t>(blockSize_), 0.0f);
        inputBufferR_.assign(static_cast<size_t>(blockSize_), 0.0f);
        outputBufferL_.assign(static_cast<size_t>(blockSize_), 0.0f);
        outputBufferR_.assign(static_cast<size_t>(blockSize_), 0.0f);

        inputChannels_[0] = inputBufferL_.data();
        inputChannels_[1] = inputBufferR_.data();
        outputChannels_[0] = outputBufferL_.data();
        outputChannels_[1] = outputBufferR_.data();

        capturedAudioL_.clear();
        capturedAudioR_.clear();
        // Pre-reservar capacidad típica para evitar realocaciones (ej: 10 segundos @ 48kHz)
        capturedAudioL_.reserve(48000 * 10);
        capturedAudioR_.reserve(48000 * 10);
    }

    /**
     * @brief Bombea exactamente un bloque de 256 muestras a través del callback real.
     */
    void pumpOneBlock()
    {
        // Limpiar buffers de salida antes del callback
        std::fill(outputBufferL_.begin(), outputBufferL_.end(), 0.0f);
        std::fill(outputBufferR_.begin(), outputBufferR_.end(), 0.0f);

        // Invocar el callback real del motor de audio (LabAudioEngine)
        engine_.audioDeviceIOCallbackWithContext(
            inputChannels_, channels_,
            outputChannels_, channels_,
            blockSize_, dummyContext_
        );

        // Almacenar el bloque capturado
        capturedAudioL_.insert(capturedAudioL_.end(), outputBufferL_.begin(), outputBufferL_.end());
        capturedAudioR_.insert(capturedAudioR_.end(), outputBufferR_.begin(), outputBufferR_.end());

        currentBlock_++;
    }

    /**
     * @brief Bombea bloques deterministamente hasta que la condición se cumple o se supera maxBlocks.
     * Cero sleep(): utiliza juce::Thread::yield() para dar oportunidad al worker thread del secuenciador.
     */
    PumpResult pumpUntil(std::function<bool()> completedPredicate,
                         int maxBlocks = 5000,
                         std::function<std::string()> statusDiagnostics = nullptr)
    {
        PumpResult res;
        int pumpedBlocks = 0;
        int idleYieldCycles = 0;
        constexpr int maxIdleYieldCycles = 2000;

        while (pumpedBlocks < maxBlocks)
        {
            if (completedPredicate && completedPredicate())
            {
                res.success = true;
                res.pumpedBlocks = pumpedBlocks;
                res.processedSamples = static_cast<int64_t>(pumpedBlocks) * blockSize_;
                res.stopReason = "PredicateSatisfied";
                return res;
            }

            auto receiverState = engine_.getResponseReceiver().getState();
            if (receiverState == audio::ReceiverState::Recording ||
                receiverState == audio::ReceiverState::WaitingForTrigger)
            {
                pumpOneBlock();
                pumpedBlocks++;
                idleYieldCycles = 0;
            }
            else
            {
                // El secuenciador está en transición (settling, calibración previa o análisis)
                juce::Thread::sleep(5);
                idleYieldCycles++;
                if (idleYieldCycles > maxIdleYieldCycles)
                {
                    break;
                }
            }
        }

        if (completedPredicate && completedPredicate())
        {
            res.success = true;
            res.pumpedBlocks = pumpedBlocks;
            res.processedSamples = static_cast<int64_t>(pumpedBlocks) * blockSize_;
            res.stopReason = "PredicateSatisfiedAtLimit";
            return res;
        }

        // Timeout determinista superado
        res.success = false;
        res.pumpedBlocks = pumpedBlocks;
        res.processedSamples = static_cast<int64_t>(pumpedBlocks) * blockSize_;
        res.stopReason = (idleYieldCycles > maxIdleYieldCycles) ? "TimedOutStalledInIdleState" : "TimedOutExceededMaxBlocks";

        std::string diag = "Integration parity timed out:\n";
        diag += "  stopReason:       " + res.stopReason + "\n";
        diag += "  maxBlocks:        " + std::to_string(maxBlocks) + "\n";
        diag += "  pumpedBlocks:     " + std::to_string(pumpedBlocks) + "\n";
        diag += "  processedSamples: " + std::to_string(res.processedSamples) + "\n";
        diag += "  logicalTimeSec:   " + std::to_string(static_cast<double>(res.processedSamples) / sampleRate_) + " s\n";
        if (statusDiagnostics)
        {
            diag += "  Diagnostics:\n" + statusDiagnostics();
        }
        res.diagnosticReport = diag;

        return res;
    }

    [[nodiscard]] int getPumpedBlockCount() const noexcept { return currentBlock_; }
    [[nodiscard]] int64_t getProcessedSampleCount() const noexcept { return static_cast<int64_t>(currentBlock_) * blockSize_; }
    [[nodiscard]] const std::vector<float>& getCapturedOutputL() const noexcept { return capturedAudioL_; }
    [[nodiscard]] const std::vector<float>& getCapturedOutputR() const noexcept { return capturedAudioR_; }

    // --- Helpers de Comparación Numérica Canónica (Nivel 2 y Nivel 3) ---

    static float computeMaxAbsoluteDifference(const std::vector<float>& a, const std::vector<float>& b) noexcept
    {
        const size_t n = std::min(a.size(), b.size());
        float maxDiff = 0.0f;
        for (size_t i = 0; i < n; ++i)
        {
            maxDiff = std::max(maxDiff, std::abs(a[i] - b[i]));
        }
        return maxDiff;
    }

    static float computeRmse(const std::vector<float>& a, const std::vector<float>& b) noexcept
    {
        const size_t n = std::min(a.size(), b.size());
        if (n == 0) return 0.0f;
        double sumSq = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
            sumSq += diff * diff;
        }
        return static_cast<float>(std::sqrt(sumSq / static_cast<double>(n)));
    }

    static std::string computeCanonicalBufferSha256(const std::vector<float>& buffer)
    {
        if (buffer.empty())
            return "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

        return abdaudiolab::synth::Sha256::computeHex(buffer.data(), buffer.size() * sizeof(float));
    }

    static float computePeakDbfs(const std::vector<float>& buffer) noexcept
    {
        float peak = 0.0f;
        for (float s : buffer)
        {
            peak = std::max(peak, std::abs(s));
        }
        return (peak > 1e-7f) ? (20.0f * std::log10(peak)) : -140.0f;
    }

    static float computeRmsDbfs(const std::vector<float>& buffer) noexcept
    {
        if (buffer.empty()) return -140.0f;
        double sumSq = 0.0;
        for (float s : buffer)
        {
            sumSq += static_cast<double>(s * s);
        }
        float rms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(buffer.size())));
        return (rms > 1e-7f) ? (20.0f * std::log10(rms)) : -140.0f;
    }

private:
    audio::LabAudioEngine& engine_;
    double sampleRate_ { 48000.0 };
    int blockSize_ { 256 };
    int channels_ { 2 };
    int currentBlock_ { 0 };

    std::vector<float> inputBufferL_;
    std::vector<float> inputBufferR_;
    std::vector<float> outputBufferL_;
    std::vector<float> outputBufferR_;

    const float* inputChannels_[2] { nullptr, nullptr };
    float* outputChannels_[2] { nullptr, nullptr };
    juce::AudioIODeviceCallbackContext dummyContext_;

    std::vector<float> capturedAudioL_;
    std::vector<float> capturedAudioR_;
};

} // namespace abdaudiolab::test::support
