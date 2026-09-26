#include <catch2/catch_test_macros.hpp>
#include <string>
#include "profiling/MeasurementRecipeService.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-09A: Ingestion y Parsing estricto de MeasurementRecipe", "[recipe][parsing]")
{
    MeasurementRecipeService service;

    SECTION("Sintaxis JSON malformada produce diagnostico sintactico RFC 6901")
    {
        const std::string badJson = "{ \"schemaVersion\": \"1.0\", "; // Truncado
        const auto result = service.loadAndValidateJson(badJson);

        REQUIRE_FALSE(result.isSuccess());
        REQUIRE(result.hasErrors());
        REQUIRE_FALSE(result.diagnostics.empty());
        CHECK(result.diagnostics[0].code == "ERR_SYNTAX_INVALID_JSON");
        CHECK(result.diagnostics[0].jsonPointer == "");
    }

    SECTION("Root que no es un objeto JSON produce error fatal")
    {
        const std::string arrayJson = "[1, 2, 3]";
        const auto result = service.loadAndValidateJson(arrayJson);

        REQUIRE_FALSE(result.isSuccess());
        REQUIRE(result.hasErrors());
        CHECK(result.diagnostics[0].code == "ERR_SCHEMA_ROOT_MUST_BE_OBJECT");
        CHECK(result.diagnostics[0].jsonPointer == "");
    }

    SECTION("Valor invalido en AssistanceLevel produce diagnostico de enum con puntero")
    {
        const std::string invalidEnumJson = R"({
            "schemaVersion": "1.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "test_bad_enum",
            "displayName": "Test Bad Enum",
            "description": "Prueba de enum invalido",
            "assistanceLevel": "HyperSonicDrive",
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

        const auto result = service.loadAndValidateJson(invalidEnumJson);
        REQUIRE_FALSE(result.isSuccess());
        
        bool foundEnumError = false;
        for (const auto& diag : result.diagnostics)
        {
            if (diag.code == "ERR_SCHEMA_INVALID_ASSISTANCE_LEVEL" && diag.jsonPointer == "/assistanceLevel")
            {
                foundEnumError = true;
                break;
            }
        }
        CHECK(foundEnumError);
    }

    SECTION("Tipo de dato incorrecto (string en vez de array) produce diagnostico")
    {
        const std::string badTypeJson = R"({
            "schemaVersion": "1.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "test_bad_type",
            "displayName": "Test Bad Type",
            "description": "Prueba de tipo erroneo",
            "assistanceLevel": "Quick",
            "revision": 1,
            "targetConstraints": {
                "targetKinds": "NotAnArray",
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

        const auto result = service.loadAndValidateJson(badTypeJson);
        REQUIRE_FALSE(result.isSuccess());
        
        bool foundTypeError = false;
        for (const auto& diag : result.diagnostics)
        {
            if (diag.code == "ERR_SCHEMA_EMPTY_TARGET_KINDS" && diag.jsonPointer == "/targetConstraints/targetKinds")
            {
                foundTypeError = true;
                break;
            }
        }
        CHECK(foundTypeError);
    }
}
