/**
 * @file MidiDeviceHotplugMonitor.h
 * @brief Reactive USB MIDI hotplug monitor for ABDAudioLab.
 * @details Re-exports ABDSharedCode::HardwareMidiDetect::HardwareMidiHotplugMonitor
 *          with an adapter for local abdaudiolab::core::HardwareContract.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <HardwareMidiDetect/HardwareMidiHotplugMonitor.h>
#include "../core/HardwareContractRegistry.h"

namespace abdaudiolab::hardware
{

class MidiDeviceHotplugMonitor : public abd::hwid::HardwareMidiHotplugMonitor
{
public:
    using HardwareMidiHotplugMonitor::HardwareMidiHotplugMonitor;

    void setContracts(const std::vector<core::HardwareContract>& contracts)
    {
        localContracts = contracts;
        setContractsFromExternal(contracts);
    }

    void preWarmAsync(std::function<void(const std::vector<abd::hwid::DiscoveredDevice>&)> onComplete = nullptr)
    {
        std::thread([this, onComplete = std::move(onComplete)]() {
            refreshBaseline();
            std::vector<abd::hwid::DiscoveredDevice> discovered;
            auto outs = juce::MidiOutput::getAvailableDevices();
            auto ins = juce::MidiInput::getAvailableDevices();

            std::vector<abd::hwid::HardwareContract> converted;
            for (const auto& c : localContracts)
            {
                abd::hwid::HardwareContract hc;
                hc.id = c.id;
                hc.displayName = c.displayName;
                hc.midiIdentity.portNameMatches = c.midiIdentity.portNameMatches;
                converted.push_back(std::move(hc));
            }

            for (const auto& outDev : outs)
            {
                juce::MidiDeviceInfo inDev;
                for (const auto& candidate : ins)
                {
                    if (candidate.name == outDev.name)
                    {
                        inDev = candidate;
                        break;
                    }
                }
                auto match = abd::hwid::HardwareMidiDetector::matchFromPortNames(inDev, outDev, converted);
                if (match.has_value())
                {
                    discovered.push_back(*match);
                }
            }
            if (onComplete)
            {
                juce::MessageManager::callAsync([onComplete, discovered]() {
                    onComplete(discovered);
                });
            }
        }).detach();
    }

private:
    std::vector<core::HardwareContract> localContracts;
};

using DiscoveredDevice = abd::hwid::DiscoveredDevice;

} // namespace abdaudiolab::hardware
