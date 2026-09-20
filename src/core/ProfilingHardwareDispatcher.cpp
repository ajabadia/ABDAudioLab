#include "ProfilingHardwareDispatcher.h"

namespace abdaudiolab::core
{

ProfilingHardwareDispatcher::ProfilingHardwareDispatcher(hardware::IHardwareController* hardwareInterface)
    : hardware(hardwareInterface)
{
}

void ProfilingHardwareDispatcher::setParameter(int paramIndex, float normalizedValue)
{
    if (targetPlugin != nullptr)
    {
        const auto& params = targetPlugin->getParameters();
        int zeroBased = paramIndex - 1;
        if (zeroBased >= 0 && zeroBased < params.size() && params[zeroBased] != nullptr)
        {
            params[zeroBased]->setValueNotifyingHost(std::clamp(normalizedValue, 0.0f, 1.0f));
            return;
        }
    }

    if (hardware != nullptr)
    {
        hardware->setParameter(paramIndex, normalizedValue);
    }
}

void ProfilingHardwareDispatcher::executeLifecycleActions(const std::vector<HardwareSetupAction>& actions)
{
    if (hardware == nullptr)
        return;

    for (const auto& action : actions)
    {
        switch (action.method)
        {
            case HardwareMethod::MIDI_CC:
            {
                int rawValue = juce::jlimit(0, 127, static_cast<int>(std::round(action.normalizedValue * 127.0f)));
                auto msg = juce::MidiMessage::controllerEvent(action.channel, action.controlNumber, rawValue);
                hardware->sendMidiMessage(msg);
                break;
            }
            case HardwareMethod::NRPN:
            {
                int val14Bit = juce::jlimit(0, 16383, static_cast<int>(std::round(action.normalizedValue * 16383.0f)));
                uint8_t paramMSB = static_cast<uint8_t>((action.controlNumber >> 7) & 0x7F);
                uint8_t paramLSB = static_cast<uint8_t>(action.controlNumber & 0x7F);
                uint8_t valMSB   = static_cast<uint8_t>((val14Bit >> 7) & 0x7F);
                uint8_t valLSB   = static_cast<uint8_t>(val14Bit & 0x7F);

                hardware->sendMidiMessage(juce::MidiMessage::controllerEvent(action.channel, 99, paramMSB));
                hardware->sendMidiMessage(juce::MidiMessage::controllerEvent(action.channel, 98, paramLSB));
                hardware->sendMidiMessage(juce::MidiMessage::controllerEvent(action.channel, 6,  valMSB));
                hardware->sendMidiMessage(juce::MidiMessage::controllerEvent(action.channel, 38, valLSB));
                break;
            }
            case HardwareMethod::SYSEX_RAW:
            {
                juce::MemoryBlock sysexBytes;
                sysexBytes.loadFromHexString(action.sysexHexPayload);
                if (sysexBytes.getSize() > 0)
                {
                    auto msg = juce::MidiMessage(sysexBytes.getData(), static_cast<int>(sysexBytes.getSize()));
                    hardware->sendMidiMessage(msg);
                }
                break;
            }
            default:
                break;
        }

        if (action.settlingDelayMs > 0)
        {
            juce::Thread::sleep(action.settlingDelayMs);
        }
    }
}

void ProfilingHardwareDispatcher::executeMeasurementRecipe(const MeasurementPresetRecipe& recipe)
{
    if (hardware == nullptr)
        return;

    if (recipe.recipeType.empty() && recipe.setupActions.empty() && recipe.excitationNotes.empty())
        return;

    // 1. Despachar acciones de preparación del hardware
    executeLifecycleActions(recipe.setupActions);

    // 2. Despachar secuencias preparatorias de notas si existen
    for (const auto& ev : recipe.excitationNotes)
    {
        if (ev.startDelayMs > 0)
            juce::Thread::sleep(ev.startDelayMs);

        sendNoteOn(1, ev.noteNumber, static_cast<float>(ev.velocity) / 127.0f);

        if (!ev.isLegato && ev.durationMs > 0)
        {
            juce::Thread::sleep(ev.durationMs);
            sendNoteOff(1, ev.noteNumber, 0.0f);
        }
    }
}

void ProfilingHardwareDispatcher::sendNoteOn(int channel, int noteNumber, float normalizedVelocity)
{
    if (midiSink)
    {
        midiSink(juce::MidiMessage::noteOn(channel, noteNumber, normalizedVelocity));
    }
    if (hardware != nullptr)
    {
        hardware->sendNoteOn(channel, noteNumber, normalizedVelocity);
    }
}

void ProfilingHardwareDispatcher::sendNoteOff(int channel, int noteNumber, float velocity)
{
    if (midiSink)
    {
        midiSink(juce::MidiMessage::noteOff(channel, noteNumber, velocity));
    }
    if (hardware != nullptr)
    {
        hardware->sendNoteOff(channel, noteNumber, velocity);
    }
}

void ProfilingHardwareDispatcher::sendAllNotesOff(int channel)
{
    if (midiSink)
    {
        midiSink(juce::MidiMessage::allNotesOff(channel));
    }
    if (hardware != nullptr)
    {
        hardware->sendAllNotesOff(channel);
    }
}

void ProfilingHardwareDispatcher::sendAllSoundOff(int channel)
{
    if (midiSink)
    {
        midiSink(juce::MidiMessage::allSoundOff(channel));
    }
    if (hardware != nullptr)
    {
        hardware->sendMidiMessage(juce::MidiMessage::allSoundOff(channel));
    }
}

void ProfilingHardwareDispatcher::injectHardwareModulationValue(ModExcitationType excitationType,
                                                               int channel,
                                                               int controlCCNumber,
                                                               const juce::String& sysexTemplate,
                                                               int rawValue)
{
    if (hardware == nullptr)
        return;

    if (excitationType == ModExcitationType::Velocity)
    {
        float normVelocity = static_cast<float>(juce::jlimit(0, 127, rawValue)) / 127.0f;
        hardware->sendNoteOn(channel, 60, normVelocity);
        juce::Thread::sleep(15); // Duración física de pulso MIDI gate
        hardware->sendNoteOff(channel, 60, 0.0f);
    }
    else if (excitationType == ModExcitationType::CC)
    {
        int ccVal = juce::jlimit(0, 127, rawValue);
        auto msg = juce::MidiMessage::controllerEvent(channel, controlCCNumber, ccVal);
        hardware->sendMidiMessage(msg);
    }
    else if (excitationType == ModExcitationType::Aftertouch)
    {
        int pressureVal = juce::jlimit(0, 127, rawValue);
        auto msg = juce::MidiMessage::channelPressureChange(channel, pressureVal);
        hardware->sendMidiMessage(msg);
    }
    else if (excitationType == ModExcitationType::SysExAmount)
    {
        juce::String hexByte = juce::String::toHexString(juce::jlimit(0, 255, rawValue)).paddedLeft('0', 2);
        juce::String hexPayload = sysexTemplate.replace("XX", hexByte);
        juce::MemoryBlock sysexBytes;
        sysexBytes.loadFromHexString(hexPayload);
        if (sysexBytes.getSize() > 0)
        {
            auto msg = juce::MidiMessage(sysexBytes.getData(), static_cast<int>(sysexBytes.getSize()));
            hardware->sendMidiMessage(msg);
        }
    }
}

} // namespace abdaudiolab::core
