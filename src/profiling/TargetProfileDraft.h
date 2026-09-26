#pragma once

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace abdaudiolab::profiling
{

/**
 * @brief Estado de certeza semántica del parámetro en el borrador (Draft).
 */
enum class DraftSemanticStatus
{
    Unknown,
    Inferred,
    UserConfirmed,
    Declared
};

inline const char* draftSemanticStatusToString(DraftSemanticStatus s) noexcept
{
    switch (s)
    {
        case DraftSemanticStatus::Unknown:       return "Unknown";
        case DraftSemanticStatus::Inferred:      return "Inferred";
        case DraftSemanticStatus::UserConfirmed: return "UserConfirmed";
        case DraftSemanticStatus::Declared:      return "Declared";
    }
    return "Unknown";
}

/**
 * @brief Descriptor individual de parámetro descubierto e inferido en el borrador.
 */
struct DraftParameterMapping
{
    int parameterIndex { -1 };
    std::string parameterId;
    std::string discoveredName;
    std::string suggestedSemanticId;
    DraftSemanticStatus semanticStatus { DraftSemanticStatus::Unknown };
    std::string inferenceReason;
    std::string valueType { "continuous" };
    std::pair<double, double> normalizedRange { 0.0, 1.0 };
    double defaultValue { 0.0 };
    std::string unit;
    bool isAutomatable { true };
};

/**
 * @brief Borrador de perfil generado automáticamente a partir de introspección técnica.
 * 
 * Regla normativa fundamental:
 * "Descubrir no es comprender. Inferir no es confirmar. Un borrador no es una autorización de ejecución científica."
 * isExecutable() retorna siempre false.
 */
struct TargetProfileDraft
{
    std::string schemaVersion { "1.0" };
    std::string kind { "abd.target-profile-draft" };
    std::string draftId;
    std::string sourceTargetName;
    std::string sourceVendor;
    std::string sourceFormat { "VST3" };
    std::string sourcePluginUid;
    std::string sourceBinaryHash;

    std::vector<DraftParameterMapping> parameters;

    [[nodiscard]] bool isExecutable() const noexcept
    {
        return false;
    }

    [[nodiscard]] const DraftParameterMapping* findParameterByIndex(int index) const noexcept
    {
        for (const auto& p : parameters)
        {
            if (p.parameterIndex == index)
                return &p;
        }
        return nullptr;
    }

    [[nodiscard]] const DraftParameterMapping* findParameterById(const std::string& id) const noexcept
    {
        for (const auto& p : parameters)
        {
            if (p.parameterId == id)
                return &p;
        }
        return nullptr;
    }

    [[nodiscard]] const DraftParameterMapping* findParameterBySuggestedSemanticId(const std::string& semanticId) const noexcept
    {
        for (const auto& p : parameters)
        {
            if (p.suggestedSemanticId == semanticId)
                return &p;
        }
        return nullptr;
    }

    [[nodiscard]] std::string toJson() const
    {
        nlohmann::json j;
        j["schemaVersion"] = schemaVersion;
        j["kind"] = kind;
        j["draftId"] = draftId;
        j["sourceTargetName"] = sourceTargetName;
        j["sourceVendor"] = sourceVendor;
        j["sourceFormat"] = sourceFormat;
        j["sourcePluginUid"] = sourcePluginUid;
        j["sourceBinaryHash"] = sourceBinaryHash;
        j["isExecutable"] = false;

        nlohmann::json paramsArr = nlohmann::json::array();
        for (const auto& p : parameters)
        {
            nlohmann::json pj;
            pj["parameterIndex"] = p.parameterIndex;
            pj["parameterId"] = p.parameterId;
            pj["discoveredName"] = p.discoveredName;
            pj["suggestedSemanticId"] = p.suggestedSemanticId;
            pj["semanticStatus"] = draftSemanticStatusToString(p.semanticStatus);
            pj["inferenceReason"] = p.inferenceReason;
            pj["valueType"] = p.valueType;
            pj["normalizedRange"] = { p.normalizedRange.first, p.normalizedRange.second };
            pj["defaultValue"] = p.defaultValue;
            pj["unit"] = p.unit;
            pj["isAutomatable"] = p.isAutomatable;
            paramsArr.push_back(pj);
        }
        j["parameters"] = paramsArr;

        return j.dump(2);
    }
};

} // namespace abdaudiolab::profiling
