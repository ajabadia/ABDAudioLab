#include "SharedHardwareContractAdapter.h"
#include <juce_core/juce_core.h>

namespace abdaudiolab::core
{

const HardwareContract* SharedHardwareContractAdapter::findContractById(const std::string& id) const noexcept
{
    for (const auto& c : localCache_)
        if (c.id == id) return &c;
    return nullptr;
}

void SharedHardwareContractAdapter::rebuildLocalCache()
{
    localCache_.clear();
    for (const auto& sharedC : sharedRegistry_.getContracts())
    {
        HardwareContract localC;

        // Copy base identity fields 1:1
        localC.schemaVersion = sharedC.schemaVersion;
        localC.id = sharedC.id;
        localC.displayName = sharedC.displayName;
        localC.description = sharedC.description;
        localC.deviceType = sharedC.deviceType;
        localC.brand = sharedC.brand;
        localC.brandLogo = sharedC.brandLogo;
        localC.modelImage = sharedC.modelImage;
        localC.manufacturer = sharedC.manufacturer;
        localC.model = sharedC.model;
        localC.modelIdHex = sharedC.modelIdHex;
        localC.autoDetectSysEx = sharedC.autoDetectSysEx;

        localC.midiIdentity.manufacturer = sharedC.midiIdentity.manufacturer;
        localC.midiIdentity.manufacturerIdHex = sharedC.midiIdentity.manufacturerIdHex;
        localC.midiIdentity.model = sharedC.midiIdentity.model;
        localC.midiIdentity.modelIdHex = sharedC.midiIdentity.modelIdHex;
        localC.midiIdentity.familyIdHex = sharedC.midiIdentity.familyIdHex;
        localC.midiIdentity.sysexHeaderHex = sharedC.midiIdentity.sysexHeaderHex;
        localC.midiIdentity.portNameMatches = sharedC.midiIdentity.portNameMatches;

        // Domain-specific: read from raw JSON
        if (auto rawJson = sharedRegistry_.getRawContractJson(sharedC.id))
        {
            parseFunctionsFromRawJson(*rawJson, localC);
        }

        localCache_.push_back(std::move(localC));
    }
}

void SharedHardwareContractAdapter::parseFunctionsFromRawJson(const nlohmann::json& j, HardwareContract& out)
{
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
                            ctrl.options.push_back(opt.get<std::string>());
                    }
                    f.controls.push_back(std::move(ctrl));
                }
            }
            out.functions.push_back(std::move(f));
        }
    }
    else if (j.contains("parameters") && j["parameters"].is_array())
    {
        // Legacy v1 schema → single default function
        HardwareFunction f;
        f.id = "main_function";
        f.name = out.displayName;
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
            f.controls.push_back(std::move(ctrl));
        }
        out.functions.push_back(std::move(f));
    }
}

} // namespace abdaudiolab::core