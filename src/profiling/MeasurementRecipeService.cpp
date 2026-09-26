#include "MeasurementRecipeService.h"
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

std::string rfc8785Serialize(const json& j)
{
    // nlohmann::json utiliza std::map internamente, garantizando ordenación lexicográfica de claves de objetos.
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
        // Formateo canónico para números en punto flotante
        double val = j.get<double>();
        if (std::floor(val) == val && std::abs(val) < 1e15)
        {
            // Entero exacto expresado en float
            return std::to_string(static_cast<int64_t>(val));
        }
        return j.dump();
    }
    return j.dump();
}

} // namespace

std::string MeasurementRecipeService::canonicalizeJsonRfc8785(const std::string& rawJsonString)
{
    try
    {
        json parsed = json::parse(rawJsonString);
        return rfc8785Serialize(parsed);
    }
    catch (...)
    {
        return rawJsonString;
    }
}

std::string MeasurementRecipeService::computeRecipeDocumentHash(std::string_view jsonString)
{
    std::string canonical = canonicalizeJsonRfc8785(std::string(jsonString));
    return synth::Sha256::computeHex(canonical);
}

RecipeLoadResult MeasurementRecipeService::loadAndValidate(const juce::File& recipeFile) const
{
    RecipeLoadResult result;

    if (!recipeFile.existsAsFile())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "";
        d.code = "ERR_IO_FILE_NOT_FOUND";
        d.message = "El archivo de receta no existe: " + recipeFile.getFullPathName().toStdString();
        result.diagnostics.push_back(d);
        return result;
    }

    juce::String content = recipeFile.loadFileAsString();
    return loadAndValidateJson(content.toStdString());
}

