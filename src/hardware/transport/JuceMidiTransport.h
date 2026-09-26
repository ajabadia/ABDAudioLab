#pragma once

#include "IMidiTransport.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>

namespace abdaudiolab::hardware
{

/**
 * @brief Concrete IMidiTransport implementation backed by juce::MidiOutput.
 * 
 * Invariants:
 * - Does not open ports automatically on application boot.
 * - Does not contain embedded pacing, sleeps, or scheduling logic (governed externally by HardwareDispatchScheduler).
 * - Thread-safe for invocation from worker/sequencer pipelines outside the real-time audio callback.
 */
class JuceMidiTransport final : public IMidiTransport
{
public:
    JuceMidiTransport();
    ~JuceMidiTransport() override;

    MidiTransportOpenResult open(const MidiPortSelection& selection) override;
    MidiTransportSendResult sendCc(const MidiCcMessage& message) override;
    MidiTransportSendResult sendSysEx(const MidiSysExMessage& message) override;
    void close() noexcept override;

    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] MidiPortSelection getActivePortSelection() const noexcept override;

private:
    std::unique_ptr<juce::MidiOutput> midiOutput_;
    MidiPortSelection activePort_;
    uint64_t sequenceCounter_ { 0 };
};

} // namespace abdaudiolab::hardware
