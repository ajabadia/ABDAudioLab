#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <string_view>
#include <vector>
#include "TargetProfile.h"
#include "TargetProfileDraft.h"
#include "MeasurementRecipe.h"
#include "synth/TargetContract.h"

namespace abdaudiolab::profiling
{

struct BinaryAuditResult
{
    bool passed { true };
    bool isWarning { false };
    std::string expectedHash;
    std::string observedHash;
    std::string policy;
    std::vector<ValidationDiagnostic> diagnostics;

    [[nodiscard]] bool hasErrors() const noexcept
    {
        return !passed;
    }
};

struct TargetProfileLoadResult
{
    TargetProfile profile;
    std::string canonicalProfileHash;
    std::vector<ValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return std::none_of(diagnostics.begin(), diagnostics.end(), [](const ValidationDiagnostic& d) {
            return d.severity == DiagnosticSeverity::Error;
        });
    }

    [[nodiscard]] bool hasErrors() const noexcept { return !isSuccess(); }
};

class TargetProfileService
{
public:
    TargetProfileService() = default;
    ~TargetProfileService() = default;

    [[nodiscard]] TargetProfileLoadResult loadAndValidateProfile(const juce::File& file) const;
    [[nodiscard]] TargetProfileLoadResult loadAndValidateProfileJson(std::string_view jsonString) const;
    [[nodiscard]] static std::string canonicalizeJsonRfc8785(const std::string& rawJsonString);
    [[nodiscard]] static std::string computeCanonicalProfileHash(std::string_view jsonString);
    [[nodiscard]] const TargetParameterMapping* findMapping(const TargetProfile& profile,
                                                           std::string_view semanticId) const noexcept;
    [[nodiscard]] static std::string serializeProfileToJson(const TargetProfile& profile);

    [[nodiscard]] BinaryAuditResult auditBinaryFixity(
        const TargetProfile& profile,
        const std::string& observedBinaryHash) const;

    [[nodiscard]] TargetProfileDraft generateDraftFromContract(
        const synth::TargetContract& contract,
        const std::string& binaryHash = {}) const;

    struct ConfirmedMappingRequest
    {
        std::string semanticId;
        int parameterIndex { -1 };
        std::string parameterId;
        std::string displayName;
        std::pair<double, double> normalizedRange { 0.0, 1.0 };
        std::string confirmationStatus { "UserConfirmed" };
    };

    [[nodiscard]] TargetProfile promoteDraftToProfile(
        const TargetProfileDraft& draft,
        const std::string& targetProfileId,
        const std::string& displayName,
        const std::string& vendor,
        const std::vector<ConfirmedMappingRequest>& confirmedMappings,
        const std::string& binaryIdentityPolicy = "warn-on-mismatch") const;

    [[nodiscard]] static std::vector<uint8_t> formatSysExMessage(
        const MidiSysExIdentifier& sysexId,
        uint8_t deviceId,
        double normalizedValue);
};

} // namespace abdaudiolab::profiling
