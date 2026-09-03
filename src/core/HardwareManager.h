/**
 * @file HardwareManager.h
 * @brief Manages hardware device controllers, Sysex/MIDI attachment, and routing validation.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "HardwareContractRegistry.h"
#include "../gui/SlideInDrawer.h"
#include "../hardware/HardwareController.h"
#include "../hardware/AiraSysExController.h"
#include "../hardware/MidiCcController.h"
#include "../hardware/ManualAnalogueController.h"
#include "../hardware/MockHardwareController.h"
#include "../hardware/RoutingValidator.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

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

    [[nodiscard]] const std::vector<gui::HardwareItem>& getHardwareList() const noexcept { return hardwareList; }
    [[nodiscard]] const gui::HardwareItem* findHardwareItem(const juce::String& hardwareId) const;

    [[nodiscard]] hardware::IHardwareController* getActiveController() noexcept { return activeController.get(); }
    [[nodiscard]] hardware::MockHardwareController* getMockController() noexcept { return mockController.get(); }

    bool selectHardwareAndFunction(const juce::String& hardwareId, const juce::String& functionId);
    [[nodiscard]] juce::String getActiveHardwareId() const noexcept { return currentHardwareId; }
    [[nodiscard]] juce::String getActiveFunctionId() const noexcept { return currentFunctionId; }

    bool validateConnection(uint8_t sourceId, uint8_t destId, const std::array<uint8_t, 6>& slotSubmoduleTypes, std::string* errorMessage = nullptr) const;

private:
    std::vector<gui::HardwareItem> hardwareList;
    HardwareContractRegistry contractRegistry;
    juce::String currentHardwareId { "AIRA_S1" };
    juce::String currentFunctionId { "FILTER_RESONANCE_SWEEP" };

    std::unique_ptr<hardware::IHardwareController> activeController;
    std::unique_ptr<hardware::MockHardwareController> mockController;
};

} // namespace abdaudiolab::core
