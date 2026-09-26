#include "TargetProfileService.h"
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <sstream>
#include <cmath>
#include "../synth/Sha256.h"

namespace abdaudiolab::profiling
{

namespace
{

using json = nlohmann::json;

bool checkAllowedKeys(const json& j,
                      const std::unordered_set<std::string>& allowed,
                      const std::string& parentPointer,
                      std::vector<ValidationDiagnostic>& diagnostics)
{
    bool allValid = true;
    for (auto it = j.begin(); it != j.end(); ++it)
    {
        if (allowed.find(it.key()) == allowed.end())
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = parentPointer + "/" + it.key();
            d.code = "ERR_SCHEMA_UNKNOWN_FIELD";
            d.message = "Propiedad desconocida no permitida por la especificacion: '" + it.key() + "'";
            diagnostics.push_back(d);
            allValid = false;
        }
    }
    return allValid;
}

bool validateSysExTemplate(const std::string& tmpl,
                           const std::string& parentPointer,
                           std::vector<ValidationDiagnostic>& diagnostics)
{
    if (tmpl.empty())
    {
        diagnostics.push_back({DiagnosticSeverity::Error, parentPointer + "/messageTemplate",
            "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "messageTemplate no puede estar vacio."});
        return false;
    }

    std::istringstream iss(tmpl);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
    {
        tokens.push_back(token);
    }

    if (tokens.size() < 2)
    {
        diagnostics.push_back({DiagnosticSeverity::Error, parentPointer + "/messageTemplate",
            "ERR_SYSEX_INVALID_DELIMITERS", "SysEx debe contener al menos delimitadores F0 y F7."});
        return false;
    }

    std::string firstTok = tokens.front();
    std::string lastTok = tokens.back();
    for (char& c : firstTok) c = static_cast<char>(std::toupper(c));
    for (char& c : lastTok) c = static_cast<char>(std::toupper(c));

    if (firstTok != "F0" || lastTok != "F7")
    {
        diagnostics.push_back({DiagnosticSeverity::Error, parentPointer + "/messageTemplate",
            "ERR_SYSEX_INVALID_DELIMITERS", "SysEx debe iniciar estrictamente con F0 y finalizar con F7."});
        return false;
    }

    static const std::unordered_set<std::string> kAllowedSysExTokens = {
        "{deviceid}", "{value7bit}", "{valuenibblemsb}", "{valuenibblelsb}", "{checksum}", "{xx}", "xx"
    };

    for (size_t i = 1; i + 1 < tokens.size(); ++i)
    {
        std::string tok = tokens[i];
        std::string tokLower = tok;
        for (char& c : tokLower) c = static_cast<char>(std::tolower(c));

        if (tok.front() == '{' && tok.back() == '}')
        {
            if (kAllowedSysExTokens.find(tokLower) == kAllowedSysExTokens.end())
            {
                diagnostics.push_back({DiagnosticSeverity::Error, parentPointer + "/messageTemplate",
                    "ERR_SYSEX_UNKNOWN_TOKEN", "Token SysEx desconocido o no permitido: " + tok});
                return false;
            }
        }
        else if (tokLower == "xx")
        {
            // Permitido para retrocompatibilidad
        }
        else
        {
            try
            {
                size_t idx = 0;
                int byteVal = std::stoi(tok, &idx, 16);
                if (idx != tok.size() || byteVal < 0 || byteVal > 0x7F)
                {
                    diagnostics.push_back({DiagnosticSeverity::Error, parentPointer + "/messageTemplate",
                        "ERR_SYSEX_ILLEGAL_STATUS_BYTE", "Byte de carga util SysEx ilegal o fuera de rango [0x00, 0x7F]: " + tok});
                    return false;
                }
            }
            catch (...)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, parentPointer + "/messageTemplate",
                    "ERR_SYSEX_UNKNOWN_TOKEN", "Token SysEx invalido: " + tok});
                return false;
            }
        }
    }

    return true;
}

std::string rfc8785Serialize(const json& j)
{
    if (j.is_object())
    {
        std::string out = "{";
        bool first = true;
        for (auto it = j.begin(); it != j.end(); ++it)
        {
            if (!first) out += ",";
            first = false;
            out += json(it.key()).dump();
            out += ":";
            out += rfc8785Serialize(it.value());
        }
        out += "}";
        return out;
    }
    else if (j.is_array())
    {
        std::string out = "[";
        bool first = true;
        for (const auto& item : j)
        {
            if (!first) out += ",";
            first = false;
            out += rfc8785Serialize(item);
        }
        out += "]";
        return out;
    }
    else if (j.is_number_float())
    {
        double val = j.get<double>();
        if (std::floor(val) == val && std::abs(val) < 1e15)
            return std::to_string(static_cast<int64_t>(val));
        return j.dump();
    }
    return j.dump();
}

} // namespace

TargetProfileLoadResult TargetProfileService::loadAndValidateProfile(const juce::File& file) const
{
    TargetProfileLoadResult res;
    if (!file.existsAsFile())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.code = "ERR_IO_FILE_NOT_FOUND";
        d.message = "El archivo de perfil no existe: " + file.getFullPathName().toStdString();
        res.diagnostics.push_back(d);
        return res;
    }

    std::string content = file.loadFileAsString().toStdString();
    return loadAndValidateProfileJson(content);
}

