#include <catch2/catch_test_macros.hpp>
#include <string>
#include "profiling/MeasurementRecipeService.h"
#include "profiling/ExperimentPlanCompiler.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-09A: Separacion y reproducibilidad de la cadena de 4 Hashes Canonicos", "[recipe][hash]")
{
    MeasurementRecipeService service;

    SECTION("Invarianza de RFC 8785 ante reordenacion de claves en el documento JSON")
    {
        const std::string jsonOrderA = R"({
            "schemaVersion": "1.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "hash_test_order",
            "displayName": "Order Test",
            "description": "Prueba de reordenacion RFC 8785",
            "assistanceLevel": "Quick",
            "revision": 1,
            "targetConstraints": {
                "targetKinds": ["PluginVST3"],
                "requiredCapabilities": ["vst3_parameters"],
                "allowedSampleRatesHz": [48000],
                "channels": 2
            },
            "excitation": {
                "notes": [
                    { "midiNote": 60, "velocity": 0.5, "gateMs": 250.0, "settlingMs": 50.0 }
                ],
                "repetitions": 1
            },
            "measurement": {
                "points": [
                    { "parameter": "cutoff", "normalizedValue": 0.5 }
                ],
                "calibrationPolicy": "Required",
                "analysisPolicy": "CanonicalV1"
            },
            "evaluationPolicy": {
                "minimumSnrDb": 50.0,
                "maximumThdPercent": 1.0,
                "f0ToleranceCents": 10.0
            },
            "provenance": {
                "authoringSource": "builtin",
                "documentationRef": "test"
            }
        })";

        // Mismas claves y valores pero en orden arbitrario inverso
        const std::string jsonOrderB = R"({
            "provenance": {
                "documentationRef": "test",
                "authoringSource": "builtin"
            },
            "evaluationPolicy": {
                "f0ToleranceCents": 10.0,
                "maximumThdPercent": 1.0,
                "minimumSnrDb": 50.0
            },
            "measurement": {
                "analysisPolicy": "CanonicalV1",
                "calibrationPolicy": "Required",
                "points": [
                    { "normalizedValue": 0.5, "parameter": "cutoff" }
                ]
            },
            "excitation": {
                "repetitions": 1,
                "notes": [
                    { "settlingMs": 50.0, "gateMs": 250.0, "velocity": 0.5, "midiNote": 60 }
                ]
            },
            "targetConstraints": {
                "channels": 2,
                "allowedSampleRatesHz": [48000],
                "requiredCapabilities": ["vst3_parameters"],
                "targetKinds": ["PluginVST3"]
            },
            "revision": 1,
            "assistanceLevel": "Quick",
            "description": "Prueba de reordenacion RFC 8785",
            "displayName": "Order Test",
            "recipeId": "hash_test_order",
            "kind": "abd.measurement-recipe",
            "schemaVersion": "1.0"
        })";

        const auto resultA = service.loadAndValidateJson(jsonOrderA);
        const auto resultB = service.loadAndValidateJson(jsonOrderB);

        REQUIRE(resultA.isSuccess());
        REQUIRE(resultB.isSuccess());

        // Ambos documentos JSON deben arrojar exactamente el mismo recipeDocumentHash
        CHECK(resultA.recipeDocumentHash == resultB.recipeDocumentHash);
    }

    SECTION("Sensibilidad estricta: cambiar una nota muta recipeDocumentHash y experimentPlanHash")
    {
        const std::string jsonBase = R"({
            "schemaVersion": "1.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "hash_test_mutation",
            "displayName": "Mutation Test",
            "description": "Prueba de sensibilidad",
            "assistanceLevel": "Quick",
            "revision": 1,
            "targetConstraints": {
                "targetKinds": ["PluginVST3"],
                "requiredCapabilities": ["vst3_parameters"],
                "allowedSampleRatesHz": [48000],
                "channels": 2
            },
            "excitation": {
                "notes": [
                    { "midiNote": 60, "velocity": 0.5, "gateMs": 250.0, "settlingMs": 50.0 }
                ],
                "repetitions": 1
            },
            "measurement": {
                "points": [
                    { "parameter": "cutoff", "normalizedValue": 0.5 }
                ],
                "calibrationPolicy": "Required",
                "analysisPolicy": "CanonicalV1"
            },
            "evaluationPolicy": {
                "minimumSnrDb": 50.0,
                "maximumThdPercent": 1.0,
                "f0ToleranceCents": 10.0
            },
            "provenance": {
                "authoringSource": "builtin",
                "documentationRef": "test"
            }
        })";

        std::string jsonMutated = jsonBase;
        // Cambiar midiNote 60 por 61
        const size_t pos = jsonMutated.find("\"midiNote\": 60");
        REQUIRE(pos != std::string::npos);
        jsonMutated.replace(pos, 14, "\"midiNote\": 61");

        const auto resultBase = service.loadAndValidateJson(jsonBase);
        const auto resultMut = service.loadAndValidateJson(jsonMutated);

        REQUIRE(resultBase.isSuccess());
        REQUIRE(resultMut.isSuccess());

        CHECK(resultBase.recipeDocumentHash != resultMut.recipeDocumentHash);

        const auto planBase = ExperimentPlanCompiler::compileToExperimentPlan(resultBase.recipe);
        const auto planMut = ExperimentPlanCompiler::compileToExperimentPlan(resultMut.recipe);

        CHECK(planBase.planHash != planMut.planHash);
    }

    SECTION("Ajuste 1: experimentPlanHash es invariable ante cambios de entorno fisico, mientras resolvedExecutionPlanHash cambia")
    {
        const std::string jsonDoc = R"({
            "schemaVersion": "1.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "hash_test_env_independence",
            "displayName": "Env Independence Test",
            "description": "Prueba de separacion de hashes",
            "assistanceLevel": "Quick",
            "revision": 1,
            "targetConstraints": {
                "targetKinds": ["PluginVST3"],
                "requiredCapabilities": ["vst3_parameters"],
                "allowedSampleRatesHz": [44100, 48000, 96000],
                "channels": 2
            },
            "excitation": {
                "notes": [
                    { "midiNote": 60, "velocity": 0.5, "gateMs": 250.0, "settlingMs": 50.0 }
                ],
                "repetitions": 1
            },
            "measurement": {
                "points": [
                    { "parameter": "cutoff", "normalizedValue": 0.5 }
                ],
                "calibrationPolicy": "Required",
                "analysisPolicy": "CanonicalV1"
            },
            "evaluationPolicy": {
                "minimumSnrDb": 50.0,
                "maximumThdPercent": 1.0,
                "f0ToleranceCents": 10.0
            },
            "provenance": {
                "authoringSource": "builtin",
                "documentationRef": "test"
            }
        })";

        const auto loadRes = service.loadAndValidateJson(jsonDoc);
        REQUIRE(loadRes.isSuccess());

        // Compilar el plan científico puro
        const auto planA = ExperimentPlanCompiler::compileToExperimentPlan(loadRes.recipe);
        const auto planB = ExperimentPlanCompiler::compileToExperimentPlan(loadRes.recipe);

        // El experimentPlanHash es IDENTICO e independiente de la maquina o entorno
        CHECK(planA.planHash == planB.planHash);

        // Entorno 1: MockAudioEngine a 48kHz, buffer 256
        ExecutionEnvironment env1;
        env1.driver = "MockAudioEngine";
        env1.sampleRate = 48000.0;
        env1.blockSize = 256;
        env1.discoveredCapabilities = { "vst3_parameters" };

        // Entorno 2: WASAPI a 96kHz, buffer 512
        ExecutionEnvironment env2;
        env2.driver = "WASAPI";
        env2.sampleRate = 96000.0;
        env2.blockSize = 512;
        env2.discoveredCapabilities = { "vst3_parameters" };

        const auto res1 = ExperimentPlanCompiler::resolveExecutionPlan(planA, env1, &loadRes.recipe);
        const auto res2 = ExperimentPlanCompiler::resolveExecutionPlan(planA, env2, &loadRes.recipe);

        REQUIRE(res1.succeeded());
        REQUIRE(res2.succeeded());
        REQUIRE(res1.resolvedPlan.has_value());
        REQUIRE(res2.resolvedPlan.has_value());

        // Reproducibilidad cientifica: el plan de origen sigue siendo el mismo
        CHECK(res1.resolvedPlan->experimentPlan.planHash == res2.resolvedPlan->experimentPlan.planHash);

        // Reproducibilidad de ejecucion: los hashes de realizacion concreta son distintos
        CHECK(res1.resolvedPlan->resolvedExecutionPlanHash != res2.resolvedPlan->resolvedExecutionPlanHash);
    }
}
