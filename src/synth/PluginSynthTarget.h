#pragma once

#include "ISynthTarget.h"
#include "Sha256.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace abdaudiolab::synth
{

/**
 * @brief Adaptador para instrumentos virtuales VST3/AU hospedados en memoria RAM.
 * 
 * Despacha eventos MIDI con sampleOffset exacto sub-bloque hacia el processBlock
 * del plugin y captura el audio digital directo sin pasar por convertidores ADC/DAC.
 */
class PluginSynthTarget : public ISynthTarget
{
public:
    explicit PluginSynthTarget(juce::AudioProcessor& processor)
        : processor_(processor)
    {
    }

    bool loadState(const SynthPresetState& state) override
    {
        if (!state.rawSysEx.empty())
        {
            processor_.setStateInformation(state.rawSysEx.data(), static_cast<int>(state.rawSysEx.size()));
        }

        // Aplicar parámetros normalizados por índice o ID
        auto params = processor_.getParameters();
        for (const auto& np : state.normalizedParameters)
        {
            for (auto* p : params)
            {
                if (p != nullptr && p->getName(32).toStdString() == np.name)
                {
                    p->setValueNotifyingHost(static_cast<float>(np.value));
                    break;
                }
            }
        }

        isStateVerified_ = true;
        return true;
    }

    [[nodiscard]] StateAppliedStatus verifyState() const override
    {
        return isStateVerified_ ? StateAppliedStatus::Passed : StateAppliedStatus::Unverified;
    }

    void prepare(const ProcessingSpec& spec) override
    {
        spec_ = spec;
        processor_.setPlayConfigDetails(0, spec.numChannels, spec.sampleRate, spec.blockSize);
        processor_.prepareToPlay(spec.sampleRate, spec.blockSize);
    }

    void resetState() override
    {
        processor_.reset();
    }

    void render(const MidiExcitationSequence& sequence,
                std::vector<float>& destinationAudio,
                int /*repetitionIndex*/ = 0) override
    {
        int totalSamples = static_cast<int>(std::lround(sequence.totalDurationSec * spec_.sampleRate));
        destinationAudio.assign(static_cast<size_t>(totalSamples), 0.0f);

        juce::AudioBuffer<float> blockBuf(spec_.numChannels, spec_.blockSize);
        juce::MidiBuffer midiBuf;

        int samplesRendered = 0;
        while (samplesRendered < totalSamples)
        {
            int currentBlockSize = std::min(spec_.blockSize, totalSamples - samplesRendered);
            blockBuf.setSize(spec_.numChannels, currentBlockSize, false, false, true);
            blockBuf.clear();
            midiBuf.clear();

            // Inyectar eventos MIDI con sampleOffset exacto dentro del bloque
            for (const auto& ev : sequence.events)
            {
                if (ev.sampleOffset >= samplesRendered && ev.sampleOffset < (samplesRendered + currentBlockSize))
                {
                    int offsetInBlock = ev.sampleOffset - samplesRendered;
                    juce::MidiMessage msg;

                    if (ev.type == TimedMidiType::NoteOn)
                        msg = juce::MidiMessage::noteOn(ev.channel, ev.noteNumber, ev.velocity);
                    else if (ev.type == TimedMidiType::NoteOff)
                        msg = juce::MidiMessage::noteOff(ev.channel, ev.noteNumber, 0.0f);
                    else if (ev.type == TimedMidiType::AllNotesOff)
                        msg = juce::MidiMessage::allNotesOff(ev.channel);

                    if (msg.getRawDataSize() > 0)
                        midiBuf.addEvent(msg, offsetInBlock);
                }
            }

            // Inyectar eventos de automatización de parámetros en este bloque
            for (const auto& pe : sequence.parameterEvents)
            {
                if (pe.sampleOffset >= samplesRendered && pe.sampleOffset < (samplesRendered + currentBlockSize))
                {
                    for (auto* p : processor_.getParameters())
                    {
                        if (p != nullptr)
                        {
                            std::string pName = p->getName(32).toStdString();
                            if (pName == pe.normalizedParameterId || pName == pe.nativeParameterId)
                            {
                                p->setValueNotifyingHost(static_cast<float>(pe.normalizedValue));
                                break;
                            }
                        }
                    }
                }
            }

            processor_.processBlock(blockBuf, midiBuf);

            // Copiar salida del canal izquierdo a destinationAudio
            const float* outChannel = blockBuf.getReadPointer(0);
            for (int i = 0; i < currentBlockSize; ++i)
            {
                destinationAudio[static_cast<size_t>(samplesRendered + i)] = outChannel[i];
            }

            samplesRendered += currentBlockSize;
        }
    }

    [[nodiscard]] TargetTimingInfo timingInfo() const override
    {
        TargetTimingInfo info;
        info.isPhysicalHardware = false;
        info.isDirectPlugin = true;
        info.declaredLatencySamples = static_cast<double>(processor_.getLatencySamples());
        info.measuredTransportLatencyMs = (spec_.sampleRate > 0.0) ? (info.declaredLatencySamples / spec_.sampleRate * 1000.0) : 0.0;
        info.timingJitterMs = 0.0; // En bus de software interno directo en RAM el jitter de transporte es nulo
        info.timingDescription = "Direct RAM Hosted Plugin (Sample-Accurate MidiBuffer)";
        return info;
    }

    [[nodiscard]] bool supportsBinaryState() const override
    {
        return true;
    }

    StateTransferResult getState(std::vector<uint8_t>& stateData) const override
    {
        juce::MemoryBlock mb;
        processor_.getStateInformation(mb);
        stateData.assign(static_cast<const uint8_t*>(mb.getData()),
                         static_cast<const uint8_t*>(mb.getData()) + mb.getSize());

        StateTransferResult res;
        res.supported = true;
        res.succeeded = true;
        res.byteCount = mb.getSize();
        res.stateDataHash = Sha256::computeHex(stateData.data(), stateData.size());
        return res;
    }

    StateTransferResult setState(const std::vector<uint8_t>& stateData) override
    {
        StateTransferResult res;
        res.supported = true;
        res.byteCount = stateData.size();
        res.stateDataHash = Sha256::computeHex(stateData.data(), stateData.size());

        if (stateData.empty())
        {
            res.succeeded = false;
            res.errorCode = "EMPTY_STATE_DATA";
            return res;
        }

        processor_.setStateInformation(stateData.data(), static_cast<int>(stateData.size()));
        res.succeeded = true;
        return res;
    }

private:
    juce::AudioProcessor& processor_;
    ProcessingSpec spec_;
    bool isStateVerified_ { false };
};

} // namespace abdaudiolab::synth