TargetProfileLoadResult TargetProfileService::loadAndValidateProfileJson(std::string_view jsonString) const
{
    TargetProfileLoadResult res;
    json root;

    try
    {
        root = json::parse(jsonString);
    }
    catch (const json::parse_error& e)
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.code = "ERR_SYNTAX_INVALID_JSON";
        d.message = std::string("Error de sintaxis JSON: ") + e.what();
        res.diagnostics.push_back(d);
        return res;
    }

    if (!root.is_object())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.code = "ERR_SCHEMA_ROOT_NOT_OBJECT";
        d.message = "La raiz del documento de perfil debe ser un objeto JSON.";
        res.diagnostics.push_back(d);
        return res;
    }

    static const std::unordered_set<std::string> kAllowedRoot = {
        "$schema", "schemaVersion", "kind", "targetProfileId", "displayName",
        "vendor", "targetKind", "revision", "identity", "capabilities",
        "parameters", "measurementPolicies", "transportPolicy"
    };

    checkAllowedKeys(root, kAllowedRoot, "", res.diagnostics);

    // 1. schemaVersion
    if (!root.contains("schemaVersion") || !root["schemaVersion"].is_string())
    {
        res.diagnostics.push_back({DiagnosticSeverity::Error, "/schemaVersion", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "Falta schemaVersion."});
    }
    else if (root["schemaVersion"].get<std::string>() != "1.0")
    {
        res.diagnostics.push_back({DiagnosticSeverity::Error, "/schemaVersion", "ERR_SCHEMA_VERSION_UNSUPPORTED", "schemaVersion no soportada."});
    }
    else
    {
        res.profile.schemaVersion = root["schemaVersion"].get<std::string>();
    }

    // 2. kind
    if (!root.contains("kind") || !root["kind"].is_string() || root["kind"].get<std::string>() != "abd.target-profile")
    {
        res.diagnostics.push_back({DiagnosticSeverity::Error, "/kind", "ERR_SCHEMA_KIND_MISMATCH", "kind debe ser 'abd.target-profile'."});
    }
    else
    {
        res.profile.kind = root["kind"].get<std::string>();
    }

    // 3. targetProfileId, displayName, vendor, revision
    if (root.contains("targetProfileId") && root["targetProfileId"].is_string())
        res.profile.targetProfileId = root["targetProfileId"].get<std::string>();
    else
        res.diagnostics.push_back({DiagnosticSeverity::Error, "/targetProfileId", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "targetProfileId requerido."});

    if (root.contains("displayName") && root["displayName"].is_string())
        res.profile.displayName = root["displayName"].get<std::string>();
    else
        res.diagnostics.push_back({DiagnosticSeverity::Error, "/displayName", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "displayName requerido."});

    if (root.contains("vendor") && root["vendor"].is_string())
        res.profile.vendor = root["vendor"].get<std::string>();
    else
        res.diagnostics.push_back({DiagnosticSeverity::Error, "/vendor", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "vendor requerido."});

    if (root.contains("revision") && root["revision"].is_number_integer())
        res.profile.revision = root["revision"].get<int>();

    // 4. targetKind
    if (root.contains("targetKind") && root["targetKind"].is_string())
    {
        std::string tk = root["targetKind"].get<std::string>();
        if (tk == "SyntheticFixture" || tk == "PluginVST3" || tk == "HardwareDigital" || tk == "HardwareAnalogue")
            res.profile.targetKind = tk;
        else
            res.diagnostics.push_back({DiagnosticSeverity::Error, "/targetKind", "ERR_SCHEMA_INVALID_ENUM", "targetKind invalido."});
    }
    else
    {
        res.diagnostics.push_back({DiagnosticSeverity::Error, "/targetKind", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "targetKind requerido."});
    }

    // 5. identity
    if (root.contains("identity") && root["identity"].is_object())
    {
        const auto& idj = root["identity"];
        static const std::unordered_set<std::string> kAllowedId = {
            "canonicalTargetId", "acceptedUniqueIds", "binaryIdentityPolicy", "expectedBinarySha256"
        };
        checkAllowedKeys(idj, kAllowedId, "/identity", res.diagnostics);

        if (idj.contains("canonicalTargetId") && idj["canonicalTargetId"].is_string())
            res.profile.identity.canonicalTargetId = idj["canonicalTargetId"].get<std::string>();
        if (idj.contains("binaryIdentityPolicy") && idj["binaryIdentityPolicy"].is_string())
            res.profile.identity.binaryIdentityPolicy = idj["binaryIdentityPolicy"].get<std::string>();
        if (idj.contains("expectedBinarySha256") && idj["expectedBinarySha256"].is_string())
            res.profile.identity.expectedBinarySha256 = idj["expectedBinarySha256"].get<std::string>();
        if (idj.contains("acceptedUniqueIds") && idj["acceptedUniqueIds"].is_array())
        {
            for (const auto& u : idj["acceptedUniqueIds"])
                if (u.is_string()) res.profile.identity.acceptedUniqueIds.push_back(u.get<std::string>());
        }
    }

    // 6. capabilities
    if (root.contains("capabilities") && root["capabilities"].is_object())
    {
        const auto& capj = root["capabilities"];
        static const std::unordered_set<std::string> kAllowedCap = {
            "midiInput", "supportsParameterAutomation", "controlTransports",
            "audioOutput", "sampleRatesHz", "blockSizes", "supportsPolyphony",
            "midiChannels", "midiNoteRange"
        };
        checkAllowedKeys(capj, kAllowedCap, "/capabilities", res.diagnostics);

        if (capj.contains("midiInput") && capj["midiInput"].is_boolean())
            res.profile.capabilities.midiInput = capj["midiInput"].get<bool>();
        if (capj.contains("supportsParameterAutomation") && capj["supportsParameterAutomation"].is_boolean())
            res.profile.capabilities.supportsParameterAutomation = capj["supportsParameterAutomation"].get<bool>();

        if (capj.contains("controlTransports") && capj["controlTransports"].is_array())
        {
            for (const auto& t : capj["controlTransports"])
            {
                if (!t.is_string()) continue;
                std::string ts = t.get<std::string>();
                if (ts == "InternalParameter") res.profile.capabilities.controlTransports.push_back(ControlTransportKind::InternalParameter);
                else if (ts == "VST3Parameter") res.profile.capabilities.controlTransports.push_back(ControlTransportKind::VST3Parameter);
                else if (ts == "MidiContinuousController") res.profile.capabilities.controlTransports.push_back(ControlTransportKind::MidiContinuousController);
                else if (ts == "MidiSysEx") res.profile.capabilities.controlTransports.push_back(ControlTransportKind::MidiSysEx);
                else if (ts == "ManualOperator") res.profile.capabilities.controlTransports.push_back(ControlTransportKind::ManualOperator);
                else res.diagnostics.push_back({DiagnosticSeverity::Error, "/capabilities/controlTransports", "ERR_SCHEMA_INVALID_ENUM", "Transport no soportado: " + ts});
            }
        }

        if (capj.contains("audioOutput") && capj["audioOutput"].is_object())
        {
            const auto& aout = capj["audioOutput"];
            if (aout.contains("supportedChannelCounts") && aout["supportedChannelCounts"].is_array())
            {
                res.profile.capabilities.audioOutput.supportedChannelCounts.clear();
                for (const auto& c : aout["supportedChannelCounts"])
                    if (c.is_number_integer()) res.profile.capabilities.audioOutput.supportedChannelCounts.push_back(c.get<int>());
            }
            if (aout.contains("requiredChannelCount") && aout["requiredChannelCount"].is_number_integer())
                res.profile.capabilities.audioOutput.requiredChannelCount = aout["requiredChannelCount"].get<int>();
            if (aout.contains("channelLayout") && aout["channelLayout"].is_string())
                res.profile.capabilities.audioOutput.channelLayout = aout["channelLayout"].get<std::string>();
            if (aout.contains("supportedObservationLayouts") && aout["supportedObservationLayouts"].is_array())
            {
                res.profile.capabilities.audioOutput.supportedObservationLayouts.clear();
                for (const auto& l : aout["supportedObservationLayouts"])
                    if (l.is_string()) res.profile.capabilities.audioOutput.supportedObservationLayouts.push_back(l.get<std::string>());
            }
        }

        if (capj.contains("sampleRatesHz") && capj["sampleRatesHz"].is_array())
        {
            res.profile.capabilities.sampleRatesHz.clear();
            for (const auto& sr : capj["sampleRatesHz"])
                if (sr.is_number_integer()) res.profile.capabilities.sampleRatesHz.push_back(sr.get<int>());
        }

        if (capj.contains("blockSizes") && capj["blockSizes"].is_array())
        {
            res.profile.capabilities.blockSizes.clear();
            for (const auto& bs : capj["blockSizes"])
                if (bs.is_number_integer()) res.profile.capabilities.blockSizes.push_back(bs.get<int>());
        }

        if (capj.contains("midiNoteRange") && capj["midiNoteRange"].is_array() && capj["midiNoteRange"].size() == 2)
        {
            int nmin = capj["midiNoteRange"][0].get<int>();
            int nmax = capj["midiNoteRange"][1].get<int>();
            if (nmin > nmax || nmin < 0 || nmax > 127)
            {
                res.diagnostics.push_back({DiagnosticSeverity::Error, "/capabilities/midiNoteRange", "ERR_SEMANTICS_INVALID_RANGE", "midiNoteRange invalido o invertido."});
            }
            else
            {
                res.profile.capabilities.midiNoteRange = { nmin, nmax };
            }
        }
    }

    // 7. parameters
    if (root.contains("parameters") && root["parameters"].is_array())
    {
        std::unordered_set<std::string> seenSemanticIds;
        int pidx = 0;
        for (const auto& p : root["parameters"])
        {
            std::string ptr = "/parameters/" + std::to_string(pidx++);
            if (!p.is_object()) continue;

            static const std::unordered_set<std::string> kAllowedParam = {
                "semanticId", "displayName", "technicalIdentifier", "valueType",
                "normalizedRange", "mappingCurve", "confirmationStatus"
            };
            checkAllowedKeys(p, kAllowedParam, ptr, res.diagnostics);

            TargetParameterMapping mapping;
            if (p.contains("semanticId") && p["semanticId"].is_string())
            {
                mapping.semanticId = p["semanticId"].get<std::string>();
                if (!seenSemanticIds.insert(mapping.semanticId).second)
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/semanticId", "ERR_SEMANTICS_DUPLICATE_ID", "semanticId duplicado: " + mapping.semanticId});
                }
            }

            if (p.contains("displayName") && p["displayName"].is_string())
                mapping.displayName = p["displayName"].get<std::string>();

            if (p.contains("normalizedRange") && p["normalizedRange"].is_array() && p["normalizedRange"].size() == 2)
            {
                double rmin = p["normalizedRange"][0].get<double>();
                double rmax = p["normalizedRange"][1].get<double>();
                if (rmin > rmax || rmin < 0.0 || rmax > 1.0)
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/normalizedRange", "ERR_SEMANTICS_INVALID_RANGE", "normalizedRange fuera de [0, 1] o invertido."});
                }
                else
                {
                    mapping.normalizedRange = { rmin, rmax };
                }
            }

            if (p.contains("confirmationStatus") && p["confirmationStatus"].is_string())
            {
                std::string cs = p["confirmationStatus"].get<std::string>();
                if (cs != "Declared" && cs != "Audited" && cs != "Experimental" && cs != "UserConfirmed")
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/confirmationStatus", "ERR_SCHEMA_INVALID_ENUM", "confirmationStatus desconocido: " + cs});
                }
                else
                {
                    mapping.confirmationStatus = cs;
                }
            }

            if (p.contains("valueType") && p["valueType"].is_string())
                mapping.valueType = p["valueType"].get<std::string>();

            if (p.contains("mappingCurve") && p["mappingCurve"].is_object() && p["mappingCurve"].contains("kind") && p["mappingCurve"]["kind"].is_string())
                mapping.mappingCurve = p["mappingCurve"]["kind"].get<std::string>();

            if (p.contains("technicalIdentifier") && p["technicalIdentifier"].is_object())
            {
                const auto& tid = p["technicalIdentifier"];
                std::string kind = tid.value("kind", "");
                if (kind == "InternalParameter")
                {
                    InternalParameterIdentifier internalId;
                    internalId.parameterKey = tid.value("parameterKey", "");
                    if (internalId.parameterKey.empty())
                        res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/parameterKey", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "parameterKey vacio."});
                    mapping.technicalIdentifier = internalId;
                }
                else if (kind == "VST3Parameter")
                {
                    Vst3ParameterIdentifier vstId;
                    vstId.parameterIndex = tid.value("parameterIndex", -1);
                    vstId.parameterId = tid.value("parameterId", "");
                    if (vstId.parameterIndex < 0)
                        res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/parameterIndex", "ERR_SEMANTICS_INVALID_RANGE", "parameterIndex negativo."});
                    mapping.technicalIdentifier = vstId;
                }
                else if (kind == "MidiContinuousController")
                {
                    static const std::unordered_set<std::string> kAllowedCc = { "kind", "channel", "controllerNumber" };
                    checkAllowedKeys(tid, kAllowedCc, ptr + "/technicalIdentifier", res.diagnostics);

                    MidiCcIdentifier ccId;
                    if (!tid.contains("channel") || !tid["channel"].is_number_integer())
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/channel", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "channel requerido."});
                    }
                    else
                    {
                        ccId.channel = tid["channel"].get<int>();
                        if (ccId.channel < 1 || ccId.channel > 16)
                        {
                            res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/channel", "ERR_SEMANTICS_INVALID_RANGE", "channel debe estar en [1, 16]."});
                        }
                    }

                    if (!tid.contains("controllerNumber") || !tid["controllerNumber"].is_number_integer())
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/controllerNumber", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "controllerNumber requerido."});
                    }
                    else
                    {
                        ccId.controllerNumber = tid["controllerNumber"].get<int>();
                        if (ccId.controllerNumber < 0 || ccId.controllerNumber > 127)
                        {
                            res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/controllerNumber", "ERR_SEMANTICS_INVALID_RANGE", "controllerNumber debe estar en [0, 127]."});
                        }
                    }
                    mapping.technicalIdentifier = ccId;
                }
                else if (kind == "MidiSysEx")
                {
                    static const std::unordered_set<std::string> kAllowedSysEx = {
                        "kind", "messageTemplate", "manufacturerId", "deviceIdPolicy",
                        "valueEncoding", "checksumPolicy", "requiresExplicitConfirmation"
                    };
                    checkAllowedKeys(tid, kAllowedSysEx, ptr + "/technicalIdentifier", res.diagnostics);

                    MidiSysExIdentifier sysexId;
                    if (!tid.contains("messageTemplate") || !tid["messageTemplate"].is_string())
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/messageTemplate", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "messageTemplate requerido."});
                    }
                    else
                    {
                        sysexId.messageTemplate = tid["messageTemplate"].get<std::string>();
                        validateSysExTemplate(sysexId.messageTemplate, ptr + "/technicalIdentifier", res.diagnostics);
                    }

                    if (tid.contains("manufacturerId") && tid["manufacturerId"].is_string())
                        sysexId.manufacturerId = tid["manufacturerId"].get<std::string>();

                    if (tid.contains("deviceIdPolicy") && tid["deviceIdPolicy"].is_string())
                        sysexId.deviceIdPolicy = tid["deviceIdPolicy"].get<std::string>();

                    if (tid.contains("valueEncoding") && tid["valueEncoding"].is_string())
                    {
                        std::string enc = tid["valueEncoding"].get<std::string>();
                        if (enc != "7bit" && enc != "nibble-msb-first" && enc != "nibble-lsb-first")
                        {
                            res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/valueEncoding", "ERR_SCHEMA_INVALID_ENUM", "valueEncoding no valido."});
                        }
                        else
                        {
                            sysexId.valueEncoding = enc;
                        }
                    }

                    if (tid.contains("checksumPolicy") && tid["checksumPolicy"].is_string())
                    {
                        std::string cp = tid["checksumPolicy"].get<std::string>();
                        if (cp != "none" && cp != "yamaha-dx7" && cp != "roland")
                        {
                            res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/checksumPolicy", "ERR_SCHEMA_INVALID_ENUM", "checksumPolicy no valido."});
                        }
                        else
                        {
                            sysexId.checksumPolicy = cp;
                        }
                    }

                    if (tid.contains("requiresExplicitConfirmation") && tid["requiresExplicitConfirmation"].is_boolean())
                        sysexId.requiresExplicitConfirmation = tid["requiresExplicitConfirmation"].get<bool>();

                    mapping.technicalIdentifier = sysexId;
                }
                else if (kind == "ManualOperator")
                {
                    static const std::unordered_set<std::string> kAllowedManual = {
                        "kind", "instructionId", "confirmationPrompt", "controlWidget"
                    };
                    checkAllowedKeys(tid, kAllowedManual, ptr + "/technicalIdentifier", res.diagnostics);

                    ManualOperatorIdentifier manualId;
                    if (!tid.contains("instructionId") || !tid["instructionId"].is_string() || tid["instructionId"].get<std::string>().empty())
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/instructionId", "ERR_SCHEMA_MISSING_REQUIRED_FIELD", "instructionId requerido y no vacio."});
                    }
                    else
                    {
                        manualId.instructionId = tid["instructionId"].get<std::string>();
                    }

                    if (tid.contains("confirmationPrompt") && tid["confirmationPrompt"].is_string())
                        manualId.confirmationPrompt = tid["confirmationPrompt"].get<std::string>();

                    if (tid.contains("controlWidget") && tid["controlWidget"].is_string())
                        manualId.controlWidget = tid["controlWidget"].get<std::string>();

                    mapping.technicalIdentifier = manualId;
                }
                else
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, ptr + "/technicalIdentifier/kind", "ERR_SCHEMA_INVALID_ENUM", "kind de transporte desconocido."});
                }
            }

            res.profile.parameters.push_back(mapping);
        }
    }

    // 8. measurementPolicies
    if (root.contains("measurementPolicies") && root["measurementPolicies"].is_object())
    {
        const auto& mp = root["measurementPolicies"];
        if (mp.contains("warmupTimeMs") && mp["warmupTimeMs"].is_number_integer())
            res.profile.measurementPolicies.warmupTimeMs = mp["warmupTimeMs"].get<int>();
        if (mp.contains("defaultSettlingTimeMs") && mp["defaultSettlingTimeMs"].is_number_integer())
            res.profile.measurementPolicies.defaultSettlingTimeMs = mp["defaultSettlingTimeMs"].get<int>();
        if (mp.contains("recommendedCalibrationPolicy") && mp["recommendedCalibrationPolicy"].is_string())
            res.profile.measurementPolicies.recommendedCalibrationPolicy = mp["recommendedCalibrationPolicy"].get<std::string>();
        if (mp.contains("requiresResetBetweenTrials") && mp["requiresResetBetweenTrials"].is_boolean())
            res.profile.measurementPolicies.requiresResetBetweenTrials = mp["requiresResetBetweenTrials"].get<bool>();
    }

    // 9. transportPolicy (opcional, defaults normativos si no existe)
    if (root.contains("transportPolicy"))
    {
        if (!root["transportPolicy"].is_object())
        {
            res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy",
                "ERR_SCHEMA_TYPE_MISMATCH", "transportPolicy debe ser un objeto."});
        }
        else
        {
            res.profile.hasExplicitTransportPolicy = true;
            const auto& tp = root["transportPolicy"];
            static const std::unordered_set<std::string> kAllowedTransportPolicy = {
                "minimumInterMessageDelayMs", "maximumMessagesPerSecond",
                "requiresResponseAck", "responseTimeoutMs",
                "retryPolicy", "maxRetries",
                "requiresExplicitConfirmation", "allowsBulkDump",
                "requiresVerifiedIdentity", "allowsUserConfirmedUnverifiedIdentity"
            };

            checkAllowedKeys(tp, kAllowedTransportPolicy, "/transportPolicy", res.diagnostics);

            // minimumInterMessageDelayMs: 0..60000 ms
            if (tp.contains("minimumInterMessageDelayMs"))
            {
                if (!tp["minimumInterMessageDelayMs"].is_number_integer())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/minimumInterMessageDelayMs",
                        "ERR_SCHEMA_TYPE_MISMATCH", "minimumInterMessageDelayMs debe ser entero."});
                }
                else
                {
                    int delay = tp["minimumInterMessageDelayMs"].get<int>();
                    if (delay < 0 || delay > 60000)
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/minimumInterMessageDelayMs",
                            "ERR_SEMANTICS_INVALID_RANGE", "minimumInterMessageDelayMs debe estar entre 0 y 60000 ms."});
                    }
                    else
                    {
                        res.profile.transportPolicy.minimumInterMessageDelayMs = delay;
                    }
                }
            }

            // maximumMessagesPerSecond: >= 0 (0 = sin límite explícito)
            if (tp.contains("maximumMessagesPerSecond"))
            {
                if (!tp["maximumMessagesPerSecond"].is_number_integer())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/maximumMessagesPerSecond",
                        "ERR_SCHEMA_TYPE_MISMATCH", "maximumMessagesPerSecond debe ser entero."});
                }
                else
                {
                    int mps = tp["maximumMessagesPerSecond"].get<int>();
                    if (mps < 0)
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/maximumMessagesPerSecond",
                            "ERR_SEMANTICS_INVALID_RANGE", "maximumMessagesPerSecond no puede ser negativo."});
                    }
                    else
                    {
                        res.profile.transportPolicy.maximumMessagesPerSecond = mps;
                    }
                }
            }

            // requiresResponseAck
            if (tp.contains("requiresResponseAck"))
            {
                if (!tp["requiresResponseAck"].is_boolean())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/requiresResponseAck",
                        "ERR_SCHEMA_TYPE_MISMATCH", "requiresResponseAck debe ser booleano."});
                }
                else
                {
                    res.profile.transportPolicy.requiresResponseAck = tp["requiresResponseAck"].get<bool>();
                }
            }

            // responseTimeoutMs: >= 0
            if (tp.contains("responseTimeoutMs"))
            {
                if (!tp["responseTimeoutMs"].is_number_integer())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/responseTimeoutMs",
                        "ERR_SCHEMA_TYPE_MISMATCH", "responseTimeoutMs debe ser entero."});
                }
                else
                {
                    int timeout = tp["responseTimeoutMs"].get<int>();
                    if (timeout < 0)
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/responseTimeoutMs",
                            "ERR_SEMANTICS_INVALID_RANGE", "responseTimeoutMs no puede ser negativo."});
                    }
                    else
                    {
                        res.profile.transportPolicy.responseTimeoutMs = timeout;
                    }
                }
            }

            // retryPolicy: "none", "linear", "exponential"
            if (tp.contains("retryPolicy"))
            {
                if (!tp["retryPolicy"].is_string())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/retryPolicy",
                        "ERR_SCHEMA_TYPE_MISMATCH", "retryPolicy debe ser string."});
                }
                else
                {
                    std::string polStr = tp["retryPolicy"].get<std::string>();
                    if (polStr == "none")
                    {
                        res.profile.transportPolicy.retryPolicy = RetryPolicy::None;
                    }
                    else if (polStr == "linear")
                    {
                        res.profile.transportPolicy.retryPolicy = RetryPolicy::Linear;
                    }
                    else if (polStr == "exponential")
                    {
                        res.profile.transportPolicy.retryPolicy = RetryPolicy::Exponential;
                    }
                    else
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/retryPolicy",
                            "ERR_SCHEMA_UNKNOWN_ENUM", "retryPolicy desconocido: " + polStr});
                    }
                }
            }

            // maxRetries: >= 0
            if (tp.contains("maxRetries"))
            {
                if (!tp["maxRetries"].is_number_integer())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/maxRetries",
                        "ERR_SCHEMA_TYPE_MISMATCH", "maxRetries debe ser entero."});
                }
                else
                {
                    int retries = tp["maxRetries"].get<int>();
                    if (retries < 0)
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/maxRetries",
                            "ERR_SEMANTICS_INVALID_RANGE", "maxRetries no puede ser negativo."});
                    }
                    else
                    {
                        res.profile.transportPolicy.maxRetries = retries;
                    }
                }
            }

            // requiresExplicitConfirmation
            if (tp.contains("requiresExplicitConfirmation"))
            {
                if (!tp["requiresExplicitConfirmation"].is_boolean())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/requiresExplicitConfirmation",
                        "ERR_SCHEMA_TYPE_MISMATCH", "requiresExplicitConfirmation debe ser booleano."});
                }
                else
                {
                    res.profile.transportPolicy.requiresExplicitConfirmation = tp["requiresExplicitConfirmation"].get<bool>();
                }
            }

            // allowsBulkDump
            if (tp.contains("allowsBulkDump"))
            {
                if (!tp["allowsBulkDump"].is_boolean())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/allowsBulkDump",
                        "ERR_SCHEMA_TYPE_MISMATCH", "allowsBulkDump debe ser booleano."});
                }
                else
                {
                    bool bdump = tp["allowsBulkDump"].get<bool>();
                    if (bdump)
                    {
                        res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/allowsBulkDump",
                            "ERR_TRANSPORT_BULK_DUMP_UNSUPPORTED", "allowsBulkDump = true no esta soportado en la version actual."});
                    }
                    res.profile.transportPolicy.allowsBulkDump = bdump;
                }
            }

            // requiresVerifiedIdentity
            if (tp.contains("requiresVerifiedIdentity"))
            {
                if (!tp["requiresVerifiedIdentity"].is_boolean())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/requiresVerifiedIdentity",
                        "ERR_SCHEMA_TYPE_MISMATCH", "requiresVerifiedIdentity debe ser booleano."});
                }
                else
                {
                    res.profile.transportPolicy.requiresVerifiedIdentity = tp["requiresVerifiedIdentity"].get<bool>();
                }
            }

            // allowsUserConfirmedUnverifiedIdentity
            if (tp.contains("allowsUserConfirmedUnverifiedIdentity"))
            {
                if (!tp["allowsUserConfirmedUnverifiedIdentity"].is_boolean())
                {
                    res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/allowsUserConfirmedUnverifiedIdentity",
                        "ERR_SCHEMA_TYPE_MISMATCH", "allowsUserConfirmedUnverifiedIdentity debe ser booleano."});
                }
                else
                {
                    res.profile.transportPolicy.allowsUserConfirmedUnverifiedIdentity = tp["allowsUserConfirmedUnverifiedIdentity"].get<bool>();
                }
            }

            // Reglas de consistencia semantica cruzada:
            // 1. requiresResponseAck == true exige responseTimeoutMs > 0
            if (res.profile.transportPolicy.requiresResponseAck && res.profile.transportPolicy.responseTimeoutMs <= 0)
            {
                res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/responseTimeoutMs",
                    "ERR_SEMANTICS_INVALID_POLICY", "Si requiresResponseAck es true, responseTimeoutMs debe ser mayor que 0."});
            }

            // 2. retryPolicy == None exige maxRetries == 0
            if (res.profile.transportPolicy.retryPolicy == RetryPolicy::None && res.profile.transportPolicy.maxRetries > 0)
            {
                res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/maxRetries",
                    "ERR_SEMANTICS_INVALID_POLICY", "Si retryPolicy es 'none', maxRetries debe ser 0."});
            }

            // 3. requiresResponseAck == false exige retryPolicy == None
            if (!res.profile.transportPolicy.requiresResponseAck && res.profile.transportPolicy.retryPolicy != RetryPolicy::None)
            {
                res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/retryPolicy",
                    "ERR_SEMANTICS_INVALID_POLICY", "Si requiresResponseAck es false, retryPolicy debe ser 'none'."});
            }

            // 4. requiresVerifiedIdentity == true y allowsUserConfirmedUnverifiedIdentity == true son incompatibles
            if (res.profile.transportPolicy.requiresVerifiedIdentity && res.profile.transportPolicy.allowsUserConfirmedUnverifiedIdentity)
            {
                res.diagnostics.push_back({DiagnosticSeverity::Error, "/transportPolicy/allowsUserConfirmedUnverifiedIdentity",
                    "ERR_SEMANTICS_INVALID_POLICY", "Si requiresVerifiedIdentity es true, allowsUserConfirmedUnverifiedIdentity debe ser false."});
            }
        }
    }

    if (res.isSuccess())
        res.canonicalProfileHash = computeCanonicalProfileHash(jsonString);

    return res;
}

