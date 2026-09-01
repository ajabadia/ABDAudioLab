#pragma once

#include "HardwareController.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <map>

namespace abdaudiolab::hardware
{

/**
 * @brief Standard MIDI Continuous Controller (CC) hardware driver.
 */
class MidiCcController : public IHardwareController
{
public:
    explicit MidiCcController(juce::String deviceIdentifier = "", int midiChannel = 1)
        : targetDeviceIdentifier(std::move(deviceIdentifier)), channel(std::clamp(midiChannel, 1, 16))
    {
    }

    ~MidiCcController() override
    {
        disconnect();
    }

    void setTargetDeviceIdentifier(const juce::String& identifier)
    {
        targetDeviceIdentifier = identifier;
    }

    void setChannel(int newChannel)
    {
        channel = std::clamp(newChannel, 1, 16);
    }

    [[nodiscard]] bool isAutomatic() const noexcept override { return true; }

    [[nodiscard]] juce::String getHardwareName() const override
    {
        if (midiOut != nullptr)
            return "MIDI CC: " + midiOut->getName();
        return "Generic MIDI CC Device (Channel " + juce::String(channel) + ")";
    }

    bool connect() override
    {
        auto midiOutputs = juce::MidiOutput::getAvailableDevices();
        if (midiOutputs.isEmpty())
            return false;

        if (targetDeviceIdentifier.isNotEmpty())
        {
            for (const auto& d : midiOutputs)
            {
                if (d.identifier == targetDeviceIdentifier || d.name == targetDeviceIdentifier)
                {
                    midiOut = juce::MidiOutput::openDevice(d.identifier);
                    return midiOut != nullptr;
                }
            }
        }

        // Open first available
        midiOut = juce::MidiOutput::openDevice(midiOutputs[0].identifier);
        return midiOut != nullptr;
    }

    void disconnect() override
    {
        midiOut.reset();
    }

    [[nodiscard]] bool isConnected() const noexcept override
    {
        return midiOut != nullptr;
    }

    bool setParameter(int paramIndex, float normalizedValue) override
    {
        int raw = std::clamp(static_cast<int>(std::lround(normalizedValue * 127.0f)), 0, 127);
        return setParameterRaw(paramIndex, raw);
    }

    bool setParameterRaw(int paramIndex, int rawValue) override
    {
        if (!isConnected())
            return false;

        uint8_t val = static_cast<uint8_t>(std::clamp(rawValue, 0, 127));
        midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(channel, paramIndex, val));
        cachedValues[paramIndex] = val;
        return true;
    }

    bool setupSubmodule(int /*slotIndex*/, uint8_t /*typeId*/) override
    {
        return true; // No-op on generic CC synths
    }

    bool setPatchCable(uint8_t /*sourceId*/, uint8_t /*destId*/, bool /*isConnected*/) override
    {
        return true; // No-op on fixed architecture CC synths
    }

    void requestStateDump() override
    {
    }

private:
    juce::String targetDeviceIdentifier;
    int channel { 1 };
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::map<int, uint8_t> cachedValues;
};

} // namespace abdaudiolab::hardware
