#include "HardwareManager.h"

namespace abdaudiolab::core
{

HardwareManager::HardwareManager()
{
    mockController = std::make_unique<hardware::MockHardwareController>();
}

void HardwareManager::initializeHardwareRegistry(const juce::File& contractsDir)
{
    contractRegistry.loadContractsFromDirectory(contractsDir);
    hardwareList.clear();
    for (const auto& contract : contractRegistry.getContracts())
    {
        gui::HardwareItem item;
        item.id = juce::String(contract.id);
        item.displayName = juce::String(contract.displayName);
        item.description = juce::String(contract.description);
        item.brand = juce::String(contract.brand);
        item.brandLogo = juce::String(contract.brandLogo);
        item.modelImage = juce::String(contract.modelImage);

        if (contract.deviceType == "AUTOMATED_SYSEX")
            item.protocol = "AIRA_SYSEX";
        else if (contract.deviceType == "AUTOMATED_MIDI_CC")
            item.protocol = "MIDI_CC";
        else if (contract.deviceType == "MANUAL_EURORACK" || contract.deviceType == "ANALOGUE_PEDAL")
            item.protocol = "MANUAL_ANALOGUE";
        else
            item.protocol = "MOCK_DSP";

        for (const auto& fn : contract.functions)
        {
            gui::FunctionItem fi;
            fi.id = juce::String(fn.id);
            fi.name = juce::String(fn.name);
            fi.blockType = juce::String(fn.blockType);
            fi.captureMode = juce::String(fn.captureMode);
            fi.defaultBurstDurationSec = fn.defaultBurstDurationSec;
            item.functions.push_back(std::move(fi));
        }

        hardwareList.push_back(std::move(item));
    }
}

const gui::HardwareItem* HardwareManager::findHardwareItem(const juce::String& hardwareId) const
{
    for (const auto& item : hardwareList)
    {
        if (item.id == hardwareId) return &item;
    }
    return nullptr;
}

bool HardwareManager::selectHardwareAndFunction(const juce::String& hardwareId, const juce::String& functionId)
{
    currentHardwareId = hardwareId;
    currentFunctionId = functionId;

    const auto* item = findHardwareItem(hardwareId);
    if (item == nullptr) return false;

    if (item->protocol == "AIRA_SYSEX")
    {
        auto airaModel = hardware::AiraSysExController::mapHardwareIdToAiraModel(hardwareId);
        activeController = std::make_unique<hardware::AiraSysExController>(static_cast<hardware::AiraModel>(airaModel));
    }
    else if (item->protocol == "MIDI_CC")
    {
        activeController = std::make_unique<hardware::MidiCcController>();
    }
    else if (item->protocol == "MANUAL_ANALOGUE")
    {
        activeController = std::make_unique<hardware::ManualAnalogueController>();
    }
    else
    {
        activeController = std::make_unique<hardware::MockHardwareController>();
    }

    if (activeController)
    {
        activeController->connect();
    }
    return true;
}

bool HardwareManager::validateConnection(uint8_t sourceId, uint8_t destId, const std::array<uint8_t, 6>& slotSubmoduleTypes, std::string* errorMessage) const
{
    hardware::RoutingValidator validator;
    return validator.validateConnection(sourceId, destId, slotSubmoduleTypes, errorMessage);
}

} // namespace abdaudiolab::core
