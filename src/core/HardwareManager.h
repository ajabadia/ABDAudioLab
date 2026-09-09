/**
 * @file HardwareManager.h
 * @brief Manages hardware device controllers, Sysex/MIDI attachment, and routing validation.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "SharedHardwareContractAdapter.h"
#include "../gui/drawers/DrawerDataModels.h"
#include "../gui/HardwareSelectorPill.h"
#include "../hardware/HardwareController.h"
#include "../hardware/AiraSysExController.h"
#include "../hardware/MidiCcController.h"
#include "../hardware/ManualAnalogueController.h"
#include "../hardware/MockHardwareController.h"
#include "../hardware/CasioCzVirtualController.h"
#include "../hardware/MidiDeviceHotplugMonitor.h"
#include "../hardware/RoutingValidator.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

namespace abdaudiolab::audio
{
class LabAudioEngine;
}

namespace abdaudiolab::core
{

/**
 * @class HardwareManager
 * @brief Orquesta la creación y vinculación de controladores de hardware (Sysex, CC, Analógico, Mock).
 */
class HardwareManager
{
public:
    HardwareManager();
    ~HardwareManager() = default;

    void initializeHardwareRegistry(const juce::File& contractsDir);

    [[nodiscard]] HardwareContractRegistry& getContractRegistry() noexcept { return contractRegistry; }
    [[nodiscard]] const HardwareContractRegistry& getContractRegistry() const noexcept { return contractRegistry; }
    [[nodiscard]] const HardwareContract* findContractById(const std::string& id) const { return contractRegistry.findContractById(id); }

    [[nodiscard]] const std::vector<gui::HardwareItem>& getHardwareList() const noexcept { return hardwareList; }
    [[nodiscard]] const gui::HardwareItem* findHardwareItem(const juce::String& hardwareId) const;

    [[nodiscard]] hardware::IHardwareController* getActiveController() noexcept;
    [[nodiscard]] hardware::MockHardwareController* getMockController() noexcept { return mockController.get(); }

    bool selectHardwareAndFunction(const juce::String& hardwareId, const juce::String& functionId);
    gui::HardwareConnectionStatus selectHardware(const juce::String& hardwareId,
                                                const juce::String& functionId,
                                                audio::LabAudioEngine& audioEngine);

    [[nodiscard]] juce::String getActiveHardwareId() const noexcept { return currentHardwareId; }
    [[nodiscard]] juce::String getActiveFunctionId() const noexcept { return currentFunctionId; }

    bool validateConnection(uint8_t sourceId, uint8_t destId, const std::array<uint8_t, 6>& slotSubmoduleTypes, std::string* errorMessage = nullptr) const;

    void setManualPromptCallback(hardware::ManualAnalogueController::PromptCallback cb);

    // 1.7.13 Musical MIDI Automation & Note Triggering
    bool sendNoteOn(int channel, int noteNumber, float velocity);
    bool sendNoteOff(int channel, int noteNumber, float velocity = 0.0f);
    bool sendAllNotesOff(int channel = 1);
    bool sendPitchBend(int channel, int pitchWheelValue);
    bool sendChannelPressure(int channel, float pressureValue);
    bool sendMidiControlChange(int channel, int controllerNumber, int controllerValue);
    [[nodiscard]] bool isAutonomousSynth(const juce::String& hardwareId, const juce::String& functionId) const;

    [[nodiscard]] hardware::MidiDeviceHotplugMonitor& getHotplugMonitor() noexcept { return hotplugMonitor; }
    [[nodiscard]] const hardware::MidiDeviceHotplugMonitor& getHotplugMonitor() const noexcept { return hotplugMonitor; }

private:
    std::vector<gui::HardwareItem> hardwareList;
    HardwareContractRegistry contractRegistry;
    hardware::MidiDeviceHotplugMonitor hotplugMonitor;
    juce::String currentHardwareId { "AIRA_S1" };
    juce::String currentFunctionId { "FILTER_RESONANCE_SWEEP" };

    std::unique_ptr<hardware::IHardwareController> activeController;
    std::unique_ptr<hardware::MockHardwareController> mockController;
    hardware::ManualAnalogueController::PromptCallback manualPromptCallback;
};

} // namespace abdaudiolab::core