RecipeLoadResult MeasurementRecipeService::loadAndValidateJson(std::string_view jsonString) const
{
    RecipeLoadResult result;

    // -----------------------------------------------------------------------
    // CAPA 1: VALIDACIÓN SINTÁCTICA (Syntax Layer)
    // -----------------------------------------------------------------------
    json root;
    try
    {
        root = json::parse(jsonString);
    }
    catch (const json::parse_error& err)
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "";
        d.code = "ERR_SYNTAX_INVALID_JSON";
        d.message = std::string("Error de sintaxis JSON en byte ") + std::to_string(err.byte) + ": " + err.what();
        result.diagnostics.push_back(d);
        return result;
    }

    if (!root.is_object())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "";
        d.code = "ERR_SCHEMA_ROOT_MUST_BE_OBJECT";
        d.message = "La raíz del documento debe ser un objeto JSON.";
        result.diagnostics.push_back(d);
        return result;
    }

    // Calcular hash canónico RFC 8785
    result.recipeDocumentHash = computeRecipeDocumentHash(jsonString);

    // -----------------------------------------------------------------------
    // CAPA 2: ESQUEMA Y PROHIBICIÓN DE CAMPOS DESCONOCIDOS (Schema Layer)
    // -----------------------------------------------------------------------
    static const std::unordered_set<std::string> kAllowedRootKeys = {
        "schemaVersion", "kind", "recipeId", "displayName", "description",
        "assistanceLevel", "revision", "targetConstraints", "excitation",
        "measurement", "evaluationPolicy", "provenance"
    };
    checkAllowedKeys(root, kAllowedRootKeys, "", result.diagnostics);

    // Campos obligatorios de primer nivel
    static const std::vector<std::string> kRequiredRootKeys = {
        "schemaVersion", "kind", "recipeId", "displayName", "assistanceLevel",
        "revision", "targetConstraints", "excitation", "measurement", "evaluationPolicy"
    };

    for (const auto& reqKey : kRequiredRootKeys)
    {
        if (!root.contains(reqKey))
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/" + reqKey;
            d.code = "ERR_SCHEMA_MISSING_REQUIRED_FIELD";
            d.message = "Campo requerido ausente: '" + reqKey + "'";
            result.diagnostics.push_back(d);
        }
    }

    if (result.hasErrors())
    {
        return result;
    }

    // -----------------------------------------------------------------------
    // CAPA 3: TIPOS Y ENUMS PRIMITIVOS
    // -----------------------------------------------------------------------
    if (!root["schemaVersion"].is_string() || root["schemaVersion"].get<std::string>() != "1.0")
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/schemaVersion";
        d.code = "ERR_SCHEMA_UNSUPPORTED_VERSION";
        d.message = "Version de esquema no soportada. Se requiere exactamente '1.0'.";
        result.diagnostics.push_back(d);
    }

    if (!root["kind"].is_string() || root["kind"].get<std::string>() != "abd.measurement-recipe")
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/kind";
        d.code = "ERR_SCHEMA_INVALID_KIND";
        d.message = "Tipo de documento invalido. Se requiere 'abd.measurement-recipe'.";
        result.diagnostics.push_back(d);
    }

    if (!root["recipeId"].is_string() || root["recipeId"].get<std::string>().empty())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/recipeId";
        d.code = "ERR_SCHEMA_INVALID_RECIPE_ID";
        d.message = "recipeId debe ser una cadena no vacia.";
        result.diagnostics.push_back(d);
    }

    if (!root["displayName"].is_string() || root["displayName"].get<std::string>().empty())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/displayName";
        d.code = "ERR_SCHEMA_INVALID_DISPLAY_NAME";
        d.message = "displayName debe ser una cadena no vacia.";
        result.diagnostics.push_back(d);
    }

    std::string assistanceLevelStr = root["assistanceLevel"].is_string() ? root["assistanceLevel"].get<std::string>() : "";
    auto optLevel = assistanceLevelFromString(assistanceLevelStr);
    if (!optLevel.has_value())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/assistanceLevel";
        d.code = "ERR_SCHEMA_INVALID_ASSISTANCE_LEVEL";
        d.message = "assistanceLevel debe ser 'Quick', 'Configurable' o 'Advanced'.";
        result.diagnostics.push_back(d);
    }

    if (!root["revision"].is_number_integer() || root["revision"].get<int>() < 1)
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/revision";
        d.code = "ERR_SCHEMA_INVALID_REVISION";
        d.message = "revision debe ser un entero >= 1.";
        result.diagnostics.push_back(d);
    }

    // -----------------------------------------------------------------------
    // CAPA 4: TARGET CONSTRAINTS
    // -----------------------------------------------------------------------
    const auto& tcJson = root["targetConstraints"];
    if (!tcJson.is_object())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/targetConstraints";
        d.code = "ERR_SCHEMA_INVALID_TYPE";
        d.message = "targetConstraints debe ser un objeto.";
        result.diagnostics.push_back(d);
    }
    else
    {
        static const std::unordered_set<std::string> kAllowedTcKeys = {
            "targetKinds", "requiredCapabilities", "allowedSampleRatesHz", "channels"
        };
        checkAllowedKeys(tcJson, kAllowedTcKeys, "/targetConstraints", result.diagnostics);

        if (!tcJson.contains("targetKinds") || !tcJson["targetKinds"].is_array() || tcJson["targetKinds"].empty())
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/targetConstraints/targetKinds";
            d.code = "ERR_SCHEMA_EMPTY_TARGET_KINDS";
            d.message = "targetKinds debe ser un array no vacio.";
            result.diagnostics.push_back(d);
        }

        if (!tcJson.contains("allowedSampleRatesHz") || !tcJson["allowedSampleRatesHz"].is_array() || tcJson["allowedSampleRatesHz"].empty())
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/targetConstraints/allowedSampleRatesHz";
            d.code = "ERR_SCHEMA_EMPTY_SAMPLE_RATES";
            d.message = "allowedSampleRatesHz debe ser un array no vacio.";
            result.diagnostics.push_back(d);
        }
        else
        {
            for (size_t i = 0; i < tcJson["allowedSampleRatesHz"].size(); ++i)
            {
                int sr = tcJson["allowedSampleRatesHz"][i].is_number_integer() ? tcJson["allowedSampleRatesHz"][i].get<int>() : 0;
                if (sr != 44100 && sr != 48000 && sr != 88200 && sr != 96000 && sr != 192000)
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = "/targetConstraints/allowedSampleRatesHz/" + std::to_string(i);
                    d.code = "ERR_SEMANTICS_UNSUPPORTED_SAMPLE_RATE";
                    d.message = "Sample rate no admitido: " + std::to_string(sr);
                    result.diagnostics.push_back(d);
                }
            }
        }

        if (!tcJson.contains("channels") || !tcJson["channels"].is_number_integer())
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/targetConstraints/channels";
            d.code = "ERR_SCHEMA_INVALID_TYPE";
            d.message = "channels debe ser un entero (1 o 2).";
            result.diagnostics.push_back(d);
        }
        else
        {
            int ch = tcJson["channels"].get<int>();
            if (ch < 1 || ch > 2)
            {
                ValidationDiagnostic d;
                d.severity = DiagnosticSeverity::Error;
                d.jsonPointer = "/targetConstraints/channels";
                d.code = "ERR_SEMANTICS_INVALID_CHANNEL_COUNT";
                d.message = "channels debe ser 1 (Mono) o 2 (Stereo).";
                result.diagnostics.push_back(d);
            }
        }
    }

    // -----------------------------------------------------------------------
    // CAPA 5: EXCITATION Y VALIDACIÓN SEMÁNTICA (Notes, Gate, Repetitions)
    // -----------------------------------------------------------------------
    const auto& excJson = root["excitation"];
    if (!excJson.is_object())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/excitation";
        d.code = "ERR_SCHEMA_INVALID_TYPE";
        d.message = "excitation debe ser un objeto.";
        result.diagnostics.push_back(d);
    }
    else
    {
        static const std::unordered_set<std::string> kAllowedExcKeys = {
            "notes", "repetitions", "seed"
        };
        checkAllowedKeys(excJson, kAllowedExcKeys, "/excitation", result.diagnostics);

        if (!excJson.contains("repetitions") || !excJson["repetitions"].is_number_integer())
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/excitation/repetitions";
            d.code = "ERR_SCHEMA_INVALID_TYPE";
            d.message = "repetitions debe ser un entero.";
            result.diagnostics.push_back(d);
        }
        else
        {
            int reps = excJson["repetitions"].get<int>();
            if (reps < 1 || reps > 32)
            {
                ValidationDiagnostic d;
                d.severity = DiagnosticSeverity::Error;
                d.jsonPointer = "/excitation/repetitions";
                d.code = "ERR_SEMANTICS_INVALID_RANGE";
                d.message = "repetitions debe estar en el rango [1, 32].";
                result.diagnostics.push_back(d);
            }
        }

        if (!excJson.contains("notes") || !excJson["notes"].is_array() || excJson["notes"].empty())
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/excitation/notes";
            d.code = "ERR_SCHEMA_EMPTY_NOTES";
            d.message = "notes debe ser un array no vacio.";
            result.diagnostics.push_back(d);
        }
        else
        {
            static const std::unordered_set<std::string> kAllowedNoteKeys = {
                "midiNote", "velocity", "gateMs", "settlingMs"
            };

            for (size_t i = 0; i < excJson["notes"].size(); ++i)
            {
                std::string notePtr = "/excitation/notes/" + std::to_string(i);
                const auto& nJson = excJson["notes"][i];
                if (!nJson.is_object())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = notePtr;
                    d.code = "ERR_SCHEMA_INVALID_TYPE";
                    d.message = "Cada nota de excitacion debe ser un objeto.";
                    result.diagnostics.push_back(d);
                    continue;
                }

                checkAllowedKeys(nJson, kAllowedNoteKeys, notePtr, result.diagnostics);

                // midiNote
                if (!nJson.contains("midiNote") || !nJson["midiNote"].is_number_integer())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = notePtr + "/midiNote";
                    d.code = "ERR_SCHEMA_MISSING_REQUIRED_FIELD";
                    d.message = "midiNote es obligatorio y debe ser entero.";
                    result.diagnostics.push_back(d);
                }
                else
                {
                    int note = nJson["midiNote"].get<int>();
                    if (note < 0 || note > 127)
                    {
                        ValidationDiagnostic d;
                        d.severity = DiagnosticSeverity::Error;
                        d.jsonPointer = notePtr + "/midiNote";
                        d.code = "ERR_SEMANTICS_INVALID_RANGE";
                        d.message = "midiNote debe estar en el rango [0, 127].";
                        result.diagnostics.push_back(d);
                    }
                }

                // velocity
                if (!nJson.contains("velocity") || !nJson["velocity"].is_number())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = notePtr + "/velocity";
                    d.code = "ERR_SCHEMA_MISSING_REQUIRED_FIELD";
                    d.message = "velocity es obligatorio y debe ser numerico.";
                    result.diagnostics.push_back(d);
                }
                else
                {
                    double vel = nJson["velocity"].get<double>();
                    if (vel < 0.0 || vel > 1.0)
                    {
                        ValidationDiagnostic d;
                        d.severity = DiagnosticSeverity::Error;
                        d.jsonPointer = notePtr + "/velocity";
                        d.code = "ERR_SEMANTICS_INVALID_RANGE";
                        d.message = "velocity debe estar en el rango [0.0, 1.0].";
                        result.diagnostics.push_back(d);
                    }
                }

                // gateMs
                if (!nJson.contains("gateMs") || !nJson["gateMs"].is_number())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = notePtr + "/gateMs";
                    d.code = "ERR_SCHEMA_MISSING_REQUIRED_FIELD";
                    d.message = "gateMs es obligatorio y debe ser numerico.";
                    result.diagnostics.push_back(d);
                }
                else
                {
                    double gate = nJson["gateMs"].get<double>();
                    if (gate <= 0.0)
                    {
                        ValidationDiagnostic d;
                        d.severity = DiagnosticSeverity::Error;
                        d.jsonPointer = notePtr + "/gateMs";
                        d.code = "ERR_SEMANTICS_INVALID_RANGE";
                        d.message = "gateMs debe ser estrictamente mayor que 0.";
                        result.diagnostics.push_back(d);
                    }
                }

                // settlingMs
                if (!nJson.contains("settlingMs") || !nJson["settlingMs"].is_number())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = notePtr + "/settlingMs";
                    d.code = "ERR_SCHEMA_MISSING_REQUIRED_FIELD";
                    d.message = "settlingMs es obligatorio y debe ser numerico.";
                    result.diagnostics.push_back(d);
                }
                else
                {
                    double settling = nJson["settlingMs"].get<double>();
                    if (settling < 0.0)
                    {
                        ValidationDiagnostic d;
                        d.severity = DiagnosticSeverity::Error;
                        d.jsonPointer = notePtr + "/settlingMs";
                        d.code = "ERR_SEMANTICS_INVALID_RANGE";
                        d.message = "settlingMs debe ser mayor o igual que 0.";
                        result.diagnostics.push_back(d);
                    }
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // CAPA 6: MEASUREMENT (Points, Calibration & Coherence)
    // -----------------------------------------------------------------------
    const auto& measJson = root["measurement"];
    if (!measJson.is_object())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/measurement";
        d.code = "ERR_SCHEMA_INVALID_TYPE";
        d.message = "measurement debe ser un objeto.";
        result.diagnostics.push_back(d);
    }
    else
    {
        static const std::unordered_set<std::string> kAllowedMeasKeys = {
            "points", "calibrationPolicy", "analysisPolicy"
        };
        checkAllowedKeys(measJson, kAllowedMeasKeys, "/measurement", result.diagnostics);

        if (!measJson.contains("points") || !measJson["points"].is_array() || measJson["points"].empty())
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/measurement/points";
            d.code = "ERR_SCHEMA_EMPTY_POINTS";
            d.message = "points debe ser un array no vacio.";
            result.diagnostics.push_back(d);
        }
        else
        {
            static const std::unordered_set<std::string> kAllowedPointKeys = {
                "parameter", "normalizedValue"
            };

            std::unordered_set<std::string> seenPointKeys;

            for (size_t i = 0; i < measJson["points"].size(); ++i)
            {
                std::string ptPtr = "/measurement/points/" + std::to_string(i);
                const auto& pJson = measJson["points"][i];
                if (!pJson.is_object())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = ptPtr;
                    d.code = "ERR_SCHEMA_INVALID_TYPE";
                    d.message = "Cada punto de medicion debe ser un objeto.";
                    result.diagnostics.push_back(d);
                    continue;
                }

                checkAllowedKeys(pJson, kAllowedPointKeys, ptPtr, result.diagnostics);

                std::string paramName;
                if (!pJson.contains("parameter") || !pJson["parameter"].is_string() || pJson["parameter"].get<std::string>().empty())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = ptPtr + "/parameter";
                    d.code = "ERR_SCHEMA_MISSING_REQUIRED_FIELD";
                    d.message = "parameter es obligatorio y debe ser una cadena no vacia.";
                    result.diagnostics.push_back(d);
                }
                else
                {
                    paramName = pJson["parameter"].get<std::string>();
                }

                double normVal = 0.0;
                if (!pJson.contains("normalizedValue") || !pJson["normalizedValue"].is_number())
                {
                    ValidationDiagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.jsonPointer = ptPtr + "/normalizedValue";
                    d.code = "ERR_SCHEMA_MISSING_REQUIRED_FIELD";
                    d.message = "normalizedValue es obligatorio y debe ser numerico.";
                    result.diagnostics.push_back(d);
                }
                else
                {
                    normVal = pJson["normalizedValue"].get<double>();
                    if (normVal < 0.0 || normVal > 1.0)
                    {
                        ValidationDiagnostic d;
                        d.severity = DiagnosticSeverity::Error;
                        d.jsonPointer = ptPtr + "/normalizedValue";
                        d.code = "ERR_SEMANTICS_INVALID_RANGE";
                        d.message = "normalizedValue debe estar en el rango [0.0, 1.0].";
                        result.diagnostics.push_back(d);
                    }
                }

                // Detección de duplicado lógico
                if (!paramName.empty())
                {
                    std::string key = paramName + "@" + std::to_string(normVal);
                    if (seenPointKeys.find(key) != seenPointKeys.end())
                    {
                        ValidationDiagnostic d;
                        d.severity = DiagnosticSeverity::Error;
                        d.jsonPointer = ptPtr;
                        d.code = "ERR_COHERENCE_DUPLICATE_POINT";
                        d.message = "Punto de medicion duplicado para parametro '" + paramName + "' con valor " + std::to_string(normVal);
                        result.diagnostics.push_back(d);
                    }
                    seenPointKeys.insert(key);
                }
            }
        }

        std::string calPol = measJson.value("calibrationPolicy", "Required");
        if (calPol != "Required" && calPol != "Optional" && calPol != "None")
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/measurement/calibrationPolicy";
            d.code = "ERR_SCHEMA_INVALID_ENUM";
            d.message = "calibrationPolicy debe ser 'Required', 'Optional' o 'None'.";
            result.diagnostics.push_back(d);
        }

        std::string anaPol = measJson.value("analysisPolicy", "CanonicalV1");
        if (anaPol != "CanonicalV1" && anaPol != "HarmonicFull" && anaPol != "LinearTransferOnly")
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/measurement/analysisPolicy";
            d.code = "ERR_SCHEMA_INVALID_ENUM";
            d.message = "analysisPolicy debe ser 'CanonicalV1', 'HarmonicFull' o 'LinearTransferOnly'.";
            result.diagnostics.push_back(d);
        }
    }

    // -----------------------------------------------------------------------
    // CAPA 7: EVALUATION POLICY
    // -----------------------------------------------------------------------
    const auto& evalJson = root["evaluationPolicy"];
    if (!evalJson.is_object())
    {
        ValidationDiagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.jsonPointer = "/evaluationPolicy";
        d.code = "ERR_SCHEMA_INVALID_TYPE";
        d.message = "evaluationPolicy debe ser un objeto.";
        result.diagnostics.push_back(d);
    }
    else
    {
        static const std::unordered_set<std::string> kAllowedEvalKeys = {
            "minimumSnrDb", "maximumThdPercent", "f0ToleranceCents"
        };
        checkAllowedKeys(evalJson, kAllowedEvalKeys, "/evaluationPolicy", result.diagnostics);

        if (!evalJson.contains("minimumSnrDb") || !evalJson["minimumSnrDb"].is_number() || evalJson["minimumSnrDb"].get<double>() < 0.0)
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/evaluationPolicy/minimumSnrDb";
            d.code = "ERR_SEMANTICS_INVALID_RANGE";
            d.message = "minimumSnrDb debe ser un numero >= 0.";
            result.diagnostics.push_back(d);
        }

        if (!evalJson.contains("maximumThdPercent") || !evalJson["maximumThdPercent"].is_number() || evalJson["maximumThdPercent"].get<double>() < 0.0)
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/evaluationPolicy/maximumThdPercent";
            d.code = "ERR_SEMANTICS_INVALID_RANGE";
            d.message = "maximumThdPercent debe ser un numero >= 0.";
            result.diagnostics.push_back(d);
        }

        if (!evalJson.contains("f0ToleranceCents") || !evalJson["f0ToleranceCents"].is_number() || evalJson["f0ToleranceCents"].get<double>() < 0.0)
        {
            ValidationDiagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.jsonPointer = "/evaluationPolicy/f0ToleranceCents";
            d.code = "ERR_SEMANTICS_INVALID_RANGE";
            d.message = "f0ToleranceCents debe ser un numero >= 0.";
            result.diagnostics.push_back(d);
        }
    }

    // -----------------------------------------------------------------------
    // POBLAR MODELO FUERTEMENTE TIPADO SI NO HAY ERRORES FATALES
    // -----------------------------------------------------------------------
    if (result.isSuccess())
    {
        result.recipe.schemaVersion = root["schemaVersion"].get<std::string>();
        result.recipe.kind = root["kind"].get<std::string>();
        result.recipe.recipeId = root["recipeId"].get<std::string>();
        result.recipe.displayName = root["displayName"].get<std::string>();
        result.recipe.description = root.value("description", "");
        result.recipe.assistanceLevel = optLevel.value();
        result.recipe.revision = root["revision"].get<int>();

        // Target constraints
        for (const auto& item : tcJson["targetKinds"])
            result.recipe.targetConstraints.targetKinds.push_back(item.get<std::string>());
        if (tcJson.contains("requiredCapabilities") && tcJson["requiredCapabilities"].is_array())
        {
            for (const auto& item : tcJson["requiredCapabilities"])
                result.recipe.targetConstraints.requiredCapabilities.push_back(item.get<std::string>());
        }
        for (const auto& item : tcJson["allowedSampleRatesHz"])
            result.recipe.targetConstraints.allowedSampleRatesHz.push_back(item.get<int>());
        result.recipe.targetConstraints.channels = tcJson["channels"].get<int>();

        // Excitation
        for (const auto& nItem : excJson["notes"])
        {
            NoteExcitationConfig n;
            n.midiNote = nItem["midiNote"].get<int>();
            n.velocity = nItem["velocity"].get<double>();
            n.gateMs = nItem["gateMs"].get<double>();
            n.settlingMs = nItem["settlingMs"].get<double>();
            result.recipe.excitation.notes.push_back(n);
        }
        result.recipe.excitation.repetitions = excJson["repetitions"].get<int>();
        if (excJson.contains("seed") && excJson["seed"].is_number_integer())
            result.recipe.excitation.seed = excJson["seed"].get<int>();

        // Measurement
        for (const auto& pItem : measJson["points"])
        {
            MeasurementPointConfig p;
            p.parameter = pItem["parameter"].get<std::string>();
            p.semanticId = p.parameter;
            p.normalizedValue = pItem["normalizedValue"].get<double>();
            result.recipe.measurement.points.push_back(p);
        }
        result.recipe.measurement.calibrationPolicy = measJson.value("calibrationPolicy", "Required");
        result.recipe.measurement.analysisPolicy = measJson.value("analysisPolicy", "CanonicalV1");

        // Evaluation
        result.recipe.evaluationPolicy.minimumSnrDb = evalJson["minimumSnrDb"].get<double>();
        result.recipe.evaluationPolicy.maximumThdPercent = evalJson["maximumThdPercent"].get<double>();
        result.recipe.evaluationPolicy.f0ToleranceCents = evalJson["f0ToleranceCents"].get<double>();

        // Provenance (opcional)
        if (root.contains("provenance") && root["provenance"].is_object())
        {
            const auto& prov = root["provenance"];
            static const std::unordered_set<std::string> kAllowedProvKeys = {
                "authoringSource", "documentationRef", "migratedFromSchemaVersion",
                "migrationToolVersion", "sourceRecipeDocumentHash",
                "sourceKind", "sourceExplorationId", "sourceExplorationHash", "promotionToolVersion"
            };
            checkAllowedKeys(prov, kAllowedProvKeys, "/provenance", result.diagnostics);

            result.recipe.provenance.authoringSource = prov.value("authoringSource", "builtin");
            result.recipe.provenance.documentationRef = prov.value("documentationRef", "");
            result.recipe.provenance.migratedFromSchemaVersion = prov.value("migratedFromSchemaVersion", "");
            result.recipe.provenance.migrationToolVersion = prov.value("migrationToolVersion", "");
            result.recipe.provenance.sourceRecipeDocumentHash = prov.value("sourceRecipeDocumentHash", "");
            result.recipe.provenance.sourceKind = prov.value("sourceKind", "");
            result.recipe.provenance.sourceExplorationId = prov.value("sourceExplorationId", "");
            result.recipe.provenance.sourceExplorationHash = prov.value("sourceExplorationHash", "");
            result.recipe.provenance.promotionToolVersion = prov.value("promotionToolVersion", "");
        }
    }

    return result;
}

