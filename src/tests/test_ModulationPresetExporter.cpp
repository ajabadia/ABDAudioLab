#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "export/ModulationPresetExporter.h"
#include "core/HardwareContractRegistry.h"
#include "math/ModulationEstimator.h"

using namespace abdaudiolab;
using namespace abdaudiolab::exporting;
using namespace abdaudiolab::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("ModulationPresetExporter: Exports and roundtrips JSON manifest cleanly", "[preset_export][export]")
{
    core::HardwareContractRegistry registry;
    ModulationPresetExporter exporter(registry);

    // 1. Configuración de Metadatos
    ExportManifestConfig config;
    config.hardwareId = "roland_aira_bitrazer";
    config.hardwareName = "Roland AIRA Bitrazer Modular";
    config.firmwareVersion = "1.10";
    config.sampleRate = 48000.0;
    config.operatorNotes = "Automated test manifest generation";

    // 2. Curvas de calibración nominales (ej. Cutoff VCF)
    std::vector<NominalCalibrationCurve> curves;
    NominalCalibrationCurve cutoffCurve;
    cutoffCurve.parameterName = "FilterCutoff";
    cutoffCurve.standardId = "STANDARD_VCF_CUTOFF_HZ";
    cutoffCurve.points = {
        { 0.0f,  0.0f,  20.0f,    "Hz" },
        { 0.5f,  0.45f, 1200.0f,  "Hz" },
        { 1.0f,  1.0f,  20000.0f, "Hz" }
    };
    curves.push_back(cutoffCurve);

    // 3. Perfil de modulación con dos nodos: uno lineal puro y otro con zona muerta
    ModulationMatrixProfile profile;
    
    // Nodo 1: Velocity -> Cutoff (Lineal puro)
    std::vector<ModulationProbePoint> linearPoints = {
        { 32.0f, 16.0f },
        { 64.0f, 32.0f },
        { 96.0f, 48.0f },
        { 127.0f, 63.5f }
    };
    auto node1 = ModulationEstimator::calculateNode(1, 4, linearPoints);
    profile.setNode(1, 4, node1);

    // Nodo 2: Aftertouch -> Resonance (No lineal / zona muerta)
    std::vector<ModulationProbePoint> deadZonePoints = {
        { 32.0f, 0.0f },
        { 64.0f, 0.0f },
        { 96.0f, 20.0f },
        { 127.0f, 40.0f }
    };
    auto node2 = ModulationEstimator::calculateNode(2, 5, deadZonePoints);
    profile.setNode(2, 5, node2);

    // 4. Exportar a archivo temporal
    auto tempFile = juce::File::createTempFile(".json");
    bool success = exporter.exportConsolidatedManifest(tempFile, config, curves, profile);
    REQUIRE(success);
    REQUIRE(tempFile.existsAsFile());
    REQUIRE(tempFile.getSize() > 100);

    // 5. Parsear y verificar integridad con juce::JSON
    juce::var parsed = juce::JSON::parse(tempFile);
    REQUIRE(parsed.isObject());

    auto* rootObj = parsed.getDynamicObject();
    REQUIRE(rootObj != nullptr);

    // Verificar bloque Metadata
    REQUIRE(rootObj->hasProperty("metadata"));
    auto metaVar = rootObj->getProperty("metadata");
    REQUIRE(metaVar["hardwareId"].toString() == "roland_aira_bitrazer");
    REQUIRE(static_cast<double>(metaVar["sampleRate"]) == 48000.0);

    // Verificar bloque NominalCalibrationCurves
    REQUIRE(rootObj->hasProperty("nominalCalibrationCurves"));
    auto* curvesArray = rootObj->getProperty("nominalCalibrationCurves").getArray();
    REQUIRE(curvesArray != nullptr);
    REQUIRE(curvesArray->size() == 1);
    auto firstCurve = (*curvesArray)[0];
    REQUIRE(firstCurve["parameterName"].toString() == "FilterCutoff");
    auto* ptsArray = firstCurve["points"].getArray();
    REQUIRE(ptsArray != nullptr);
    REQUIRE(ptsArray->size() == 3);
    REQUIRE((*ptsArray)[2]["unit"].toString() == "Hz");

    // Verificar bloque ModulationMatrix
    REQUIRE(rootObj->hasProperty("modulationMatrix"));
    auto* modArray = rootObj->getProperty("modulationMatrix").getArray();
    REQUIRE(modArray != nullptr);
    REQUIRE(modArray->size() == 2);

    // Verificar bloque Diagnostics
    REQUIRE(rootObj->hasProperty("diagnostics"));
    auto diagVar = rootObj->getProperty("diagnostics");
    // Al haber un nodo con zona muerta (R^2 < 0.95), overallLinearityVerified debe ser false
    REQUIRE(static_cast<bool>(diagVar["overallLinearityVerified"]) == false);
    auto* warningsArray = diagVar["telemetryWarnings"].getArray();
    REQUIRE(warningsArray != nullptr);
    REQUIRE(warningsArray->size() >= 1);

    // Limpieza
    tempFile.deleteFile();
}
