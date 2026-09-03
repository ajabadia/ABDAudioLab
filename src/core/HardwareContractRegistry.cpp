#include "HardwareContractRegistry.h"
#include <fstream>

namespace abdaudiolab::core
{

bool HardwareContractRegistry::loadContractsFromDirectory(const juce::File& contractsDir)
{
    if (!contractsDir.isDirectory())
    {
        lastErrorMessage = "Contracts directory does not exist: " + contractsDir.getFullPathName().toStdString();
        juce::Logger::writeToLog("[HardwareContractRegistry] " + juce::String(lastErrorMessage));
        return false;
    }

    auto files = contractsDir.findChildFiles(juce::File::findFiles, false, "*.json");
    if (files.isEmpty())
    {
        lastErrorMessage = "No JSON contracts found in directory: " + contractsDir.getFullPathName().toStdString();
        juce::Logger::writeToLog("[HardwareContractRegistry] " + juce::String(lastErrorMessage));
        return false;
    }

    std::vector<HardwareContract> loadedContracts;

    for (const auto& file : files)
    {
        try
        {
            std::ifstream ifs(file.getFullPathName().toStdString());
            if (!ifs.is_open()) continue;

            nlohmann::json j;
            ifs >> j;

            HardwareContract c;
            std::string defaultId = file.getFileNameWithoutExtension().toStdString();
            c.id = j.value("id", defaultId);
            c.displayName = j.value("displayName", std::string("Unnamed Hardware"));
            c.description = j.value("description", std::string(""));
            c.deviceType = j.value("deviceType", j.value("category", std::string("MANUAL_EURORACK")));
            c.brand = j.value("brand", std::string(""));
            c.brandLogo = j.value("brandLogo", std::string(""));
            c.modelImage = j.value("modelImage", std::string(""));

            if (j.contains("midiIdentification") && j["midiIdentification"].is_object())
            {
                const auto& midiObj = j["midiIdentification"];
                c.manufacturer = midiObj.value("manufacturer", std::string(""));
                c.model = midiObj.value("model", std::string(""));
                c.modelIdHex = midiObj.value("modelIdHex", std::string(""));
                c.autoDetectSysEx = midiObj.value("autoDetectSysEx", std::string(""));

                c.midiIdentity.manufacturer = c.manufacturer;
                c.midiIdentity.manufacturerIdHex = midiObj.value("manufacturerIdHex", std::string(""));
                c.midiIdentity.model = c.model;
                c.midiIdentity.modelIdHex = c.modelIdHex;
                c.midiIdentity.familyIdHex = midiObj.value("familyIdHex", std::string(""));
                c.midiIdentity.sysexHeaderHex = midiObj.value("sysexHeaderHex", std::string(""));

                if (midiObj.contains("portNameMatches") && midiObj["portNameMatches"].is_array())
                {
                    for (const auto& item : midiObj["portNameMatches"])
                    {
                        if (item.is_string())
                            c.midiIdentity.portNameMatches.push_back(item.get<std::string>());
                    }
                }
            }

            // Parse Functions (Schema v2)
            if (j.contains("functions") && j["functions"].is_array())
            {
                for (const auto& fJson : j["functions"])
                {
                    HardwareFunction f;
                    f.id = fJson.value("id", std::string("main"));
                    f.name = fJson.value("name", std::string("Main Function"));
                    f.blockType = fJson.value("blockType", std::string("SpectrumFilter"));
                    f.suggestedStimulus = fJson.value("suggestedStimulus", std::string("LOG_SINE_SWEEP"));
                    f.captureMode = fJson.value("captureMode", std::string("FIXED_TIME"));
                    f.defaultBurstDurationSec = fJson.value("defaultBurstDurationSec", 1.0f);
                    f.maxTimeoutSec = fJson.value("maxTimeoutSec", 60.0f);
                    f.silenceThresholdDb = fJson.value("silenceThresholdDb", -60.0f);

                    if (fJson.contains("routingGuide") && fJson["routingGuide"].is_object())
                    {
                        const auto& rg = fJson["routingGuide"];
                        f.routingGuide.stimulusOutput = rg.value("stimulusOutput", std::string(""));
                        f.routingGuide.responseInput = rg.value("responseInput", std::string(""));
                        f.routingGuide.notes = rg.value("notes", std::string(""));
                    }

                    if (fJson.contains("controls") && fJson["controls"].is_array())
                    {
                        for (const auto& cJson : fJson["controls"])
                        {
                            HardwareControl ctrl;
                            ctrl.index = cJson.value("index", static_cast<int>(f.controls.size() + 1));
                            ctrl.name = cJson.value("name", std::string("Control"));
                            ctrl.type = cJson.value("type", std::string("Knob"));
                            ctrl.controlMethod = cJson.value("controlMethod", std::string("MANUAL"));
                            ctrl.ccNumber = cJson.value("cc", cJson.value("midiCC", -1));
                            ctrl.nrpnNumber = cJson.value("nrpn", -1);
                            ctrl.sysexAddress = cJson.value("sysexAddress", std::string(""));
                            ctrl.minVal = cJson.value("min", 0.0f);
                            ctrl.maxVal = cJson.value("max", 1.0f);
                            ctrl.defaultVal = cJson.value("default", 0.5f);
                            ctrl.unit = cJson.value("unit", std::string(""));

                            if (cJson.contains("options") && cJson["options"].is_array())
                            {
                                for (const auto& opt : cJson["options"])
                                {
                                    ctrl.options.push_back(opt.get<std::string>());
                                }
                            }
                            f.controls.push_back(ctrl);
                        }
                    }
                    c.functions.push_back(f);
                }
            }
            // Backward Compatibility: Schema v1 "parameters" -> Single Default Function
            else if (j.contains("parameters") && j["parameters"].is_array())
            {
                HardwareFunction f;
                f.id = "main_function";
                f.name = c.displayName;
                f.blockType = "SpectrumFilter";
                f.suggestedStimulus = "LOG_SINE_SWEEP";
                f.routingGuide.stimulusOutput = "Audio Out 1 (L) -> Hardware Input";
                f.routingGuide.responseInput = "Hardware Output -> Audio In 1 (L)";
                f.routingGuide.notes = "Standard audio loopback routing.";

                for (const auto& pJson : j["parameters"])
                {
                    HardwareControl ctrl;
                    ctrl.index = pJson.value("index", static_cast<int>(f.controls.size() + 1));
                    ctrl.name = pJson.value("name", std::string("Param"));
                    ctrl.type = pJson.value("type", std::string("Knob"));
                    ctrl.defaultVal = pJson.value("default", 0.5f);
                    f.controls.push_back(ctrl);
                }
                c.functions.push_back(f);
            }

            loadedContracts.push_back(c);
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog("Error parsing hardware contract " + file.getFileName() + ": " + e.what());
        }
    }

    if (!loadedContracts.empty())
    {
        contracts = std::move(loadedContracts);
        lastErrorMessage.clear();
        return true;
    }

    lastErrorMessage = "Failed to parse valid contracts from directory: " + contractsDir.getFullPathName().toStdString();
    return false;
}

const HardwareContract* HardwareContractRegistry::findContractById(const std::string& id) const noexcept
{
    for (const auto& c : contracts)
    {
        if (c.id == id)
            return &c;
    }
    return nullptr;
}

} // namespace abdaudiolab::core
