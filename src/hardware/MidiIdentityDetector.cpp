/**
 * @file MidiIdentityDetector.cpp
 * @brief Fully contract-driven implementation of MIDI Universal Identity Inquiry.
 * @author ABDSynths
 * @date 2026
 */

#include "MidiIdentityDetector.h"
#include <sstream>
#include <algorithm>

namespace abdaudiolab::hardware
{

MidiIdentityDetector::MidiIdentityDetector(std::vector<core::HardwareContract> contracts)
    : registeredContracts(std::move(contracts))
{
}

MidiIdentityDetector::~MidiIdentityDetector()
{
}

void MidiIdentityDetector::setContracts(std::vector<core::HardwareContract> contracts)
{
    registeredContracts = std::move(contracts);
}

std::vector<uint8_t> MidiIdentityDetector::parseHexBytes(const std::string& hexStr)
{
    std::vector<uint8_t> bytes;
    std::stringstream ss(hexStr);
    std::string token;
    while (ss >> token)
    {
        if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0)
            token = token.substr(2);

        try
        {
            auto val = std::stoul(token, nullptr, 16);
            bytes.push_back(static_cast<uint8_t>(val & 0xFF));
        }
        catch (...)
        {
        }
    }
    return bytes;
}

juce::MidiMessage MidiIdentityDetector::makeIdentityRequest(uint8_t deviceId)
{
    // Standard MIDI Universal Non-Real Time Identity Request: F0 7E <devId> 06 01 F7
    const uint8_t sysexBytes[] = { 0x7E, deviceId, 0x06, 0x01 };
    return juce::MidiMessage::createSysExMessage(sysexBytes, sizeof(sysexBytes));
}

bool MidiIdentityDetector::parseIdentityReply(const juce::MidiMessage& msg,
                                              DiscoveredDevice& outDevice,
                                              const std::vector<core::HardwareContract>& contracts)
{
    if (!msg.isSysEx() || contracts.empty())
        return false;

    const auto* data = msg.getSysExData();
    const int size = msg.getSysExDataSize();

    if (data == nullptr || size < 4)
        return false;

    int bestScore = 0;
    DiscoveredDevice bestDev;

    for (const auto& c : contracts)
    {
        // 1. Check custom / proprietary SysEx header declared in contract
        if (!c.midiIdentity.sysexHeaderHex.empty())
        {
            auto headerBytes = parseHexBytes(c.midiIdentity.sysexHeaderHex);
            if (!headerBytes.empty() && size >= static_cast<int>(headerBytes.size()))
            {
                bool matches = true;
                for (size_t i = 0; i < headerBytes.size(); ++i)
                {
                    if (data[i] != headerBytes[i])
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches && bestScore < 10)
                {
                    bestScore = 10;
                    bestDev.hardwareId = c.id;
                    bestDev.displayName = c.displayName;
                    bestDev.manufacturer = c.midiIdentity.manufacturer;
                    bestDev.model = c.midiIdentity.model;
                    bestDev.isSysExVerified = true;
                }
            }
        }

        // 2. Check MMA Universal Non-Real Time Identity Reply:
        // data[0] = 0x7E, data[1] = devId, data[2] = 0x06, data[3] = 0x02
        if (data[0] == 0x7E && data[2] == 0x06 && data[3] == 0x02 && size >= 5)
        {
            if (c.midiIdentity.manufacturerIdHex.empty())
                continue;

            auto mfgBytes = parseHexBytes(c.midiIdentity.manufacturerIdHex);
            if (mfgBytes.empty() || size < 4 + static_cast<int>(mfgBytes.size()))
                continue;

            bool mfgMatches = true;
            for (size_t i = 0; i < mfgBytes.size(); ++i)
            {
                if (data[4 + i] != mfgBytes[i])
                {
                    mfgMatches = false;
                    break;
                }
            }
            if (!mfgMatches)
                continue;

            // If contract declares modelIdHex, verify it
            if (!c.midiIdentity.modelIdHex.empty())
            {
                auto modelBytes = parseHexBytes(c.midiIdentity.modelIdHex);
                int modelOffset = 4 + static_cast<int>(mfgBytes.size()) + 2; // after 2 bytes family code

                bool modelMatches = false;
                if (size >= modelOffset + static_cast<int>(modelBytes.size()))
                {
                    modelMatches = true;
                    for (size_t i = 0; i < modelBytes.size(); ++i)
                    {
                        if (data[modelOffset + i] != modelBytes[i])
                        {
                            modelMatches = false;
                            break;
                        }
                    }
                }

                // Also allow matching at family offset if model was placed there (e.g. Roland Juno 32H)
                if (!modelMatches)
                {
                    int familyOffset = 4 + static_cast<int>(mfgBytes.size());
                    if (size >= familyOffset + static_cast<int>(modelBytes.size()))
                    {
                        modelMatches = true;
                        for (size_t i = 0; i < modelBytes.size(); ++i)
                        {
                            if (data[familyOffset + i] != modelBytes[i])
                            {
                                modelMatches = false;
                                break;
                            }
                        }
                    }
                }

                if (modelMatches && bestScore < 10)
                {
                    bestScore = 10;
                    bestDev.deviceId = data[1];
                    bestDev.hardwareId = c.id;
                    bestDev.displayName = c.displayName;
                    bestDev.manufacturer = c.midiIdentity.manufacturer;
                    bestDev.model = c.midiIdentity.model;
                    bestDev.isSysExVerified = true;
                }
            }
            else if (bestScore < 1)
            {
                // No model ID constraint, fallback manufacturer-only match
                bestScore = 1;
                bestDev.deviceId = data[1];
                bestDev.hardwareId = c.id;
                bestDev.displayName = c.displayName;
                bestDev.manufacturer = c.midiIdentity.manufacturer;
                bestDev.model = c.midiIdentity.model;
                bestDev.isSysExVerified = true;
            }
        }
    }

    if (bestScore > 0)
    {
        outDevice = bestDev;
        return true;
    }

    return false;
}

