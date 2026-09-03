/**
 * @file MidiIdentityDetector.h
 * @brief Automatic MIDI hardware detection via Universal SysEx Identity Inquiry.
 * @details Fully contract-driven detection engine. Decodes hardware according to
 *          specifications declared in JSON hardware contracts without hardcoding.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "../core/HardwareContractRegistry.h"
#include <string>
#include <vector>
#include <optional>

namespace abdaudiolab::hardware
{

/**
 * @struct DiscoveredDevice
 * @brief Metadata for a hardware device identified via MIDI SysEx inquiry.
 */
struct DiscoveredDevice
{
    std::string hardwareId;             /**< Registry ID (from matching contract id). */
    std::string displayName;            /**< Human readable name (from contract displayName). */
    std::string manufacturer;           /**< Manufacturer name from contract. */
    std::string model;                  /**< Model designation from contract. */
    std::string firmwareVersion;        /**< Firmware / software revision string if available. */

    juce::MidiDeviceInfo inDevice;      /**< JUCE MIDI input port info. */
    juce::MidiDeviceInfo outDevice;     /**< JUCE MIDI output port info. */
    uint8_t deviceId { 0x10 };          /**< Device ID or MIDI channel. */
    bool isSysExVerified { false };     /**< True if verified via SysEx response; false if name heuristic. */
};

/**
 * @class MidiIdentityDetector
 * @brief Contract-driven scanner for MIDI ports and SysEx identity messages.
 */
class MidiIdentityDetector : private juce::MidiInputCallback
{
public:
    explicit MidiIdentityDetector(std::vector<core::HardwareContract> contracts = {});
    ~MidiIdentityDetector() override;

    /**
     * @brief Set the active hardware contracts used to recognize devices.
     */
    void setContracts(std::vector<core::HardwareContract> contracts);

    /**
     * @brief Performs a scan across all physical MIDI ports using registered contracts.
     * @param timeoutMs Timeout in milliseconds to wait for SysEx replies per port.
     * @return List of all discovered and identified hardware devices matching contracts.
     */
    std::vector<DiscoveredDevice> scanAllPorts(int timeoutMs = 350);

    /**
     * @brief Parses an incoming MIDI SysEx message against a set of hardware contracts.
     * @param msg Incoming MIDI message.
     * @param outDevice Output structure to populate if matched.
     * @param contracts List of contracts to match against.
     * @return True if message matched a contract.
     */
    static bool parseIdentityReply(const juce::MidiMessage& msg,
                                   DiscoveredDevice& outDevice,
                                   const std::vector<core::HardwareContract>& contracts);

    /**
     * @brief Heuristic name matcher for fallback detection against contract port matches.
     */
    static std::optional<DiscoveredDevice> matchFromPortNames(const juce::MidiDeviceInfo& inDev,
                                                             const juce::MidiDeviceInfo& outDev,
                                                             const std::vector<core::HardwareContract>& contracts);

    /**
     * @brief Build a standard Universal Non-Real Time Identity Request message.
     * @param deviceId Target device ID (0x7F = broadcast, 0x10 = Roland default, 0x00..0x0F = channels 1..16).
     */
    static juce::MidiMessage makeIdentityRequest(uint8_t deviceId = 0x7F);

    /**
     * @brief Helper to convert a whitespace-delimited hex string into a byte vector.
     */
    static std::vector<uint8_t> parseHexBytes(const std::string& hexStr);

private:
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    std::vector<core::HardwareContract> registeredContracts;
    std::vector<DiscoveredDevice> currentScanResults;
    juce::CriticalSection scanLock;
};

} // namespace abdaudiolab::hardware