std::string MeasurementRecipeService::serializeRecipeToJson(const MeasurementRecipe& recipe)
{
    json root = json::object();
    root["schemaVersion"] = recipe.schemaVersion.empty() ? "1.0" : recipe.schemaVersion;
    root["kind"] = recipe.kind.empty() ? "abd.measurement-recipe" : recipe.kind;
    root["recipeId"] = recipe.recipeId;
    root["displayName"] = recipe.displayName;
    if (!recipe.description.empty())
        root["description"] = recipe.description;
    root["assistanceLevel"] = assistanceLevelToString(recipe.assistanceLevel);
    root["revision"] = recipe.revision;

    // targetConstraints
    json tc = json::object();
    tc["targetKinds"] = recipe.targetConstraints.targetKinds;
    if (!recipe.targetConstraints.requiredCapabilities.empty())
        tc["requiredCapabilities"] = recipe.targetConstraints.requiredCapabilities;
    tc["allowedSampleRatesHz"] = recipe.targetConstraints.allowedSampleRatesHz;
    tc["channels"] = recipe.targetConstraints.channels;
    root["targetConstraints"] = tc;

    // excitation
    json exc = json::object();
    json notesArray = json::array();
    for (const auto& n : recipe.excitation.notes)
    {
        json noteObj = json::object();
        noteObj["midiNote"] = n.midiNote;
        noteObj["velocity"] = n.velocity;
        noteObj["gateMs"] = n.gateMs;
        noteObj["settlingMs"] = n.settlingMs;
        notesArray.push_back(noteObj);
    }
    exc["notes"] = notesArray;
    exc["repetitions"] = recipe.excitation.repetitions;
    if (recipe.excitation.seed.has_value())
        exc["seed"] = *recipe.excitation.seed;
    root["excitation"] = exc;

    // measurement
    json meas = json::object();
    json pointsArray = json::array();
    for (const auto& p : recipe.measurement.points)
    {
        json ptObj = json::object();
        ptObj["parameter"] = p.parameter;
        ptObj["normalizedValue"] = p.normalizedValue;
        pointsArray.push_back(ptObj);
    }
    meas["points"] = pointsArray;
    meas["calibrationPolicy"] = recipe.measurement.calibrationPolicy.empty() ? "Required" : recipe.measurement.calibrationPolicy;
    meas["analysisPolicy"] = recipe.measurement.analysisPolicy.empty() ? "CanonicalV1" : recipe.measurement.analysisPolicy;
    root["measurement"] = meas;

    // evaluationPolicy
    json eval = json::object();
    eval["minimumSnrDb"] = recipe.evaluationPolicy.minimumSnrDb;
    eval["maximumThdPercent"] = recipe.evaluationPolicy.maximumThdPercent;
    eval["f0ToleranceCents"] = recipe.evaluationPolicy.f0ToleranceCents;
    root["evaluationPolicy"] = eval;

    // provenance
    json prov = json::object();
    prov["authoringSource"] = recipe.provenance.authoringSource.empty() ? "builtin" : recipe.provenance.authoringSource;
    if (!recipe.provenance.documentationRef.empty())
        prov["documentationRef"] = recipe.provenance.documentationRef;
    if (!recipe.provenance.migratedFromSchemaVersion.empty())
        prov["migratedFromSchemaVersion"] = recipe.provenance.migratedFromSchemaVersion;
    if (!recipe.provenance.migrationToolVersion.empty())
        prov["migrationToolVersion"] = recipe.provenance.migrationToolVersion;
    if (!recipe.provenance.sourceRecipeDocumentHash.empty())
        prov["sourceRecipeDocumentHash"] = recipe.provenance.sourceRecipeDocumentHash;
    if (!recipe.provenance.sourceKind.empty())
        prov["sourceKind"] = recipe.provenance.sourceKind;
    if (!recipe.provenance.sourceExplorationId.empty())
        prov["sourceExplorationId"] = recipe.provenance.sourceExplorationId;
    if (!recipe.provenance.sourceExplorationHash.empty())
        prov["sourceExplorationHash"] = recipe.provenance.sourceExplorationHash;
    if (!recipe.provenance.promotionToolVersion.empty())
        prov["promotionToolVersion"] = recipe.provenance.promotionToolVersion;
    root["provenance"] = prov;

    return root.dump();
}

} // namespace abdaudiolab::profiling
