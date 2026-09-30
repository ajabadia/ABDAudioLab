// ==============================================================================
// ABDAudioLab - HITO-AUDIO-AB-5D.9: Baseline Manifest & Artifact Validation
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include "core/LabResourcePaths.h"
#include "synth/Sha256.h"

using namespace abdaudiolab::synth;

namespace {

inline juce::File getDocsQaDirectory() { return abdaudiolab::core::docsQaDir(); }

} // namespace

TEST_CASE("5D.9 - 1. Baseline Manifest Integrity and Strict Schema Validation",
          "[audioab_5d][manifest][baseline]")
{
    juce::File qaDir = getDocsQaDirectory();
    REQUIRE(qaDir.isDirectory());

    juce::File manifestFile = qaDir.getChildFile("audio-ab-5d-baseline-manifest.json");
    REQUIRE(manifestFile.existsAsFile());

    auto manifestJson = nlohmann::json::parse(manifestFile.loadFileAsString().toStdString());

    // 1. Identificadores y contratos rectores
    CHECK(manifestJson["baselineId"] == "audio-ab-5d-canonical-v1");
    CHECK(manifestJson["policyId"] == "audio-ab-5d-provisional-v1");
    CHECK(manifestJson["policyDocumentHash"] == "sha256:7f45cbb662b66299b9cf2a70d9a6c924cfdd62479e0a0d6ee0bf0b1f83424d57");
    CHECK(manifestJson["calibrationStatus"] == "ProvisionalEvidenceBased");
    CHECK(manifestJson["runCount"] == 10);
    CHECK(manifestJson["artifactCount"] == 10);

    // 2. Veredictos esperados congelados
    CHECK(manifestJson["expectedVerdicts"]["PASS"] == 4);
    CHECK(manifestJson["expectedVerdicts"]["WARN"] == 6);
    CHECK(manifestJson["expectedVerdicts"]["FAIL"] == 0);

    // 3. Disposiciones esperadas congeladas
    CHECK(manifestJson["expectedDispositions"]["Accepted"] == 4);
    CHECK(manifestJson["expectedDispositions"]["AcceptableWithExpectedDispersion"] == 6);
    CHECK(manifestJson["expectedDispositions"]["Rejected"] == 0);

    // 4. Invariantes de seguridad y frontera de autoridad
    CHECK(manifestJson["hardwareRequired"] == false);
    CHECK(manifestJson["midiRequired"] == false);
    CHECK(manifestJson["vesIncluded"] == false);
    CHECK(manifestJson["exportReadiness"] == "Blocked");

    // 5. Lista de corridas
    REQUIRE(manifestJson.contains("runs"));
    const auto& runs = manifestJson["runs"];
    REQUIRE(runs.is_array());
    CHECK(runs.size() == 10);
}

TEST_CASE("5D.9 - 2. Canonical Runs Reports Integrity, Hashes and Non-Local Sanitization",
          "[audioab_5d][manifest][baseline]")
{
    juce::File qaDir = getDocsQaDirectory();
    REQUIRE(qaDir.isDirectory());

    juce::File manifestFile = qaDir.getChildFile("audio-ab-5d-baseline-manifest.json");
    REQUIRE(manifestFile.existsAsFile());

    auto manifestJson = nlohmann::json::parse(manifestFile.loadFileAsString().toStdString());
    const auto& runs = manifestJson["runs"];

    int passCount = 0;
    int warnCount = 0;
    int failCount = 0;

    for (const auto& runEntry : runs)
    {
        std::string runId = runEntry["runId"];
        std::string reportRelPath = runEntry["reportFile"];
        std::string expectedHash = runEntry["reportHash"];
        std::string expectedVerdict = runEntry["verdict"];
        std::string expectedDisp = runEntry["disposition"];

        juce::File repFile = qaDir.getChildFile(juce::String(reportRelPath));
        INFO("Comprobando reporte de corrida: " << runId << " en " << repFile.getFullPathName().toStdString());
        REQUIRE(repFile.existsAsFile());

        std::string repContent = repFile.loadFileAsString().toStdString();
        auto repJson = nlohmann::json::parse(repContent);

        // A. Campos requeridos de contrato
        CHECK(repJson["runId"] == runId);
        CHECK(repJson["policyId"] == "audio-ab-5d-provisional-v1");
        CHECK(repJson["policyDocumentHash"] == "sha256:7f45cbb662b66299b9cf2a70d9a6c924cfdd62479e0a0d6ee0bf0b1f83424d57");
        CHECK(repJson["reportHash"] == expectedHash);

        // B. Sanitización: sin rutas locales ni absolutas
        CHECK_FALSE(repContent.find("C:\\") != std::string::npos);
        CHECK_FALSE(repContent.find("D:\\") != std::string::npos);
        CHECK_FALSE(repContent.find("/Users/") != std::string::npos);
        CHECK_FALSE(repContent.find("/home/") != std::string::npos);

        // C. Determinismo intra-motor y silencio digital
        CHECK(repJson["intraEngineDeterminism"]["identical"] == true);
        CHECK(repJson["intraEngineDeterminism"]["maxDifference"] == 0.0);
        CHECK(repJson["temporal"]["warmupAIsDigitalSilence"] == true);
        CHECK(repJson["temporal"]["warmupBIsDigitalSilence"] == true);
        CHECK(repJson["amplitude"]["clippingA"] == false);
        CHECK(repJson["amplitude"]["clippingB"] == false);
        CHECK(repJson["amplitude"]["unexpectedClipping"] == false);

        // D. Veredicto y disposición
        std::string vLevel = repJson["verdict"]["verdictLevel"];
        std::string vDisp  = repJson["verdict"]["acceptanceDisposition"];

        CHECK(vLevel == expectedVerdict);
        CHECK(vDisp == expectedDisp);

        if (vLevel == "PASS") passCount++;
        else if (vLevel == "WARN") warnCount++;
        else failCount++;

        // E. Verificación del hash canónico
        auto repCopy = repJson;
        repCopy.erase("reportHash");
        std::string canonicalDump = repCopy.dump();
        std::string computedHash = "sha256:" + Sha256::computeHex(canonicalDump);
        CHECK(computedHash == expectedHash);
    }

    CHECK(passCount == 4);
    CHECK(warnCount == 6);
    CHECK(failCount == 0);
}
