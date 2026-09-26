#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <utility>

namespace abdaudiolab::hardware
{

enum class MidiTransportError
{
    None = 0,
    NotOpen,
    OpenFailed,
    InvalidPortSelection,
    InvalidMessage,
    WriteFailed,
    Disconnected,
    PermissionDenied,
    BackendUnavailable,
    Unsupported
};

struct MidiPortSelection
{
    std::string stableDeviceId;
    std::string displayName;
    std::string expectedTargetProfileId;

    bool operator==(const MidiPortSelection& other) const noexcept
    {
        return stableDeviceId == other.stableDeviceId &&
               displayName == other.displayName &&
               expectedTargetProfileId == other.expectedTargetProfileId;
    }
};

struct MidiCcMessage
{
    uint8_t channel { 1 };          // 1..16
    uint8_t controllerNumber { 0 }; // 0..127
    uint8_t value { 0 };            // 0..127

    std::string targetProfileId;
    std::string semanticId;
    uint64_t sequenceNumber { 0 };

    bool operator==(const MidiCcMessage& other) const noexcept
    {
        return channel == other.channel &&
               controllerNumber == other.controllerNumber &&
               value == other.value &&
               targetProfileId == other.targetProfileId &&
               semanticId == other.semanticId &&
               sequenceNumber == other.sequenceNumber;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return (channel >= 1 && channel <= 16) &&
               (controllerNumber <= 127) &&
               (value <= 127);
    }
};

struct MidiSysExMessage
{
    std::vector<uint8_t> bytes;

    std::string targetProfileId;
    std::string semanticId;
    std::string checksumPolicy;
    uint64_t sequenceNumber { 0 };

    bool operator==(const MidiSysExMessage& other) const noexcept
    {
        return bytes == other.bytes &&
               targetProfileId == other.targetProfileId &&
               semanticId == other.semanticId &&
               checksumPolicy == other.checksumPolicy &&
               sequenceNumber == other.sequenceNumber;
    }

    [[nodiscard]] bool isValidSysExFrame() const noexcept
    {
        if (bytes.size() < 2) return false;
        if (bytes.front() != 0xF0 || bytes.back() != 0xF7) return false;
        for (size_t i = 1; i + 1 < bytes.size(); ++i)
        {
            if (bytes[i] > 0x7F) return false;
        }
        return true;
    }
};

struct MidiTransportOpenResult
{
    bool opened { false };
    MidiTransportError error { MidiTransportError::None };
    std::string diagnosticCode;
    std::string message;

    [[nodiscard]] static MidiTransportOpenResult ok() noexcept
    {
        return { true, MidiTransportError::None, "", "" };
    }

    [[nodiscard]] static MidiTransportOpenResult fail(MidiTransportError err, std::string diagCode, std::string msg)
    {
        return { false, err, std::move(diagCode), std::move(msg) };
    }
};

struct MidiTransportSendResult
{
    bool acceptedForTransmission { false };
    MidiTransportError error { MidiTransportError::None };

    uint64_t sequenceNumber { 0 };
    uint64_t logicalTimestampSamples { 0 };

    std::string diagnosticCode;
    std::string message;

    [[nodiscard]] static MidiTransportSendResult ok(uint64_t seqNum, uint64_t timestamp = 0) noexcept
    {
        return { true, MidiTransportError::None, seqNum, timestamp, "", "" };
    }

    [[nodiscard]] static MidiTransportSendResult fail(MidiTransportError err, std::string diagCode, std::string msg, uint64_t seqNum = 0)
    {
        return { false, err, seqNum, 0, std::move(diagCode), std::move(msg) };
    }
};

} // namespace abdaudiolab::hardware