std::string TargetProfileService::canonicalizeJsonRfc8785(const std::string& rawJsonString)
{
    json parsed = json::parse(rawJsonString);
    return rfc8785Serialize(parsed);
}

std::string TargetProfileService::computeCanonicalProfileHash(std::string_view jsonString)
{
    std::string canonicalJson = canonicalizeJsonRfc8785(std::string(jsonString));
    return synth::Sha256::computeHex(canonicalJson);
}

const TargetParameterMapping* TargetProfileService::findMapping(const TargetProfile& profile,
                                                               std::string_view semanticId) const noexcept
{
    return profile.findMappingForSemanticId(semanticId);
}

std::string TargetProfileService::serializeProfileToJson(const TargetProfile& profile)
{
    json root;
    root["$schema"] = "https://json-schema.org/draft/2020-12/schema";
    root["schemaVersion"] = profile.schemaVersion;
    root["kind"] = profile.kind;
    root["targetProfileId"] = profile.targetProfileId;
    root["displayName"] = profile.displayName;
    root["vendor"] = profile.vendor;
    root["targetKind"] = profile.targetKind;
    root["revision"] = profile.revision;

    json idj;
    idj["canonicalTargetId"] = profile.identity.canonicalTargetId;
    idj["acceptedUniqueIds"] = profile.identity.acceptedUniqueIds;
    idj["binaryIdentityPolicy"] = profile.identity.binaryIdentityPolicy;
    if (!profile.identity.expectedBinarySha256.empty())
        idj["expectedBinarySha256"] = profile.identity.expectedBinarySha256;
    root["identity"] = idj;

    json capj;
    capj["midiInput"] = profile.capabilities.midiInput;
    capj["supportsParameterAutomation"] = profile.capabilities.supportsParameterAutomation;
    json trans = json::array();
    for (auto t : profile.capabilities.controlTransports)
    {
        switch (t)
        {
            case ControlTransportKind::InternalParameter: trans.push_back("InternalParameter"); break;
            case ControlTransportKind::VST3Parameter: trans.push_back("VST3Parameter"); break;
            case ControlTransportKind::MidiContinuousController: trans.push_back("MidiContinuousController"); break;
            case ControlTransportKind::MidiSysEx: trans.push_back("MidiSysEx"); break;
            case ControlTransportKind::ManualOperator: trans.push_back("ManualOperator"); break;
        }
    }
    capj["controlTransports"] = trans;

    json aout;
    aout["supportedChannelCounts"] = profile.capabilities.audioOutput.supportedChannelCounts;
    aout["requiredChannelCount"] = profile.capabilities.audioOutput.requiredChannelCount;
    aout["channelLayout"] = profile.capabilities.audioOutput.channelLayout;
    aout["supportedObservationLayouts"] = profile.capabilities.audioOutput.supportedObservationLayouts;
    capj["audioOutput"] = aout;

    capj["sampleRatesHz"] = profile.capabilities.sampleRatesHz;
    capj["blockSizes"] = profile.capabilities.blockSizes;
    capj["supportsPolyphony"] = profile.capabilities.supportsPolyphony;
    capj["midiChannels"] = profile.capabilities.midiChannels;
    capj["midiNoteRange"] = { profile.capabilities.midiNoteRange.first, profile.capabilities.midiNoteRange.second };
    root["capabilities"] = capj;

    json params = json::array();
    for (const auto& p : profile.parameters)
    {
        json pj;
        pj["semanticId"] = p.semanticId;
        pj["displayName"] = p.displayName;
        pj["valueType"] = p.valueType;
        pj["normalizedRange"] = { p.normalizedRange.first, p.normalizedRange.second };
        pj["mappingCurve"] = { { "kind", p.mappingCurve } };
        pj["confirmationStatus"] = p.confirmationStatus;

        json tid;
        std::visit([&tid](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, InternalParameterIdentifier>)
            {
                tid["kind"] = "InternalParameter";
                tid["parameterKey"] = arg.parameterKey;
            }
            else if constexpr (std::is_same_v<T, Vst3ParameterIdentifier>)
            {
                tid["kind"] = "VST3Parameter";
                tid["parameterIndex"] = arg.parameterIndex;
                if (!arg.parameterId.empty()) tid["parameterId"] = arg.parameterId;
            }
            else if constexpr (std::is_same_v<T, MidiCcIdentifier>)
            {
                tid["kind"] = "MidiContinuousController";
                tid["channel"] = arg.channel;
                tid["controllerNumber"] = arg.controllerNumber;
            }
            else if constexpr (std::is_same_v<T, MidiSysExIdentifier>)
            {
                tid["kind"] = "MidiSysEx";
                tid["messageTemplate"] = arg.messageTemplate;
                if (!arg.manufacturerId.empty()) tid["manufacturerId"] = arg.manufacturerId;
                if (!arg.deviceIdPolicy.empty()) tid["deviceIdPolicy"] = arg.deviceIdPolicy;
                tid["valueEncoding"] = arg.valueEncoding;
                if (!arg.checksumPolicy.empty() && arg.checksumPolicy != "none") tid["checksumPolicy"] = arg.checksumPolicy;
                if (arg.requiresExplicitConfirmation) tid["requiresExplicitConfirmation"] = true;
            }
            else if constexpr (std::is_same_v<T, ManualOperatorIdentifier>)
            {
                tid["kind"] = "ManualOperator";
                tid["instructionId"] = arg.instructionId;
                if (!arg.confirmationPrompt.empty()) tid["confirmationPrompt"] = arg.confirmationPrompt;
                if (!arg.controlWidget.empty()) tid["controlWidget"] = arg.controlWidget;
            }
        }, p.technicalIdentifier);

        pj["technicalIdentifier"] = tid;
        params.push_back(pj);
    }
    root["parameters"] = params;

    json pol;
    pol["warmupTimeMs"] = profile.measurementPolicies.warmupTimeMs;
    pol["defaultSettlingTimeMs"] = profile.measurementPolicies.defaultSettlingTimeMs;
    pol["recommendedCalibrationPolicy"] = profile.measurementPolicies.recommendedCalibrationPolicy;
    pol["requiresResetBetweenTrials"] = profile.measurementPolicies.requiresResetBetweenTrials;
    root["measurementPolicies"] = pol;

    if (profile.hasExplicitTransportPolicy)
    {
        json tp;
        tp["minimumInterMessageDelayMs"] = profile.transportPolicy.minimumInterMessageDelayMs;
        tp["maximumMessagesPerSecond"] = profile.transportPolicy.maximumMessagesPerSecond;
        tp["requiresResponseAck"] = profile.transportPolicy.requiresResponseAck;
        tp["responseTimeoutMs"] = profile.transportPolicy.responseTimeoutMs;
        tp["retryPolicy"] = (profile.transportPolicy.retryPolicy == RetryPolicy::Linear) ? "linear" :
                            (profile.transportPolicy.retryPolicy == RetryPolicy::Exponential) ? "exponential" : "none";
        tp["maxRetries"] = profile.transportPolicy.maxRetries;
        tp["requiresExplicitConfirmation"] = profile.transportPolicy.requiresExplicitConfirmation;
        tp["allowsBulkDump"] = profile.transportPolicy.allowsBulkDump;
        tp["requiresVerifiedIdentity"] = profile.transportPolicy.requiresVerifiedIdentity;
        tp["allowsUserConfirmedUnverifiedIdentity"] = profile.transportPolicy.allowsUserConfirmedUnverifiedIdentity;
        root["transportPolicy"] = tp;
    }

    return root.dump(2);
}

