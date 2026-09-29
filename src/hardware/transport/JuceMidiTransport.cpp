#include "JuceMidiTransport.h"

namespace abdaudiolab::hardware
{

JuceMidiTransport::JuceMidiTransport() = default;

JuceMidiTransport::~JuceMidiTransport()
{
    close();
}

MidiTransportOpenResult JuceMidiTransport::open(const MidiPortSelection& selection)
{
    if (selection.stableDeviceId.empty())
    {
        return MidiTransportOpenResult::fail(
            MidiTransportError::InvalidPortSelection,
            "ERR_INVALID_PORT_SELECTION",
            "MidiPortSelection contains empty stableDeviceId.");
    }

    close();

    const auto available = juce::MidiOutput::getAvailableDevices();
    juce::MidiDeviceInfo targetDev;
    bool deviceFound = false;
    for (const auto& dev : available)
    {
        if (dev.identifier == juce::String(selection.stableDeviceId) ||
            dev.name == juce::String(selection.stableDeviceId))
        {
            targetDev = dev;
            deviceFound = true;
            break;
        }
    }

    if (!deviceFound)
    {
        return MidiTransportOpenResult::fail(
            MidiTransportError::OpenFailed,
            "ERR_MIDI_OUTPUT_OPEN_FAILED",
            "Device ID not found in available MIDI outputs: " + selection.stableDeviceId);
    }

    midiOutput_ = juce::MidiOutput::openDevice(targetDev.identifier);
    if (!midiOutput_)
    {
        return MidiTransportOpenResult::fail(
            MidiTransportError::OpenFailed,
            "ERR_MIDI_OUTPUT_OPEN_FAILED",
            "juce::MidiOutput::openDevice failed for deviceId: " + targetDev.identifier.toStdString());
    }

    activePort_ = selection;
    sequenceCounter_ = 0;
    return MidiTransportOpenResult::ok();
}

void JuceMidiTransport::close() noexcept
{
    if (midiOutput_)
    {
        midiOutput_->stopBackgroundThread();
        midiOutput_.reset();
    }
    activePort_ = {};
}

bool JuceMidiTransport::isOpen() const noexcept
{
    return (midiOutput_ != nullptr);
}

MidiPortSelection JuceMidiTransport::getActivePortSelection() const noexcept
{
    return activePort_;
}

MidiTransportSendResult JuceMidiTransport::sendCc(const MidiCcMessage& message)
{
    if (!isOpen())
    {
        return MidiTransportSendResult::fail(
            MidiTransportError::NotOpen,
            "ERR_MIDI_TRANSPORT_NOT_OPEN",
            "Cannot send CC message: transport port is closed.");
    }

    if (!message.isValid())
    {
        return MidiTransportSendResult::fail(
            MidiTransportError::InvalidMessage,
            "ERR_MIDI_INVALID_MESSAGE",
            "CC message parameters out of valid MIDI bounds (channel 1..16, CC 0..127, val 0..127).");
    }

    const auto juceMsg = juce::MidiMessage::controllerEvent(
        static_cast<int>(message.channel),
        static_cast<int>(message.controllerNumber),
        static_cast<int>(message.value));

    midiOutput_->sendMessageNow(juceMsg);
    sequenceCounter_++;

    return MidiTransportSendResult::ok(sequenceCounter_);
}

MidiTransportSendResult JuceMidiTransport::sendSysEx(const MidiSysExMessage& message)
{
    if (!isOpen())
    {
        return MidiTransportSendResult::fail(
            MidiTransportError::NotOpen,
            "ERR_MIDI_TRANSPORT_NOT_OPEN",
            "Cannot send SysEx message: transport port is closed.");
    }

    if (!message.isValidSysExFrame())
    {
        return MidiTransportSendResult::fail(
            MidiTransportError::InvalidMessage,
            "ERR_MIDI_INVALID_SYSEX_FRAME",
            "SysEx message frame is invalid (missing F0/F7 or 7-bit payload violation).");
    }

    const auto juceMsg = juce::MidiMessage::createSysExMessage(
        message.bytes.data(),
        static_cast<int>(message.bytes.size()));

    midiOutput_->sendMessageNow(juceMsg);
    sequenceCounter_++;

    return MidiTransportSendResult::ok(sequenceCounter_);
}

} // namespace abdaudiolab::hardware
