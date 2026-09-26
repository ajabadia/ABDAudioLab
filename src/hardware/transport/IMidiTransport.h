#pragma once

#include "MidiTransportTypes.h"

namespace abdaudiolab::hardware
{

class IMidiTransport
{
public:
    virtual ~IMidiTransport() = default;

    virtual MidiTransportOpenResult open(const MidiPortSelection& selection) = 0;
    virtual MidiTransportSendResult sendCc(const MidiCcMessage& message) = 0;
    virtual MidiTransportSendResult sendSysEx(const MidiSysExMessage& message) = 0;
    virtual void close() noexcept = 0;

    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    [[nodiscard]] virtual MidiPortSelection getActivePortSelection() const noexcept = 0;
};

} // namespace abdaudiolab::hardware