BinaryAuditResult TargetProfileService::auditBinaryFixity(
    const TargetProfile& profile,
    const std::string& observedBinaryHash) const
{
    BinaryAuditResult result;
    result.policy = profile.identity.binaryIdentityPolicy;
    result.expectedHash = profile.identity.expectedBinarySha256;
    result.observedHash = observedBinaryHash;

    if (result.policy.empty() || result.policy == "not-applicable")
    {
        result.passed = true;
        result.isWarning = false;
        return result;
    }

    if (observedBinaryHash.empty())
    {
        ValidationDiagnostic diag;
        diag.jsonPointer = "/identity/expectedBinarySha256";
        if (result.policy == "warn-on-mismatch")
        {
            diag.severity = DiagnosticSeverity::Warning;
            diag.code = "WARN_TARGET_PROFILE_BINARY_MISSING";
            diag.message = "No se ha proporcionado hash binario del target observado para verificación (política: warn-on-mismatch).";
            result.isWarning = true;
            result.passed = true;
        }
        else
        {
            diag.severity = DiagnosticSeverity::Error;
            diag.code = "ERR_TARGET_PROFILE_BINARY_MISSING";
            diag.message = "El hash binario del target observado es obligatorio bajo la política: " + result.policy;
            result.passed = false;
        }
        result.diagnostics.push_back(diag);
        return result;
    }

    if (!result.expectedHash.empty() && result.expectedHash != observedBinaryHash)
    {
        ValidationDiagnostic diag;
        diag.jsonPointer = "/identity/expectedBinarySha256";

        if (result.policy == "warn-on-mismatch")
        {
            diag.severity = DiagnosticSeverity::Warning;
            diag.code = "WARN_TARGET_PROFILE_BINARY_MISMATCH";
            diag.message = "Hash binario del target (" + observedBinaryHash + ") difiere del esperado (" + result.expectedHash + "). Continuación permitida bajo warn-on-mismatch.";
            result.isWarning = true;
            result.passed = true;
        }
        else if (result.policy == "require-audit-on-change")
        {
            diag.severity = DiagnosticSeverity::Error;
            diag.code = "ERR_TARGET_PROFILE_BINARY_MISMATCH";
            diag.message = "Discrepancia en hash binario: target alterado (" + observedBinaryHash + " vs esperado " + result.expectedHash + "). Requiere re-auditoría obligatoria.";
            result.passed = false;
        }
        else if (result.policy == "strict-bit-exact")
        {
            diag.severity = DiagnosticSeverity::Error;
            diag.code = "ERR_TARGET_PROFILE_BINARY_STRICT_MISMATCH";
            diag.message = "Discrepancia estricta bit a bit en binario del target (" + observedBinaryHash + " vs esperado " + result.expectedHash + "). Rechazo terminante.";
            result.passed = false;
        }
        result.diagnostics.push_back(diag);
    }
    else
    {
        result.passed = true;
        result.isWarning = false;
    }

    return result;
}

