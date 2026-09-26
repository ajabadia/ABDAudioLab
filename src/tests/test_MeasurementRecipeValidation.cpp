#include <catch2/catch_test_macros.hpp>
#include <string>
#include "profiling/MeasurementRecipeService.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-09A: Validacion estricta y rechazo de campos desconocidos", "[recipe][validation]")
{
    MeasurementRecipeService service;

    SECTION("Campo tipografico desconocido ('repetitons') es rechazado fatalmente con puntero RFC 6901")
    {
        const std::string unknownFieldJson = R"({
            "schemaVersion": "1.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "test_typo",
            "displayName": "Test Typo",
            "description": "Receta con error tipografico",
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
                "repetitons": 3
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

        const auto result = service.loadAndValidateJson(unknownFieldJson);
        REQUIRE_FALSE(result.isSuccess());

        bool foundUnknownProperty = false;
        for (const auto& diag : result.diagnostics)
        {
            if (diag.code == "ERR_SCHEMA_UNKNOWN_FIELD" && diag.jsonPointer == "/excitation/repetitons")
            {
                foundUnknownProperty = true;
                break;
            }
        }
        CHECK(foundUnknownProperty);
    }

    SECTION("Version de esquema desconocida o futura (ej. '2.0') es rechazada fatalmente")
    {
        const std::string futureVersionJson = R"({
            "schemaVersion": "2.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "test_future_version",
            "displayName": "Test Future Version",
            "description": "Receta con version futura no soportada",
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

        const auto result = service.loadAndValidateJson(futureVersionJson);
        REQUIRE_FALSE(result.isSuccess());

        bool foundVersionError = false;
        for (const auto& diag : result.diagnostics)
        {
            if (diag.code == "ERR_SCHEMA_UNSUPPORTED_VERSION" && diag.jsonPointer == "/schemaVersion")
            {
                foundVersionError = true;
                break;
            }
        }
        CHECK(foundVersionError);
    }

    SECTION("Rangos fisicos fuera de limites son detectados y diagnosticados")
    {
        const std::string outOfBoundsJson = R"({
            "schemaVersion": "1.0",
            "kind": "abd.measurement-recipe",
            "recipeId": "test_bounds",
            "displayName": "Test Bounds",
            "description": "Receta con parametros fuera de rango",
            "assistanceLevel": "Quick",
            "revision": 1,
            "targetConstraints": {
                "targetKinds": ["PluginVST3"],
                "requiredCapabilities": ["vst3_parameters"],
                "allowedSampleRatesHz": [-48000],
                "channels": 2
            },
            "excitation": {
                "notes": [
                    { "midiNote": 130, "velocity": 1.5, "gateMs": -10.0, "settlingMs": 50.0 }
                ],
                "repetitions": 0
            },
            "measurement": {
                "points": [
                    { "parameter": "cutoff", "normalizedValue": 1.2 }
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

        const auto result = service.loadAndValidateJson(outOfBoundsJson);
        REQUIRE_FALSE(result.isSuccess());

        int boundErrorsCount = 0;
        for (const auto& diag : result.diagnostics)
        {
            if (diag.code == "ERR_SEMANTICS_INVALID_RANGE")
            {
                boundErrorsCount++;
            }
        }
        CHECK(boundErrorsCount >= 4); // midiNote, velocity, gateMs, normalizedValue, repetitions
    }
}
