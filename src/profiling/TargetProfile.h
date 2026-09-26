#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <optional>
#include <utility>
#include <cstdint>
#include <algorithm>

namespace abdaudiolab::profiling
{

enum class ControlTransportKind
{
    InternalParameter = 0,
    VST3Parameter,
    MidiContinuousController,
    MidiSysEx,
    ManualOperator
};

struct InternalParameterIdentifier
{
    std::string parameterKey;

    bool operator==(const InternalParameterIdentifier& other) const noexcept
    {
        return parameterKey == other.parameterKey;
    }
};

struct Vst3ParameterIdentifier
{
    int32_t parameterIndex { -1 };
    std::string parameterId;

    bool operator==(const Vst3ParameterIdentifier& other) const noexcept
    {
        return parameterIndex == other.parameterIndex && parameterId == other.parameterId;
    }
};

struct MidiCcIdentifier
{
    int channel { 1 };
    int controllerNumber { 1 };

    bool operator==(const MidiCcIdentifier& other) const noexcept
    {
        return channel == other.channel && controllerNumber == other.controllerNumber;
    }
};

struct MidiSysExIdentifier
{
    std::string manufacturerId;           // Ej. "43" (Yamaha), "41" (Roland)
    std::string deviceIdPolicy { "user-configured" }; // "user-configured", "fixed", "omni"
    std::string messageTemplate;          // Ej. "F0 43 {deviceId} 09 40 {value7bit} F7" o "F0 43 00 09 40 XX F7"
    std::string valueEncoding { "7bit" }; // "7bit", "nibble-msb-first", "nibble-lsb-first"
    std::string checksumPolicy { "none" }; // "none", "yamaha-dx7", "roland"
    bool requiresExplicitConfirmation { false };

    bool operator==(const MidiSysExIdentifier& other) const noexcept
    {
        return manufacturerId == other.manufacturerId &&
               deviceIdPolicy == other.deviceIdPolicy &&
               messageTemplate == other.messageTemplate &&
               valueEncoding == other.valueEncoding &&
               checksumPolicy == other.checksumPolicy &&
               requiresExplicitConfirmation == other.requiresExplicitConfirmation;
    }
};

struct ManualOperatorIdentifier
{
    std::string instructionId;
    std::string confirmationPrompt;
    std::string controlWidget { "Knob" }; // "Knob", "Slider", "Switch", "Footswitch"

    bool operator==(const ManualOperatorIdentifier& other) const noexcept
    {
        return instructionId == other.instructionId &&
               confirmationPrompt == other.confirmationPrompt &&
               controlWidget == other.controlWidget;
    }
};

using TechnicalIdentifier = std::variant<
    InternalParameterIdentifier,
    Vst3ParameterIdentifier,
    MidiCcIdentifier,
    MidiSysExIdentifier,
    ManualOperatorIdentifier>;

struct TargetParameterMapping
{
    std::string semanticId;
    std::string displayName;
    TechnicalIdentifier technicalIdentifier;
    std::string valueType { "continuous" };
    std::pair<double, double> normalizedRange { 0.0, 1.0 };
    std::string mappingCurve { "linear" };
    std::string confirmationStatus { "Declared" };

    [[nodiscard]] ControlTransportKind getTransportKind() const noexcept
    {
        return static_cast<ControlTransportKind>(technicalIdentifier.index());
    }
};

enum class RetryPolicy
{
    None = 0,
    Linear,
    Exponential
};

struct TransportPolicy
{
    int minimumInterMessageDelayMs { 0 };
    int maximumMessagesPerSecond { 0 };

    bool requiresResponseAck { false };
    int responseTimeoutMs { 0 };

    RetryPolicy retryPolicy { RetryPolicy::None };
    int maxRetries { 0 };

    bool requiresExplicitConfirmation { false };
    bool allowsBulkDump { false };

    bool requiresVerifiedIdentity { false };
    bool allowsUserConfirmedUnverifiedIdentity { true };

    bool operator==(const TransportPolicy& other) const noexcept
    {
        return minimumInterMessageDelayMs == other.minimumInterMessageDelayMs &&
               maximumMessagesPerSecond == other.maximumMessagesPerSecond &&
               requiresResponseAck == other.requiresResponseAck &&
               responseTimeoutMs == other.responseTimeoutMs &&
               retryPolicy == other.retryPolicy &&
               maxRetries == other.maxRetries &&
               requiresExplicitConfirmation == other.requiresExplicitConfirmation &&
               allowsBulkDump == other.allowsBulkDump &&
               requiresVerifiedIdentity == other.requiresVerifiedIdentity &&
               allowsUserConfirmedUnverifiedIdentity == other.allowsUserConfirmedUnverifiedIdentity;
    }
};

struct TargetProfile
{
    std::string schemaVersion { "1.0" };
    std::string kind { "abd.target-profile" };
    std::string targetProfileId;
    std::string displayName;
    std::string vendor;
    std::string targetKind;
    int revision { 1 };

    struct Identity
    {
        std::string canonicalTargetId;
        std::vector<std::string> acceptedUniqueIds;
        std::string binaryIdentityPolicy { "not-applicable" };
        std::string expectedBinarySha256;
    } identity;

    struct AudioOutputCapabilities
    {
        std::vector<int> supportedChannelCounts { 2 };
        int requiredChannelCount { 2 };
        std::string channelLayout { "stereo" };
        std::vector<std::string> supportedObservationLayouts { "stereo" };
    };

    struct Capabilities
    {
        bool midiInput { true };
        bool supportsParameterAutomation { true };
        std::vector<ControlTransportKind> controlTransports;
        AudioOutputCapabilities audioOutput;
        std::vector<int> sampleRatesHz { 44100, 48000, 96000 };
        std::vector<int> blockSizes { 64, 128, 256, 512 };
        bool supportsPolyphony { true };
        std::vector<int> midiChannels { 1 };
        std::pair<int, int> midiNoteRange { 0, 127 };
    } capabilities;

    std::vector<TargetParameterMapping> parameters;

    struct MeasurementPolicies
    {
        int warmupTimeMs { 0 };
        int defaultSettlingTimeMs { 50 };
        std::string recommendedCalibrationPolicy { "None" };
        bool requiresResetBetweenTrials { false };
    } measurementPolicies;

    TransportPolicy transportPolicy;
    bool hasExplicitTransportPolicy { false };

    [[nodiscard]] const TargetParameterMapping* findMappingForSemanticId(std::string_view semanticId) const noexcept
    {
        auto it = std::find_if(parameters.begin(), parameters.end(),
                               [semanticId](const TargetParameterMapping& m) {
                                   if (m.semanticId == semanticId)
                                       return true;
                                   if ((semanticId == "vcf.cutoff" || semanticId == "cutoff") && m.semanticId == "filter_cutoff")
                                       return true;
                                   if (semanticId == "filter_cutoff" && (m.semanticId == "vcf.cutoff" || m.semanticId == "cutoff"))
                                       return true;
                                   if ((semanticId == "vcf.resonance" || semanticId == "resonance") && m.semanticId == "filter_resonance")
                                       return true;
                                   if (semanticId == "filter_resonance" && (m.semanticId == "vcf.resonance" || m.semanticId == "resonance"))
                                       return true;
                                   return false;
                               });
        return it != parameters.end() ? &(*it) : nullptr;
    }
};

} // namespace abdaudiolab::profiling