TargetProfileDraft TargetProfileService::generateDraftFromContract(
    const synth::TargetContract& contract,
    const std::string& binaryHash) const
{
    TargetProfileDraft draft;
    draft.schemaVersion = "1.0";
    draft.kind = "abd.target-profile-draft";
    draft.draftId = "draft-" + contract.name;
    draft.sourceTargetName = contract.name;
    draft.sourceVendor = contract.manufacturer;
    draft.sourceFormat = contract.format.empty() ? "VST3" : contract.format;
    draft.sourcePluginUid = contract.pluginUid;
    draft.sourceBinaryHash = binaryHash;

    auto toLowerStr = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    };

    draft.parameters.reserve(contract.parameters.size());

    for (size_t i = 0; i < contract.parameters.size(); ++i)
    {
        const auto& p = contract.parameters[i];
        DraftParameterMapping m;
        m.parameterIndex = static_cast<int>(i);
        m.parameterId = p.nativeId;
        m.discoveredName = p.nativeName;
        m.defaultValue = p.defaultValue;
        m.normalizedRange = { p.minValue, p.maxValue };
        m.unit = p.unit;
        m.isAutomatable = p.isAutomatable;
        m.valueType = p.isDiscrete ? "discrete" : "continuous";

        // Heurística conservadora: "Descubrir no es comprender" -> Inferred o Unknown
        std::string lowerName = toLowerStr(p.nativeName);
        std::string lowerId = toLowerStr(p.nativeId);

        if (lowerName.find("cutoff") != std::string::npos || lowerId.find("cutoff") != std::string::npos)
        {
            m.suggestedSemanticId = "filter_cutoff";
            m.semanticStatus = DraftSemanticStatus::Inferred;
            m.inferenceReason = "name_contains_cutoff";
        }
        else if (lowerName.find("resonance") != std::string::npos || lowerName.find("reso") != std::string::npos ||
                 lowerId.find("resonance") != std::string::npos || lowerId.find("reso") != std::string::npos)
        {
            m.suggestedSemanticId = "filter_resonance";
            m.semanticStatus = DraftSemanticStatus::Inferred;
            m.inferenceReason = "name_contains_resonance";
        }
        else if (lowerName.find("master") != std::string::npos || lowerName.find("volume") != std::string::npos ||
                 lowerName.find("output level") != std::string::npos || lowerId.find("output_level") != std::string::npos)
        {
            m.suggestedSemanticId = "master_volume";
            m.semanticStatus = DraftSemanticStatus::Inferred;
            m.inferenceReason = "name_contains_volume";
        }
        else
        {
            m.suggestedSemanticId = "";
            m.semanticStatus = DraftSemanticStatus::Unknown;
            m.inferenceReason = "unrecognized_semantics";
        }

        draft.parameters.push_back(m);
    }

    return draft;
}

