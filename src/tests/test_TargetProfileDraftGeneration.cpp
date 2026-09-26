#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "profiling/TargetProfileService.h"
#include "profiling/TargetProfileDraft.h"
#include "synth/TargetContract.h"

using namespace abdaudiolab::profiling;
using namespace abdaudiolab::synth;

TEST_CASE("HITO-10C: TargetProfileDraft Generation and Promotion", "[target_profile][draft]")
{
    TargetProfileService service;

    // 1. Construir un TargetContract sintetico representativo
    TargetContract contract;
    contract.name = "MockDexedVST3";
    contract.manufacturer = "Digital Suburban";
    contract.format = "VST3";
    contract.pluginUid = "vst3-mock-dexed-uid";

    TargetParameterDescriptor p0;
    p0.nativeId = "Cutoff";
    p0.nativeName = "Cutoff Frequency";
    p0.defaultValue = 0.5;
    p0.minValue = 0.0;
    p0.maxValue = 1.0;
    p0.category = ParameterCategory::Filter;
    contract.parameters.push_back(p0);

    TargetParameterDescriptor p1;
    p1.nativeId = "Resonance";
    p1.nativeName = "Filter Resonance";
    p1.defaultValue = 0.0;
    p1.minValue = 0.0;
    p1.maxValue = 1.0;
    p1.category = ParameterCategory::Filter;
    contract.parameters.push_back(p1);

    TargetParameterDescriptor p2;
    p2.nativeId = "Master";
    p2.nativeName = "Master Volume";
    p2.defaultValue = 0.8;
    p2.minValue = 0.0;
    p2.maxValue = 1.0;
    p2.category = ParameterCategory::Gain;
    contract.parameters.push_back(p2);

    TargetParameterDescriptor p3;
    p3.nativeId = "algo";
    p3.nativeName = "FM Algorithm";
    p3.defaultValue = 0.0;
    p3.minValue = 0.0;
    p3.maxValue = 31.0;
    p3.isDiscrete = true;
    p3.category = ParameterCategory::Custom;
    contract.parameters.push_back(p3);

    TargetParameterDescriptor p4;
    p4.nativeId = "op1_ratio";
    p4.nativeName = "Operator 1 Coarse Frequency";
    p4.defaultValue = 1.0;
    p4.minValue = 0.0;
    p4.maxValue = 31.0;
    p4.category = ParameterCategory::Oscillator;
    contract.parameters.push_back(p4);

    SECTION("1. Generacion de TargetProfileDraft preservando evidencia tecnica")
    {
        TargetProfileDraft draft = service.generateDraftFromContract(contract, "mock-sha256-hash");

        CHECK(draft.schemaVersion == "1.0");
        CHECK(draft.kind == "abd.target-profile-draft");
        CHECK(draft.sourceTargetName == "MockDexedVST3");
        CHECK(draft.sourceVendor == "Digital Suburban");
        CHECK(draft.sourceFormat == "VST3");
        CHECK(draft.sourcePluginUid == "vst3-mock-dexed-uid");
        CHECK(draft.sourceBinaryHash == "mock-sha256-hash");
        CHECK(draft.parameters.size() == 5);

        // Regla fundamental: Un draft NUNCA es ejecutable
        CHECK_FALSE(draft.isExecutable());
    }

    SECTION("2. Clasificacion honesta Inferred vs Unknown ('Descubrir no es comprender')")
    {
        TargetProfileDraft draft = service.generateDraftFromContract(contract, "mock-sha256-hash");

        const auto* cutoff = draft.findParameterById("Cutoff");
        REQUIRE(cutoff != nullptr);
        CHECK(cutoff->parameterIndex == 0);
        CHECK(cutoff->suggestedSemanticId == "filter_cutoff");
        CHECK(cutoff->semanticStatus == DraftSemanticStatus::Inferred);
        CHECK(cutoff->inferenceReason == "name_contains_cutoff");

        const auto* res = draft.findParameterById("Resonance");
        REQUIRE(res != nullptr);
        CHECK(res->parameterIndex == 1);
        CHECK(res->suggestedSemanticId == "filter_resonance");
        CHECK(res->semanticStatus == DraftSemanticStatus::Inferred);
        CHECK(res->inferenceReason == "name_contains_resonance");

        const auto* vol = draft.findParameterById("Master");
        REQUIRE(vol != nullptr);
        CHECK(vol->parameterIndex == 2);
        CHECK(vol->suggestedSemanticId == "master_volume");
        CHECK(vol->semanticStatus == DraftSemanticStatus::Inferred);
        CHECK(vol->inferenceReason == "name_contains_volume");

        const auto* algo = draft.findParameterById("algo");
        REQUIRE(algo != nullptr);
        CHECK(algo->parameterIndex == 3);
        CHECK(algo->suggestedSemanticId.empty());
        CHECK(algo->semanticStatus == DraftSemanticStatus::Unknown);
        CHECK(algo->inferenceReason == "unrecognized_semantics");

        const auto* op1 = draft.findParameterById("op1_ratio");
        REQUIRE(op1 != nullptr);
        CHECK(op1->parameterIndex == 4);
        CHECK(op1->suggestedSemanticId.empty());
        CHECK(op1->semanticStatus == DraftSemanticStatus::Unknown);
    }

    SECTION("3. Serializacion JSON auditable de TargetProfileDraft")
    {
        TargetProfileDraft draft = service.generateDraftFromContract(contract, "mock-sha256-hash");
        std::string jsonStr = draft.toJson();

        CHECK_FALSE(jsonStr.empty());
        CHECK(jsonStr.find("\"isExecutable\": false") != std::string::npos);
        CHECK(jsonStr.find("\"discoveredName\": \"Cutoff Frequency\"") != std::string::npos);
        CHECK(jsonStr.find("\"semanticStatus\": \"Inferred\"") != std::string::npos);
        CHECK(jsonStr.find("\"semanticStatus\": \"Unknown\"") != std::string::npos);
    }

    SECTION("4. Promocion controlada de Draft a TargetProfile formal")
    {
        TargetProfileDraft draft = service.generateDraftFromContract(contract, "mock-sha256-hash");

        std::vector<TargetProfileService::ConfirmedMappingRequest> confirmed = {
            { "filter_cutoff", 0, "Cutoff", "Cutoff Frequency", { 0.0, 1.0 }, "UserConfirmed" },
            { "filter_resonance", 1, "Resonance", "Resonance", { 0.0, 1.0 }, "UserConfirmed" }
        };

        TargetProfile profile = service.promoteDraftToProfile(
            draft,
            "vst3-mock-dexed-promoted",
            "Mock Dexed Synthesizer",
            "Digital Suburban",
            confirmed,
            "warn-on-mismatch"
        );

        CHECK(profile.targetProfileId == "vst3-mock-dexed-promoted");
        CHECK(profile.displayName == "Mock Dexed Synthesizer");
        CHECK(profile.vendor == "Digital Suburban");
        CHECK(profile.targetKind == "PluginVST3");
        CHECK(profile.identity.binaryIdentityPolicy == "warn-on-mismatch");
        CHECK(profile.identity.expectedBinarySha256 == "mock-sha256-hash");

        // Solo los parámetros explícitamente confirmados se incorporan
        REQUIRE(profile.parameters.size() == 2);
        const auto* cMap = profile.findMappingForSemanticId("filter_cutoff");
        REQUIRE(cMap != nullptr);
        CHECK(cMap->confirmationStatus == "UserConfirmed");
        CHECK(cMap->getTransportKind() == ControlTransportKind::VST3Parameter);

        const auto* rMap = profile.findMappingForSemanticId("filter_resonance");
        REQUIRE(rMap != nullptr);
        CHECK(rMap->confirmationStatus == "UserConfirmed");

        // Los parámetros no confirmados (Unknown o Inferred no promovidos) NO existen en el perfil formal
        CHECK(profile.findMappingForSemanticId("master_volume") == nullptr);
        CHECK(profile.findMappingForSemanticId("algo") == nullptr);
    }
}
