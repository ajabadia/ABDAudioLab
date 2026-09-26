#include "MockMidiTransport.h"

namespace abdaudiolab::tests
{

hardware::MidiTransportOpenResult MockMidiTransport::open(const hardware::MidiPortSelection& selection)
{
    if (m_nextOpenError.has_value())
    {
        auto err = *m_nextOpenError;
        auto code = m_nextOpenDiagCode;
        m_nextOpenError.reset();
        m_nextOpenDiagCode.clear();
        m_isOpen = false;
        return hardware::MidiTransportOpenResult::fail(err, code, "Simulated open failure");
    }

    if (selection.stableDeviceId.empty())
    {
        m_isOpen = false;
        return hardware::MidiTransportOpenResult::fail(
            hardware::MidiTransportError::InvalidPortSelection,
            "ERR_MIDI_INVALID_SELECTION",
            "stableDeviceId cannot be empty"
        );
    }

    m_activePort = selection;
    m_isOpen = true;
    m_isDisconnected = false;
    return hardware::MidiTransportOpenResult::ok();
}

hardware::MidiTransportSendResult MockMidiTransport::sendCc(const hardware::MidiCcMessage& message)
{
    if (m_isDisconnected)
    {
        return hardware::MidiTransportSendResult::fail(
            hardware::MidiTransportError::Disconnected,
            "ERR_MIDI_DISCONNECTED",
            "Transport is in disconnected state",
            m_currentSequenceNumber
        );
    }

    if (!m_isOpen)
    {
        return hardware::MidiTransportSendResult::fail(
            hardware::MidiTransportError::NotOpen,
            "ERR_MIDI_NOT_OPEN",
            "Transport has not been opened",
            m_currentSequenceNumber
        );
    }

    if (m_nextSendError.has_value())
    {
        auto err = *m_nextSendError;
        auto code = m_nextSendDiagCode;
        m_nextSendError.reset();
        m_nextSendDiagCode.clear();
        return hardware::MidiTransportSendResult::fail(
            err,
            code,
            "Simulated send failure",
            m_currentSequenceNumber
        );
    }

    if (!message.isValid())
    {
        return hardware::MidiTransportSendResult::fail(
            hardware::MidiTransportError::InvalidMessage,
            "ERR_MIDI_INVALID_CC_MESSAGE",
            "CC message channel or values out of bounds",
            m_currentSequenceNumber
        );
    }

    ++m_currentSequenceNumber;
    auto recorded = message;
    recorded.sequenceNumber = m_currentSequenceNumber;
    m_sentCc.push_back(recorded);

    return hardware::MidiTransportSendResult::ok(m_currentSequenceNumber, m_mockTimestampSamples);
}

hardware::MidiTransportSendResult MockMidiTransport::sendSysEx(const hardware::MidiSysExMessage& message)
{
    if (m_isDisconnected)
    {
        return hardware::MidiTransportSendResult::fail(
            hardware::MidiTransportError::Disconnected,
            "ERR_MIDI_DISCONNECTED",
            "Transport is in disconnected state",
            m_currentSequenceNumber
        );
    }

    if (!m_isOpen)
    {
        return hardware::MidiTransportSendResult::fail(
            hardware::MidiTransportError::NotOpen,
            "ERR_MIDI_NOT_OPEN",
            "Transport has not been opened",
            m_currentSequenceNumber
        );
    }

    if (m_nextSendError.has_value())
    {
        auto err = *m_nextSendError;
        auto code = m_nextSendDiagCode;
        m_nextSendError.reset();
        m_nextSendDiagCode.clear();
        return hardware::MidiTransportSendResult::fail(
            err,
            code,
            "Simulated send failure",
            m_currentSequenceNumber
        );
    }

    if (!message.isValidSysExFrame())
    {
        return hardware::MidiTransportSendResult::fail(
            hardware::MidiTransportError::InvalidMessage,
            "ERR_MIDI_INVALID_SYSEX_FRAME",
            "SysEx frame lacks F0/F7 delimiters or contains bytes > 0x7F",
            m_currentSequenceNumber
        );
    }

    ++m_currentSequenceNumber;
    auto recorded = message;
    recorded.sequenceNumber = m_currentSequenceNumber;
    m_sentSysEx.push_back(recorded);

    return hardware::MidiTransportSendResult::ok(m_currentSequenceNumber, m_mockTimestampSamples);
}

void MockMidiTransport::close() noexcept
{
    m_isOpen = false;
    m_activePort = {};
}

bool MockMidiTransport::isOpen() const noexcept
{
    return m_isOpen && !m_isDisconnected;
}

hardware::MidiPortSelection MockMidiTransport::getActivePortSelection() const noexcept
{
    return m_activePort;
}

const std::vector<hardware::MidiCcMessage>& MockMidiTransport::sentCc() const noexcept
{
    return m_sentCc;
}

const std::vector<hardware::MidiSysExMessage>& MockMidiTransport::sentSysEx() const noexcept
{
    return m_sentSysEx;
}

void MockMidiTransport::failNextOpen(hardware::MidiTransportError error, std::string diagnosticCode)
{
    m_nextOpenError = error;
    m_nextOpenDiagCode = std::move(diagnosticCode);
}

void MockMidiTransport::failNextSend(hardware::MidiTransportError error, std::string diagnosticCode)
{
    m_nextSendError = error;
    m_nextSendDiagCode = std::move(diagnosticCode);
}

void MockMidiTransport::simulateDisconnect()
{
    m_isDisconnected = true;
}

void MockMidiTransport::reset()
{
    m_isOpen = false;
    m_isDisconnected = false;
    m_activePort = {};
    m_nextOpenError.reset();
    m_nextOpenDiagCode.clear();
    m_nextSendError.reset();
    m_nextSendDiagCode.clear();
    m_sentCc.clear();
    m_sentSysEx.clear();
    m_currentSequenceNumber = 0;
    m_mockTimestampSamples = 0;
}

void MockMidiTransport::setMockTimestampSamples(uint64_t timestamp) noexcept
{
    m_mockTimestampSamples = timestamp;
}

} // namespace abdaudiolab::tests