TargetProfile TargetProfileService::promoteDraftToProfile(
    const TargetProfileDraft& draft,
    const std::string& targetProfileId,
    const std::string& displayName,
    const std::string& vendor,
    const std::vector<ConfirmedMappingRequest>& confirmedMappings,
    const std::string& binaryIdentityPolicy) const
{
    TargetProfile profile;
    profile.schemaVersion = "1.0";
    profile.kind = "abd.target-profile";
    profile.targetProfileId = targetProfileId.empty() ? draft.draftId : targetProfileId;
    profile.displayName = displayName.empty() ? draft.sourceTargetName : displayName;
    profile.vendor = vendor.empty() ? draft.sourceVendor : vendor;
    profile.targetKind = (draft.sourceFormat == "VST3") ? "PluginVST3" : draft.sourceFormat;
    profile.revision = 1;

    profile.identity.canonicalTargetId = draft.sourcePluginUid.empty() ? targetProfileId : draft.sourcePluginUid;
    profile.identity.acceptedUniqueIds = { profile.identity.canonicalTargetId, draft.sourceTargetName };
    profile.identity.binaryIdentityPolicy = binaryIdentityPolicy;
    profile.identity.expectedBinarySha256 = draft.sourceBinaryHash;

    profile.capabilities.midiInput = true;
    profile.capabilities.supportsParameterAutomation = true;
    profile.capabilities.controlTransports = { ControlTransportKind::VST3Parameter };
    profile.capabilities.audioOutput.supportedChannelCounts = { 2 };
    profile.capabilities.audioOutput.requiredChannelCount = 2;
    profile.capabilities.audioOutput.channelLayout = "stereo";
    profile.capabilities.audioOutput.supportedObservationLayouts = { "stereo" };
    profile.capabilities.sampleRatesHz = { 44100, 48000, 96000 };
    profile.capabilities.blockSizes = { 64, 128, 256, 512, 1024 };
    profile.capabilities.supportsPolyphony = true;
    profile.capabilities.midiChannels = { 1 };
    profile.capabilities.midiNoteRange = { 0, 127 };

    profile.measurementPolicies.warmupTimeMs = 50;
    profile.measurementPolicies.defaultSettlingTimeMs = 50;
    profile.measurementPolicies.recommendedCalibrationPolicy = "None";
    profile.measurementPolicies.requiresResetBetweenTrials = true;

    // Solo mapear los parámetros formalmente confirmados
    for (const auto& req : confirmedMappings)
    {
        TargetParameterMapping m;
        m.semanticId = req.semanticId;
        m.displayName = req.displayName.empty() ? req.semanticId : req.displayName;
        m.valueType = "continuous";
        m.normalizedRange = req.normalizedRange;
        m.mappingCurve = "linear";
        m.confirmationStatus = req.confirmationStatus.empty() ? "UserConfirmed" : req.confirmationStatus;

        Vst3ParameterIdentifier vst;
        vst.parameterIndex = req.parameterIndex;
        vst.parameterId = req.parameterId;
        m.technicalIdentifier = vst;

        profile.parameters.push_back(m);
    }

    return profile;
}