std::optional<DiscoveredDevice> MidiIdentityDetector::matchFromPortNames(const juce::MidiDeviceInfo& inDev,
                                                                        const juce::MidiDeviceInfo& outDev,
                                                                        const std::vector<core::HardwareContract>& contracts)
{
    juce::String combined = inDev.name + " " + outDev.name;
    int bestMatchLength = 0;
    std::optional<DiscoveredDevice> bestDevice;

    for (const auto& c : contracts)
    {
        // 1. Check explicit port name matches declared in contract JSON
        for (const auto& kw : c.midiIdentity.portNameMatches)
        {
            if (kw.empty()) continue;
            if (combined.containsIgnoreCase(juce::String(kw)))
            {
                if (static_cast<int>(kw.length()) > bestMatchLength)
                {
                    bestMatchLength = static_cast<int>(kw.length());
                    DiscoveredDevice dev;
                    dev.inDevice = inDev;
                    dev.outDevice = outDev;
                    dev.hardwareId = c.id;
                    dev.displayName = c.displayName;
                    dev.manufacturer = c.midiIdentity.manufacturer;
                    dev.model = c.midiIdentity.model;
                    dev.isSysExVerified = false;
                    bestDevice = dev;
                }
            }
        }

        // 2. Check model designation if >= 4 characters
        if (c.midiIdentity.model.length() >= 4 && combined.containsIgnoreCase(juce::String(c.midiIdentity.model)))
        {
            if (static_cast<int>(c.midiIdentity.model.length()) > bestMatchLength)
            {
                bestMatchLength = static_cast<int>(c.midiIdentity.model.length());
                DiscoveredDevice dev;
                dev.inDevice = inDev;
                dev.outDevice = outDev;
                dev.hardwareId = c.id;
                dev.displayName = c.displayName;
                dev.manufacturer = c.midiIdentity.manufacturer;
                dev.model = c.midiIdentity.model;
                dev.isSysExVerified = false;
                bestDevice = dev;
            }
        }
    }

    return bestDevice;
}

void MidiIdentityDetector::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    DiscoveredDevice dev;
    if (parseIdentityReply(message, dev, registeredContracts))
    {
        const juce::ScopedLock sl(scanLock);
        currentScanResults.push_back(std::move(dev));
    }
}

std::vector<DiscoveredDevice> MidiIdentityDetector::scanAllPorts(int timeoutMs)
{
    std::vector<DiscoveredDevice> discovered;

    auto midiInputs = juce::MidiInput::getAvailableDevices();
    auto midiOutputs = juce::MidiOutput::getAvailableDevices();

    if (midiOutputs.isEmpty())
        return discovered;

    for (const auto& outDevInfo : midiOutputs)
    {
        juce::MidiDeviceInfo inDevInfo;
        bool hasInput = false;
        for (const auto& inCandidate : midiInputs)
        {
            if (inCandidate.name == outDevInfo.name || inCandidate.identifier == outDevInfo.identifier)
            {
                inDevInfo = inCandidate;
                hasInput = true;
                break;
            }
        }
        if (!hasInput && !midiInputs.isEmpty())
        {
            inDevInfo = midiInputs[0];
            hasInput = true;
        }

        auto outPort = juce::MidiOutput::openDevice(outDevInfo.identifier);
        if (outPort == nullptr) continue;

        std::unique_ptr<juce::MidiInput> inPort;
        if (hasInput)
        {
            inPort = juce::MidiInput::openDevice(inDevInfo.identifier, this);
            if (inPort != nullptr)
            {
                {
                    const juce::ScopedLock sl(scanLock);
                    currentScanResults.clear();
                }
                inPort->start();
            }
        }

        // Send 1: Universal Broadcast Inquiry (7F)
        outPort->sendMessageNow(makeIdentityRequest(0x7F));

        // Send 2: Roland Default Device ID (10)
        outPort->sendMessageNow(makeIdentityRequest(0x10));

        // Send 3: Roland AIRA Specific Data Request (RQ1)
        const uint8_t airaRq1[] = { 0x41, 0x10, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7F };
        outPort->sendMessageNow(juce::MidiMessage::createSysExMessage(airaRq1, sizeof(airaRq1)));

        // Send 4: Yamaha DX7 Parameter / Dump Request
        const uint8_t dx7Req[] = { 0x43, 0x20, 0x09, 0x00, 0x00, 0x00 };
        outPort->sendMessageNow(juce::MidiMessage::createSysExMessage(dx7Req, sizeof(dx7Req)));

        juce::Thread::sleep(std::clamp(timeoutMs, 50, 600));

        bool foundSysEx = false;
        {
            const juce::ScopedLock sl(scanLock);
            if (!currentScanResults.empty())
            {
                for (auto& item : currentScanResults)
                {
                    item.inDevice = inDevInfo;
                    item.outDevice = outDevInfo;
                    discovered.push_back(item);
                }
                foundSysEx = true;
            }
        }

        if (inPort != nullptr)
        {
            inPort->stop();
            inPort.reset();
        }

        if (!foundSysEx && hasInput)
        {
            auto heuristicDev = matchFromPortNames(inDevInfo, outDevInfo, registeredContracts);
            if (heuristicDev.has_value())
            {
                discovered.push_back(heuristicDev.value());
            }
        }
    }

    return discovered;
}

} // namespace abdaudiolab::hardware
