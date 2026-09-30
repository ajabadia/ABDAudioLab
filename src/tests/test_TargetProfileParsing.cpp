/**
 * @file test_TargetProfileParsing.cpp
 * @brief HITO-10A: Verificación Contractual de Parsing y Modelo C++ de TargetProfile.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <string>
#include <vector>

#include "core/LabResourcePaths.h"
#include "profiling/TargetProfile.h"
#include "profiling/TargetProfileService.h"

using namespace abdaudiolab::profiling;

namespace
{

inline juce::File getProfilesDirectory() { return abdaudiolab::core::canonicalTargetsDir(); }

} // namespace

TEST_CASE("TargetProfile: Carga y Parsing de Perfil Piloto ReferenceSynth", "[target_profile][parsing]")
{
    juce::File profileFile = getProfilesDirectory().getChildFile("reference_synth.target.json");
    REQUIRE(profileFile.existsAsFile());

    TargetProfileService service;
    auto result = service.loadAndValidateProfile(profileFile);

    REQUIRE(result.isSuccess());
    CHECK(result.diagnostics.empty());
    CHECK_FALSE(result.canonicalProfileHash.empty());

    const auto& p = result.profile;
    CHECK(p.schemaVersion == "1.0");
    CHECK(p.kind == "abd.target-profile");
    CHECK(p.targetProfileId == "org.abd.reference-synth.v1");
    CHECK(p.displayName == "Reference Synth");
    CHECK(p.vendor == "ABD AudioLab");
    CHECK(p.targetKind == "SyntheticFixture");
    CHECK(p.revision == 1);

    // Identidad
    CHECK(p.identity.canonicalTargetId == "reference-synth");
    REQUIRE(p.identity.acceptedUniqueIds.size() == 1);
    CHECK(p.identity.acceptedUniqueIds[0] == "reference-synth-v1");
    CHECK(p.identity.binaryIdentityPolicy == "not-applicable");

    // Capacidades
    CHECK(p.capabilities.midiInput);
    CHECK(p.capabilities.supportsParameterAutomation);
    CHECK(p.capabilities.supportsPolyphony);
    REQUIRE(p.capabilities.controlTransports.size() == 1);
    CHECK(p.capabilities.controlTransports[0] == ControlTransportKind::InternalParameter);

    // audioOutput explícito
    CHECK(p.capabilities.audioOutput.requiredChannelCount == 2);
    CHECK(p.capabilities.audioOutput.channelLayout == "stereo");
    REQUIRE(p.capabilities.audioOutput.supportedChannelCounts.size() == 1);
    CHECK(p.capabilities.audioOutput.supportedChannelCounts[0] == 2);
    REQUIRE(p.capabilities.audioOutput.supportedObservationLayouts.size() == 1);
    CHECK(p.capabilities.audioOutput.supportedObservationLayouts[0] == "stereo");

    CHECK(p.capabilities.midiNoteRange.first == 0);
    CHECK(p.capabilities.midiNoteRange.second == 127);

    // Parámetros y tipado discriminado de TechnicalIdentifier
    REQUIRE(p.parameters.size() == 2);

    const auto* cutoff = p.findMappingForSemanticId("filter_cutoff");
    REQUIRE(cutoff != nullptr);
    CHECK(cutoff->displayName == "Filter Cutoff");
    CHECK(cutoff->getTransportKind() == ControlTransportKind::InternalParameter);
    REQUIRE(std::holds_alternative<InternalParameterIdentifier>(cutoff->technicalIdentifier));
    CHECK(std::get<InternalParameterIdentifier>(cutoff->technicalIdentifier).parameterKey == "filter.cutoff");
    CHECK(cutoff->normalizedRange.first == Catch::Approx(0.0));
    CHECK(cutoff->normalizedRange.second == Catch::Approx(1.0));

    const auto* res = p.findMappingForSemanticId("filter_resonance");
    REQUIRE(res != nullptr);
    CHECK(res->displayName == "Filter Resonance");
    CHECK(res->getTransportKind() == ControlTransportKind::InternalParameter);
    REQUIRE(std::holds_alternative<InternalParameterIdentifier>(res->technicalIdentifier));
    CHECK(std::get<InternalParameterIdentifier>(res->technicalIdentifier).parameterKey == "filter.resonance");

    // Políticas de ensayo
    CHECK(p.measurementPolicies.warmupTimeMs == 0);
    CHECK(p.measurementPolicies.defaultSettlingTimeMs == 50);
    CHECK(p.measurementPolicies.recommendedCalibrationPolicy == "None");
    CHECK_FALSE(p.measurementPolicies.requiresResetBetweenTrials);
}

TEST_CASE("TargetProfile: Round-Trip de Serialización y Determinismo de Hash", "[target_profile][roundtrip]")
{
    juce::File profileFile = getProfilesDirectory().getChildFile("reference_synth.target.json");
    TargetProfileService service;
    auto res1 = service.loadAndValidateProfile(profileFile);
    REQUIRE(res1.isSuccess());

    // Serializar a JSON
    std::string serialized = TargetProfileService::serializeProfileToJson(res1.profile);
    REQUIRE_FALSE(serialized.empty());

    // Re-parsear
    auto res2 = service.loadAndValidateProfileJson(serialized);
    REQUIRE(res2.isSuccess());

    // Invarianza absoluta de hash canónico RFC 8785
    CHECK(res1.canonicalProfileHash == res2.canonicalProfileHash);
    CHECK(res1.profile.targetProfileId == res2.profile.targetProfileId);
    CHECK(res1.profile.parameters.size() == res2.profile.parameters.size());
}