std::vector<uint8_t> TargetProfileService::formatSysExMessage(
    const MidiSysExIdentifier& sysexId,
    uint8_t deviceId,
    double normalizedValue)
{
    std::vector<uint8_t> bytes;
    std::istringstream iss(sysexId.messageTemplate);
    std::string tok;
    std::vector<std::string> tokens;
    while (iss >> tok) tokens.push_back(tok);

    if (tokens.empty()) return bytes;

    int val7bit = std::clamp(static_cast<int>(std::round(normalizedValue * 127.0)), 0, 127);
    uint8_t nibbleMsb = static_cast<uint8_t>((val7bit >> 4) & 0x0F);
    uint8_t nibbleLsb = static_cast<uint8_t>(val7bit & 0x0F);

    size_t checksumIdx = static_cast<size_t>(-1);

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        std::string t = tokens[i];
        std::string tLower = t;
        for (char& c : tLower) c = static_cast<char>(std::tolower(c));

        if (tLower == "f0")
        {
            bytes.push_back(0xF0);
        }
        else if (tLower == "f7")
        {
            bytes.push_back(0xF7);
        }
        else if (tLower == "{deviceid}")
        {
            bytes.push_back(deviceId & 0x7F);
        }
        else if (tLower == "{value7bit}" || tLower == "{xx}" || tLower == "xx")
        {
            bytes.push_back(static_cast<uint8_t>(val7bit));
        }
        else if (tLower == "{valuenibblemsb}")
        {
            bytes.push_back(nibbleMsb);
        }
        else if (tLower == "{valuenibblelsb}")
        {
            bytes.push_back(nibbleLsb);
        }
        else if (tLower == "{checksum}")
        {
            checksumIdx = bytes.size();
            bytes.push_back(0x00); // Placeholder
        }
        else
        {
            try
            {
                int b = std::stoi(t, nullptr, 16);
                bytes.push_back(static_cast<uint8_t>(b & 0xFF));
            }
            catch (...)
            {
                bytes.push_back(0x00);
            }
        }
    }

    if (checksumIdx < bytes.size() && sysexId.checksumPolicy == "yamaha-dx7")
    {
        // Fórmula estándar Yamaha DX7: -sum & 0x7F
        int sum = 0;
        for (size_t i = 4; i < checksumIdx; ++i)
        {
            sum += bytes[i];
        }
        uint8_t cs = static_cast<uint8_t>(((-sum) & 0x7F));
        bytes[checksumIdx] = cs;
    }
    else if (checksumIdx < bytes.size() && sysexId.checksumPolicy == "roland")
    {
        // Roland checksum: (128 - (sum % 128)) & 0x7F
        int sum = 0;
        for (size_t i = 5; i < checksumIdx; ++i)
        {
            sum += bytes[i];
        }
        uint8_t cs = static_cast<uint8_t>((128 - (sum % 128)) & 0x7F);
        bytes[checksumIdx] = cs;
    }

    return bytes;
}

} // namespace abdaudiolab::profiling

