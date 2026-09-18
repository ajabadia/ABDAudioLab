#include "HardwareManager.h"
#include "SharedHardwareContractAdapter.h"
#include "../audio/LabAudioEngine.h"

namespace abdaudiolab::core
{

HardwareManager::HardwareManager()
{
    mockController = std::make_unique<hardware::MockHardwareController>();
}

void HardwareManager::initializeHardwareRegistry(const juce::File& contractsDir)
{
    // Use shared registry via adapter to get identity + domain data
    abd::hwid::HardwareContractRegistry sharedRegistry;
    SharedHardwareContractAdapter adapter(sharedRegistry);

    if (!adapter.loadFromShared(contractsDir))
    {
        juce::Logger::writeToLog("[HardwareManager] Failed to load shared contracts: " + juce::String(adapter.getLastError()));
        return;
    }

    hardwareList.clear();
    for (const auto& contract : adapter.getContracts())
    {
        gui::HardwareItem item;
        item.id = juce::String(contract.id);
        item.displayName = juce::String(contract.displayName);
        item.description = juce::String(contract.description);
        item.brand = juce::String(contract.brand);
        item.brandLogo = juce::String(contract.brandLogo);
        item.modelImage = juce::String(contract.modelImage);

        if (contract.deviceType == "VIRTUAL_LOOPBACK_ASIO" || contract.id == "casio_cz101_mame_ves")
            item.protocol = "VIRTUAL_LOOPBACK";
        else if (contract.deviceType == "AUTOMATED_SYSEX")
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

    juce::Logger::writeToLog("[HardwareManager] Loaded " + juce::String(hardwareList.size()) + " hardware profiles from shared contracts.");
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
    else if (item->protocol == "VIRTUAL_LOOPBACK" || item->id.containsIgnoreCase("cz101") || item->id.containsIgnoreCase("casio"))
    {
        activeController = std::make_unique<hardware::CasioCzVirtualController>();
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

hardware::IHardwareController* HardwareManager::getActiveController() noexcept
{
    return activeController ? activeController.get() : mockController.get();
}

gui::HardwareConnectionStatus HardwareManager::selectHardware(const juce::String& hardwareId,
                                                            const juce::String& functionId,
                                                            audio::LabAudioEngine& audioEngine)
{
    currentHardwareId = hardwareId;
    currentFunctionId = functionId;

    const auto* contract = contractRegistry.findContractById(hardwareId.toStdString());
    if (contract == nullptr)
        return gui::HardwareConnectionStatus::NotApplicable;

    gui::HardwareConnectionStatus connStatus = gui::HardwareConnectionStatus::NotApplicable;

    if (contract->deviceType == "MOCK_DSP")
    {
        activeController = nullptr;
        audioEngine.setMockHardware(mockController.get());
        connStatus = gui::HardwareConnectionStatus::NotApplicable;
    }
    else if (contract->deviceType == "VIRTUAL_LOOPBACK_ASIO" || contract->id == "casio_cz101_mame_ves"
             || (contract->brand.find("Casio") != std::string::npos && contract->deviceType == "AUTOMATED_SYSEX"))
    {
        audioEngine.setMockHardware(nullptr);
        auto czCtrl = std::make_unique<hardware::CasioCzVirtualController>();
        czCtrl->setTargetDeviceIdentifier("ABDAudioLab_MIDI_Out");
        bool connected = czCtrl->connect();
        activeController = std::move(czCtrl);
        connStatus = connected ? gui::HardwareConnectionStatus::Connected : gui::HardwareConnectionStatus::Disconnected;
    }
    else if (contract->deviceType == "AUTOMATED_SYSEX")
    {
        audioEngine.setMockHardware(nullptr);
        auto model = hardware::AiraSysExController::mapHardwareIdToAiraModel(contract->id);
        auto airaCtrl = std::make_unique<hardware::AiraSysExController>(static_cast<hardware::AiraModel>(model));
        bool connected = airaCtrl->connect();
        activeController = std::move(airaCtrl);
        connStatus = connected ? gui::HardwareConnectionStatus::Connected : gui::HardwareConnectionStatus::Disconnected;
    }
    else if (contract->deviceType == "AUTOMATED_MIDI_CC")
    {
        audioEngine.setMockHardware(nullptr);
        auto midiCtrl = std::make_unique<hardware::MidiCcController>();

        juce::String keyword = juce::String(contract->displayName);
        if (keyword.containsIgnoreCase("DeepMind")) keyword = "DeepMind";
        else if (keyword.containsIgnoreCase("MS2000")) keyword = "MS2000";
        else if (keyword.containsIgnoreCase("CZ-101") || keyword.containsIgnoreCase("CZ101")) keyword = "CZ";
        else if (keyword.containsIgnoreCase("PRO-800") || keyword.containsIgnoreCase("PRO800")) keyword = "PRO-800";
        else if (keyword.containsIgnoreCase("Bass Station") || keyword.containsIgnoreCase("BassStation")) keyword = "Bass Station";

        midiCtrl->setTargetDeviceIdentifier(keyword);
        bool connected = midiCtrl->connect();
        activeController = std::move(midiCtrl);
        connStatus = connected ? gui::HardwareConnectionStatus::Connected : gui::HardwareConnectionStatus::Disconnected;
    }
    else if (contract->deviceType == "MANUAL_EURORACK" || contract->deviceType == "ANALOGUE_PEDAL")
    {
        audioEngine.setMockHardware(nullptr);
        auto manualCtrl = std::make_unique<hardware::ManualAnalogueController>();
        if (manualPromptCallback)
            manualCtrl->setPromptCallback(manualPromptCallback);
        manualCtrl->connect();
        activeController = std::move(manualCtrl);
        connStatus = gui::HardwareConnectionStatus::NotApplicable;
    }

    return connStatus;
}

void HardwareManager::setManualPromptCallback(hardware::ManualAnalogueController::PromptCallback cb)
{
    manualPromptCallback = std::move(cb);
    if (auto* manual = dynamic_cast<hardware::ManualAnalogueController*>(activeController.get()))
    {
        manual->setPromptCallback(manualPromptCallback);
    }
}

bool HardwareManager::validateConnection(uint8_t sourceId, uint8_t destId, const std::array<uint8_t, 6>& slotSubmoduleTypes, std::string* errorMessage) const
{
    hardware::RoutingValidator validator;
    return validator.validateConnection(sourceId, destId, slotSubmoduleTypes, errorMessage);
}

bool HardwareManager::sendNoteOn(int channel, int noteNumber, float velocity)
{
    auto* ctrl = getActiveController();
    return ctrl != nullptr && ctrl->sendNoteOn(channel, noteNumber, velocity);
}

bool HardwareManager::sendNoteOff(int channel, int noteNumber, float velocity)
{
    auto* ctrl = getActiveController();
    return ctrl != nullptr && ctrl->sendNoteOff(channel, noteNumber, velocity);
}

bool HardwareManager::sendAllNotesOff(int channel)
{
    auto* ctrl = getActiveController();
    return ctrl != nullptr && ctrl->sendAllNotesOff(channel);
}

bool HardwareManager::sendPitchBend(int channel, int pitchWheelValue)
{
    auto* ctrl = getActiveController();
    return ctrl != nullptr && ctrl->sendPitchBend(channel, pitchWheelValue);
}

bool HardwareManager::sendChannelPressure(int channel, float pressureValue)
{
    auto* ctrl = getActiveController();
    return ctrl != nullptr && ctrl->sendChannelPressure(channel, pressureValue);
}

bool HardwareManager::sendMidiControlChange(int channel, int controllerNumber, int controllerValue)
{
    auto* ctrl = getActiveController();
    if (ctrl == nullptr) return false;
    return ctrl->sendMidiMessage(juce::MidiMessage::controllerEvent(juce::jlimit(1, 16, channel),
                                                                    juce::jlimit(0, 127, controllerNumber),
                                                                    juce::jlimit(0, 127, controllerValue)));
}

bool HardwareManager::isAutonomousSynth(const juce::String& hardwareId, const juce::String& functionId) const
{
    const auto* contract = contractRegistry.findContractById(hardwareId.toStdString());
    if (contract == nullptr)
    {
        juce::String hid = hardwareId.toLowerCase();
        return hid.contains("juno") || hid.contains("junio") || hid.contains("deepmind")
            || hid.contains("ms2000") || hid.contains("cz") || hid.contains("pro800")
            || hid.contains("pro-800") || hid.contains("bassstation") || hid.contains("dx7");
    }

    // Devices controlled via MIDI CC / SysEx / Virtual ASIO / Software Plugins that generate audio
    if (contract->deviceType == "AUTOMATED_MIDI_CC" || contract->deviceType == "AUTOMATED_SYSEX"
        || contract->deviceType == "VIRTUAL_LOOPBACK_ASIO" || contract->deviceType == "SOFTWARE_PLUGIN")
    {
        for (const auto& fn : contract->functions)
        {
            if (fn.id == functionId.toStdString() || functionId.isEmpty())
            {
                if (fn.excitationMode == ExcitationMode::MidiNotes
                    || fn.suggestedStimulus == "NOTE_ON_EXCITATION"
                    || fn.suggestedStimulus == "SILENT_CAPTURE" || fn.suggestedStimulus == "GATE_PULSE"
                    || juce::String(fn.routingGuide.stimulusOutput).containsIgnoreCase("MIDI")
                    || juce::String(fn.routingGuide.stimulusOutput).containsIgnoreCase("NONE")
                    || fn.routingGuide.stimulusOutput.empty())
                {
                    return true;
                }
            }
        }
        // General default for MIDI synthesizers: if not an audio effect or filter in
        juce::String name = juce::String(contract->displayName);
        if (name.containsIgnoreCase("Juno") || name.containsIgnoreCase("DeepMind")
            || name.containsIgnoreCase("Prophecy") || name.containsIgnoreCase("MS2000")
            || name.containsIgnoreCase("CZ-101") || name.containsIgnoreCase("PRO-800")
            || name.containsIgnoreCase("Bass Station") || name.containsIgnoreCase("DX7")
            || name.containsIgnoreCase("Dexed"))
        {
            return true;
        }
    }
    return false;
}

} // namespace abdaudiolab::core
