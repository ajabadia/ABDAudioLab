#include "ReferenceSynthProcessor.h"
#include <cmath>
#include <numbers>

namespace abdaudiolab::fixtures
{

ReferenceSynthProcessor::ReferenceSynthProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // 1. Parámetros de audio (AudioControl)
    addParameter(cutoffParam_ = new juce::AudioParameterFloat(
        juce::ParameterID("cutoff", 1), "Filter Cutoff",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

    addParameter(resonanceParam_ = new juce::AudioParameterFloat(
        juce::ParameterID("resonance", 1), "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));

    addParameter(attackParam_ = new juce::AudioParameterFloat(
        juce::ParameterID("attack", 1), "Envelope Attack",
        juce::NormalisableRange<float>(0.001f, 2.0f), 0.010f));

    addParameter(gainParam_ = new juce::AudioParameterFloat(
        juce::ParameterID("gain", 1), "Master Gain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

    addParameter(pitchBendParam_ = new juce::AudioParameterFloat(
        juce::ParameterID("pitch", 1), "Pitch Bend",
        juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));

    // 2. Parámetros técnicos de infraestructura (TestInfrastructure)
    addParameter(controlModeParam_ = new juce::AudioParameterChoice(
        juce::ParameterID("test_control_mode", 1), "Test Control Mode",
        juce::StringArray{ "SampleAccurate", "BlockQuantized" }, 0));

    addParameter(smoothingTimeParam_ = new juce::AudioParameterFloat(
        juce::ParameterID("test_smoothing_ms", 1), "Test Smoothing Time Ms",
        juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f));

    addParameter(latencyParam_ = new juce::AudioParameterInt(
        juce::ParameterID("test_declared_latency", 1), "Test Declared Latency Samples", 0, 512, 0));
}

void ReferenceSynthProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 96000.0;
    currentPhase_ = 0.0;
    phaseIncrement_ = 0.0;
    isNoteActive_ = false;
    envLevel_ = 0.0;
    s1_ = 0.0;
    s2_ = 0.0;
    smoothedCutoff_ = static_cast<double>(cutoffParam_->get());

    if (latencyParam_ != nullptr)
    {
        setLatencySamples(latencyParam_->get());
    }
}

void ReferenceSynthProcessor::releaseResources()
{
}

void ReferenceSynthProcessor::reset()
{
    currentPhase_ = 0.0;
    phaseIncrement_ = 0.0;
    isNoteActive_ = false;
    envLevel_ = 0.0;
    s1_ = 0.0;
    s2_ = 0.0;
    smoothedCutoff_ = (cutoffParam_ != nullptr) ? static_cast<double>(cutoffParam_->get()) : 0.8;
}

bool ReferenceSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Salida estéreo o mono soportada
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    {
        return false;
    }
    return true;
}

void ReferenceSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    buffer.clear();

    if (latencyParam_ != nullptr)
    {
        setLatencySamples(latencyParam_->get());
    }

    bool isBlockQuantized = (controlModeParam_ != nullptr && controlModeParam_->getIndex() == 1);
    double smoothingTimeMs = (smoothingTimeParam_ != nullptr) ? static_cast<double>(smoothingTimeParam_->get()) : 0.0;
    double smoothingCoeff = 1.0;
    if (smoothingTimeMs > 0.001)
    {
        smoothingCoeff = 1.0 - std::exp(-1000.0 / (smoothingTimeMs * sampleRate_));
    }

    double blockCutoff = static_cast<double>(cutoffParam_->get());
    double blockResonance = static_cast<double>(resonanceParam_->get());
    double blockAttack = static_cast<double>(attackParam_->get());
    double blockGain = static_cast<double>(gainParam_->get());
    double blockPitch = static_cast<double>(pitchBendParam_->get());

    auto midiIter = midiMessages.findNextSamplePosition(0);

    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
    {
        // 1. Despachar eventos MIDI en el sampleOffset exacto
        while (midiIter != midiMessages.end() && (*midiIter).samplePosition == sampleIdx)
        {
            auto meta = *midiIter;
            auto msg = meta.getMessage();

            if (msg.isNoteOn())
            {
                activeMidiNote_ = msg.getNoteNumber();
                activeVelocity_ = msg.getFloatVelocity();
                isNoteActive_ = true;
                currentPhase_ = 0.0; // Reset de fase canónico al inicio de nota

                double targetFreq = 440.0 * std::pow(2.0, (static_cast<double>(activeMidiNote_) - 69.0 + blockPitch) / 12.0);
                phaseIncrement_ = 2.0 * std::numbers::pi * targetFreq / sampleRate_;
            }
            else if (msg.isNoteOff())
            {
                if (msg.getNoteNumber() == activeMidiNote_)
                {
                    isNoteActive_ = false;
                }
            }
            else if (msg.isAllNotesOff())
            {
                isNoteActive_ = false;
                envLevel_ = 0.0;
            }

            midiIter++;
        }

        // 2. Control de envolvente de amplitud
        double attackSamples = std::max(1.0, blockAttack * sampleRate_);
        double attackInc = 1.0 / attackSamples;

        if (isNoteActive_)
        {
            envLevel_ = std::min(1.0, envLevel_ + attackInc);
        }
        else
        {
            envLevel_ = std::max(0.0, envLevel_ - attackInc * 2.0);
        }

        // 3. Suavizado o cuantización de controles
        double currentCutoff = isBlockQuantized ? blockCutoff : static_cast<double>(cutoffParam_->get());
        if (smoothingTimeMs > 0.001)
        {
            smoothedCutoff_ += smoothingCoeff * (currentCutoff - smoothedCutoff_);
        }
        else
        {
            smoothedCutoff_ = currentCutoff;
        }

        // 4. Oscilador analógico virtual con armónicos h1, h2, h3
        double rawOsc = 0.0;
        if (envLevel_ > 1e-6)
        {
            double h2 = 0.35 * smoothedCutoff_;
            double h3 = 0.20 * smoothedCutoff_ * smoothedCutoff_;
            rawOsc = std::sin(currentPhase_) + h2 * std::sin(currentPhase_ * 2.0) + h3 * std::sin(currentPhase_ * 3.0);
            rawOsc = (rawOsc / 1.55);

            currentPhase_ += phaseIncrement_;
            if (currentPhase_ >= 2.0 * std::numbers::pi)
            {
                currentPhase_ -= 2.0 * std::numbers::pi;
            }
        }

        // 5. Filtro TPT Lowpass de 2 polos (ZDF / State Variable Filter)
        // Ley exponencial de cutoff: 20 Hz a 20 kHz
        double cutoffHz = 20.0 * std::pow(1000.0, std::clamp(smoothedCutoff_, 0.0, 1.0));
        cutoffHz = std::min(cutoffHz, sampleRate_ * 0.45);
        double g = std::tan(std::numbers::pi * cutoffHz / sampleRate_);
        double r = 1.0 - 0.95 * std::clamp(blockResonance, 0.0, 1.0); // Amortiguamiento
        double h = 1.0 / (1.0 + 2.0 * r * g + g * g);

        double inSample = rawOsc;
        double yHP = (inSample - (2.0 * r + g) * s1_ - s2_) * h;
        double yBP = g * yHP + s1_;
        s1_ = g * yHP + yBP;
        double yLP = g * yBP + s2_;
        s2_ = g * yBP + yLP;

        float outSample = static_cast<float>(yLP * envLevel_ * activeVelocity_ * blockGain);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.setSample(ch, sampleIdx, outSample);
        }
    }

    // 6. Actualizar el canal privado de Ground Truth exclusivamente para el oráculo de tests
    groundTruthState_.realInternalCutoffHz = 20.0 * std::pow(1000.0, smoothedCutoff_);
    groundTruthState_.realResonanceQ = 1.0 / (2.0 * (1.0 - 0.95 * blockResonance));
    groundTruthState_.realAttackSec = blockAttack;
    groundTruthState_.realGainLinear = blockGain;
    groundTruthState_.realPitchSemitones = blockPitch;
    groundTruthState_.isBlockQuantizedActive = isBlockQuantized;
    groundTruthState_.realSmoothingTimeMs = smoothingTimeMs;
    groundTruthState_.realPipelineLatencySamples = (latencyParam_ != nullptr) ? latencyParam_->get() : 0;
    groundTruthState_.activeVoiceCount = isNoteActive_ ? 1 : 0;
}

ReferenceSynthGroundTruth ReferenceSynthProcessor::getPrivateGroundTruthForTesting() const noexcept
{
    return groundTruthState_;
}

void ReferenceSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    stream.writeInt(0x52454653); // Magic 'REFS'
    stream.writeInt(1);          // Versión 1

    const auto& params = getParameters();
    stream.writeInt(params.size());
    for (auto* p : params)
    {
        stream.writeFloat(p != nullptr ? p->getValue() : 0.0f);
    }
}

void ReferenceSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes < 12)
        return;

    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    int magic = stream.readInt();
    if (magic != 0x52454653)
        return;

    int version = stream.readInt();
    if (version == 1)
    {
        int numParams = stream.readInt();
        const auto& params = getParameters();
        int toRead = std::min(numParams, params.size());
        for (int i = 0; i < toRead; ++i)
        {
            float val = stream.readFloat();
            if (params[i] != nullptr)
            {
                params[i]->setValueNotifyingHost(val);
            }
        }
    }
}

juce::AudioProcessorEditor* ReferenceSynthProcessor::createEditor()
{
    return nullptr;
}

bool ReferenceSynthProcessor::hasEditor() const
{
    return false;
}

const juce::String ReferenceSynthProcessor::getName() const
{
    return "ReferenceSynth";
}

bool ReferenceSynthProcessor::acceptsMidi() const
{
    return true;
}

bool ReferenceSynthProcessor::producesMidi() const
{
    return false;
}

bool ReferenceSynthProcessor::isMidiEffect() const
{
    return false;
}

double ReferenceSynthProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ReferenceSynthProcessor::getNumPrograms()
{
    return 1;
}

int ReferenceSynthProcessor::getCurrentProgram()
{
    return 0;
}

void ReferenceSynthProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String ReferenceSynthProcessor::getProgramName(int /*index*/)
{
    return "Default";
}

void ReferenceSynthProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

} // namespace abdaudiolab::fixtures

// Función de exportación de plugin para compilación VST3
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new abdaudiolab::fixtures::ReferenceSynthProcessor();
}
