#include "HardwareContractRegistry.h"
#include <fstream>

namespace abdaudiolab::core
{

bool HardwareContractRegistry::loadProfileResilient(const juce::File& jsonFile, HardwareContract& outContract, juce::String& outWarning)
{
    outWarning.clear();

    if (!jsonFile.existsAsFile())
    {
        outWarning = "El archivo '" + jsonFile.getFileName() + "' no existe o no es accesible.";
        return false;
    }

    try
    {
        std::ifstream ifs(jsonFile.getFullPathName().toStdString());
        if (!ifs.is_open())
        {
            outWarning = "No se pudo abrir el archivo '" + jsonFile.getFileName() + "' para lectura.";
            return false;
        }

        nlohmann::json j;
        ifs >> j;

        if (!j.is_object())
        {
            outWarning = "El archivo '" + jsonFile.getFileName() + "' no contiene un objeto JSON raíz válido.";
            return false;
        }

        // Validación Estructural Estricta
        if (!j.contains("id") || !j["id"].is_string() || j["id"].get<std::string>().empty())
        {
            outWarning = "Perfil '" + jsonFile.getFileName() + "' omitido: falta la clave obligatoria 'id' o no es una cadena válida.";
            return false;
        }

        if (!j.contains("displayName") || !j["displayName"].is_string() || j["displayName"].get<std::string>().empty())
        {
            outWarning = "Perfil '" + jsonFile.getFileName() + "' omitido: falta la clave obligatoria 'displayName' o está vacía.";
            return false;
        }

        HardwareContract c;
        c.schemaVersion = j.value("schemaVersion", std::string("2.0"));
        c.id = j["id"].get<std::string>();
        c.displayName = j["displayName"].get<std::string>();
        c.description = j.value("description", std::string(""));
        c.deviceType = j.value("deviceType", j.value("category", std::string("MANUAL_EURORACK")));
        c.brand = j.value("brand", std::string(""));
        c.brandLogo = j.value("brandLogo", std::string(""));
        c.modelImage = j.value("modelImage", std::string(""));
        c.theme = j.value("theme", std::string("audiolab-light"));

        // Parse MIDI Identification
        if (j.contains("midiIdentification") && j["midiIdentification"].is_object())
        {
            const auto& midiObj = j["midiIdentification"];
            c.manufacturer = midiObj.value("manufacturer", c.brand);
            if (c.manufacturer.empty()) c.manufacturer = c.brand;
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

        // Helper lambdas for actions and measurement recipes
        auto parseActions = [](const nlohmann::json& arrJson, std::vector<HardwareSetupAction>& actions) {
            if (!arrJson.is_array()) return;
            for (const auto& aJson : arrJson)
            {
                if (!aJson.is_object()) continue;
                HardwareSetupAction act;
                act.description = aJson.value("description", "");
                std::string methodStr = aJson.value("method", "MIDI_CC");
                if (methodStr == "NRPN") act.method = HardwareMethod::NRPN;
                else if (methodStr == "SYSEX_RAW") act.method = HardwareMethod::SYSEX_RAW;
                else if (methodStr == "MANUAL_PROMPT") act.method = HardwareMethod::MANUAL_PROMPT;
                else act.method = HardwareMethod::MIDI_CC;

                act.channel = aJson.value("channel", 1);
                act.controlNumber = aJson.value("controlNumber", aJson.value("cc", aJson.value("nrpn", -1)));
                act.normalizedValue = aJson.value("normalizedValue", aJson.value("value", 0.0f));
                act.sysexHexPayload = aJson.value("sysexHexPayload", aJson.value("sysexHex", ""));
                act.settlingDelayMs = aJson.value("settlingDelayMs", 50);
                actions.push_back(act);
            }
        };

        auto parseRecipe = [&parseActions](const nlohmann::json& rJson, MeasurementPresetRecipe& recipe) {
            if (!rJson.is_object()) return;
            recipe.recipeType = rJson.value("recipeType", std::string("DIRECT_AUDIO_IN"));
            recipe.description = rJson.value("description", std::string(""));
            recipe.postSettlingDelayMs = rJson.value("postSettlingDelayMs", 100);

            if (rJson.contains("setupActions"))
                parseActions(rJson["setupActions"], recipe.setupActions);

            if (rJson.contains("excitationNotes") && rJson["excitationNotes"].is_array())
            {
                for (const auto& nJson : rJson["excitationNotes"])
                {
                    if (!nJson.is_object()) continue;
                    NoteSequenceEvent ev;
                    ev.noteNumber = nJson.value("noteNumber", nJson.value("note", 60));
                    ev.velocity = nJson.value("velocity", 100);
                    ev.startDelayMs = nJson.value("startDelayMs", 0);
                    ev.durationMs = nJson.value("durationMs", 1000);
                    ev.isLegato = nJson.value("isLegato", false);
                    recipe.excitationNotes.push_back(ev);
                }
            }
        };

        // Parse Lifecycle Contract (Data-Driven Hardware Setup / Teardown)
        if (j.contains("lifecycle") && j["lifecycle"].is_object())
        {
            const auto& lc = j["lifecycle"];
            if (lc.contains("preCalibrationSetup"))
                parseActions(lc["preCalibrationSetup"], c.lifecycle.preCalibrationSetup);
            if (lc.contains("preSessionSetup"))
                parseActions(lc["preSessionSetup"], c.lifecycle.preSessionSetup);
            if (lc.contains("postSessionTeardown"))
                parseActions(lc["postSessionTeardown"], c.lifecycle.postSessionTeardown);
        }

        // Parse Functions (Schema v2)
        if (j.contains("functions") && j["functions"].is_array())
        {
            for (const auto& fJson : j["functions"])
            {
                if (!fJson.is_object()) continue;
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

                if (fJson.contains("measurementRecipe") && fJson["measurementRecipe"].is_object())
                {
                    parseRecipe(fJson["measurementRecipe"], f.measurementRecipe);
                }

                if (fJson.contains("controls") && fJson["controls"].is_array())
                {
                    for (const auto& cJson : fJson["controls"])
                    {
                        if (!cJson.is_object()) continue;
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
                                if (opt.is_string())
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
                if (!pJson.is_object()) continue;
                HardwareControl ctrl;
                ctrl.index = pJson.value("index", static_cast<int>(f.controls.size() + 1));
                ctrl.name = pJson.value("name", std::string("Param"));
                ctrl.type = pJson.value("type", std::string("Knob"));
                ctrl.defaultVal = pJson.value("default", 0.5f);
                f.controls.push_back(ctrl);
            }
            c.functions.push_back(f);
        }

        outContract = std::move(c);
        return true;
    }
    catch (const std::exception& e)
    {
        outWarning = "Error parseando perfil de hardware '" + jsonFile.getFileName() + "': " + juce::String(e.what());
        return false;
    }
}

bool HardwareContractRegistry::loadContractsFromDirectory(const juce::File& contractsDir)
{
    warnings.clear();

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
        HardwareContract contract;
        juce::String warning;
        if (loadProfileResilient(file, contract, warning))
        {
            loadedContracts.push_back(std::move(contract));
        }
        else
        {
            if (warning.isNotEmpty())
            {
                warnings.push_back(warning);
                juce::Logger::writeToLog("[HardwareContractRegistry] " + warning);
                if (onProfileWarning)
                {
                    onProfileWarning(warning);
                }
            }
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
