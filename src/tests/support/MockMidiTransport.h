#pragma once

#include "hardware/transport/IMidiTransport.h"
#include <optional>
#include <vector>
#include <string>

namespace abdaudiolab::tests
{

class MockMidiTransport final : public hardware::IMidiTransport
{
public:
    MockMidiTransport() = default;
    ~MockMidiTransport() override = default;

    hardware::MidiTransportOpenResult open(const hardware::MidiPortSelection& selection) override;
    hardware::MidiTransportSendResult sendCc(const hardware::MidiCcMessage& message) override;
    hardware::MidiTransportSendResult sendSysEx(const hardware::MidiSysExMessage& message) override;
    void close() noexcept override;

    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] hardware::MidiPortSelection getActivePortSelection() const noexcept override;

    [[nodiscard]] const std::vector<hardware::MidiCcMessage>& sentCc() const noexcept;
    [[nodiscard]] const std::vector<hardware::MidiSysExMessage>& sentSysEx() const noexcept;

    void failNextOpen(hardware::MidiTransportError error, std::string diagnosticCode = "ERR_SIMULATED_OPEN_FAILURE");
    void failNextSend(hardware::MidiTransportError error, std::string diagnosticCode = "ERR_SIMULATED_SEND_FAILURE");
    void simulateDisconnect();
    void reset();
    void setMockTimestampSamples(uint64_t timestamp) noexcept;

private:
    bool m_isOpen { false };
    bool m_isDisconnected { false };
    hardware::MidiPortSelection m_activePort;

    std::optional<hardware::MidiTransportError> m_nextOpenError;
    std::string m_nextOpenDiagCode;

    std::optional<hardware::MidiTransportError> m_nextSendError;
    std::string m_nextSendDiagCode;

    std::vector<hardware::MidiCcMessage> m_sentCc;
    std::vector<hardware::MidiSysExMessage> m_sentSysEx;
    uint64_t m_currentSequenceNumber { 0 };
    uint64_t m_mockTimestampSamples { 0 };
};

} // namespace abdaudiolab::tests
