/**
 * @file test_ConditionalExportWithMetadata.cpp
 * @brief Catch2 unit tests for Phase 19.6: Conditional Export and Traceable Metadata.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "export/ExportReadinessEvaluator.h"
#include "export/LutExporter.h"
#include "support/LabTestScratch.h"
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

using namespace abdaudiolab;
using namespace abdaudiolab::exporting;
using namespace abdaudiolab::synth;

namespace
{

ModelEvaluation createEvaluationWithStatus(SelectionStatus status, const std::string& hash = "abcdef0123456789")
{
    ModelEvaluation eval;
    eval.decision.status = status;
    eval.decision.recommendedModelId = "wiener_hammerstein_lnl";
    eval.canonicalEvaluationHash = hash;
    eval.sampleRate = 96000.0;
    eval.metrics.rSquaredScore = (status == SelectionStatus::Rejected) ? 0.72 : 0.9985;
    eval.metrics.errorToSignalRatioDb = (status == SelectionStatus::Rejected) ? -14.2 : -48.6;
    eval.metrics.rootMeanSquareError = 0.0035;
    eval.diagnostics.peakAutocorrelation = 0.08;
    eval.uncertainty.combinedUncertaintyDb = 0.012;
    eval.validatedParameters = { "Drive", "Tone", "Level" };

    if (status == SelectionStatus::AcceptedWithWarnings)
    {
        eval.warnings.push_back("Extrapolation boundary at -3 dBFS peak");
    }
    else if (status == SelectionStatus::Rejected)
    {
        eval.warnings.push_back("ESR exceeds maximum threshold (-24.0 dB)");
    }

    return eval;
}

std::vector<MeasuredPoint> createSampleMeasuredPoints(size_t count = 4)
{
    std::vector<MeasuredPoint> pts;
    for (size_t i = 0; i < count; ++i)
    {
        MeasuredPoint p;
        p.pointId = "P_" + std::to_string(i + 1);
        p.testId = "Test_OD1";
        p.blockType = "Overdrive";
        p.stimulusType = "LogFarinaSweep";
        p.param1Normalized = static_cast<float>(i) / static_cast<float>(count);
        p.param2Normalized = 0.5f;
        p.thdPercent = 1.25f;
        p.snrDb = 85.0f;
        p.muSigmaValue = { 0.45f, 0.002f };
        p.secondaryValue = { 0.85f, 0.001f };
        p.thdValue = { 1.25f, 0.05f };
        pts.push_back(p);
    }
    return pts;
}

core::ProfilingMetadata createSampleMetadata()
{
    core::ProfilingMetadata meta;
    meta.hardwareName = "Boss OD-1 Overdrive";
    meta.targetModule = "Clipping Stage";
    meta.operatorMode = "Automated Calibration";
    meta.sampleRate = 96000.0;
    return meta;
}

} // namespace

TEST_CASE("Phase 19.6: Pre-Export Readiness Gate & Verdicts", "[export][readiness][gate]")
{
    SECTION("1. Accepted Status yields Permitted verdict")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Accepted, "hash_accepted_001");
        auto readiness = ExportReadinessEvaluator::evaluate(eval, false);

        REQUIRE(readiness.verdict == ExportGateVerdict::Permitted);
        REQUIRE(readiness.status == SelectionStatus::Accepted);
        REQUIRE(readiness.isExportPermitted == true);
        REQUIRE(readiness.requiresOperatorOverride == false);
        REQUIRE(readiness.primaryModelId == "wiener_hammerstein_lnl");
        REQUIRE(readiness.canonicalHash == "hash_accepted_001");
        REQUIRE(readiness.blockingIssues.empty());
        REQUIRE(readiness.certifiedLimits.size() >= 2);
    }

    SECTION("2. AcceptedWithWarnings Status yields PermittedWithWarning verdict")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::AcceptedWithWarnings, "hash_warn_002");
        auto readiness = ExportReadinessEvaluator::evaluate(eval, false);

        REQUIRE(readiness.verdict == ExportGateVerdict::PermittedWithWarning);
        REQUIRE(readiness.status == SelectionStatus::AcceptedWithWarnings);
        REQUIRE(readiness.isExportPermitted == true);
        REQUIRE(readiness.requiresOperatorOverride == false);
        REQUIRE(!readiness.blockingIssues.empty());
        REQUIRE(readiness.blockingIssues[0].find("Extrapolation boundary") != std::string::npos);
    }

    SECTION("3. Inconclusive Status yields Blocked verdict unless overridden")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Inconclusive, "hash_inconcl_003");

        // Without override -> blocked
        auto rBlocked = ExportReadinessEvaluator::evaluate(eval, false);
        REQUIRE(rBlocked.verdict == ExportGateVerdict::Blocked);
        REQUIRE(rBlocked.status == SelectionStatus::Inconclusive);
        REQUIRE(rBlocked.isExportPermitted == false);
        REQUIRE(rBlocked.requiresOperatorOverride == true);

        // With override -> export permitted under operator responsibility
        auto rAllowed = ExportReadinessEvaluator::evaluate(eval, true);
        REQUIRE(rAllowed.verdict == ExportGateVerdict::Blocked);
        REQUIRE(rAllowed.isExportPermitted == true);
    }

    SECTION("4. Rejected Status yields Blocked verdict unless overridden")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Rejected, "hash_rejected_004");

        auto rBlocked = ExportReadinessEvaluator::evaluate(eval, false);
        REQUIRE(rBlocked.verdict == ExportGateVerdict::Blocked);
        REQUIRE(rBlocked.status == SelectionStatus::Rejected);
        REQUIRE(rBlocked.isExportPermitted == false);
        REQUIRE(rBlocked.requiresOperatorOverride == true);

        auto rAllowed = ExportReadinessEvaluator::evaluate(eval, true);
        REQUIRE(rAllowed.verdict == ExportGateVerdict::Blocked);
        REQUIRE(rAllowed.isExportPermitted == true);
    }

    SECTION("5. InvalidMeasurement is strictly blocked, even with override")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::InvalidMeasurement, "hash_invalid_005");

        auto rBlocked = ExportReadinessEvaluator::evaluate(eval, false);
        REQUIRE(rBlocked.verdict == ExportGateVerdict::Blocked);
        REQUIRE(rBlocked.isExportPermitted == false);

        // Overriding invalid measurements must NEVER be permitted
        auto rOverride = ExportReadinessEvaluator::evaluate(eval, true);
        REQUIRE(rOverride.verdict == ExportGateVerdict::Blocked);
        REQUIRE(rOverride.isExportPermitted == false);
    }
}

TEST_CASE("Phase 19.6: C++ Preprocessor Guards & Macros Generation", "[export][cpp][guards]")
{
    SECTION("1. Permitted evaluation generates clean macros without errors or pragma warnings")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Accepted, "sha256_accepted_abc");
        auto readiness = ExportReadinessEvaluator::evaluate(eval);
        std::string guards = ExportReadinessEvaluator::generatePreprocessorGuards(readiness, 96000.0, -36.0, -3.0);

        REQUIRE(guards.find("#define ABD_PROFILE_CERTIFIED_SAMPLE_RATE 96000.0") != std::string::npos);
        REQUIRE(guards.find("#define ABD_PROFILE_MIN_INPUT_DBFS -36.0") != std::string::npos);
        REQUIRE(guards.find("#define ABD_PROFILE_MAX_INPUT_DBFS -3.0") != std::string::npos);
        REQUIRE(guards.find("#define ABD_PROFILE_STATUS \"Accepted\"") != std::string::npos);
        REQUIRE(guards.find("#define ABD_PROFILE_CANONICAL_HASH \"sha256_accepted_abc\"") != std::string::npos);
        REQUIRE(guards.find("#pragma message") == std::string::npos);
        REQUIRE(guards.find("#error") == std::string::npos);
    }

    SECTION("2. PermittedWithWarning generates #pragma message warning")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::AcceptedWithWarnings, "sha256_warn_def");
        auto readiness = ExportReadinessEvaluator::evaluate(eval);
        std::string guards = ExportReadinessEvaluator::generatePreprocessorGuards(readiness);

        REQUIRE(guards.find("#pragma message") != std::string::npos);
        REQUIRE(guards.find("[ABDAudioLab WARNING]") != std::string::npos);
        REQUIRE(guards.find("#error") == std::string::npos);
    }

    SECTION("3. Blocked status generates fatal #error compile-time guard")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Rejected, "sha256_rej_ghi");
        auto readiness = ExportReadinessEvaluator::evaluate(eval, true);
        std::string guards = ExportReadinessEvaluator::generatePreprocessorGuards(readiness);

        REQUIRE(guards.find("#ifndef ABD_ALLOW_UNVALIDATED_PROFILE") != std::string::npos);
        REQUIRE(guards.find("#error \"[ABDAudioLab FATAL]") != std::string::npos);
        REQUIRE(guards.find("Define ABD_ALLOW_UNVALIDATED_PROFILE to bypass.") != std::string::npos);
        REQUIRE(guards.find("#endif") != std::string::npos);
    }
}

TEST_CASE("Phase 19.6: Embedded JSON Metadata in C++ Header", "[export][metadata][rfc8785]")
{
    auto eval = createEvaluationWithStatus(SelectionStatus::Accepted, "sha256_test_block");
    auto readiness = ExportReadinessEvaluator::evaluate(eval);

    std::string jsonBlock = ExportReadinessEvaluator::generateEmbeddedJsonBlock(readiness, eval);
    REQUIRE(jsonBlock.find("// METADATA (RFC 8785 Traceable Verification") != std::string::npos);
    REQUIRE(jsonBlock.find("// {") != std::string::npos);
    REQUIRE(jsonBlock.find("\"schemaVersion\": \"1.0.0\"") != std::string::npos);
    REQUIRE(jsonBlock.find("\"canonicalEvaluationHash\": \"sha256_test_block\"") != std::string::npos);
    REQUIRE(jsonBlock.find("\"status\": \"Accepted\"") != std::string::npos);
}

TEST_CASE("Phase 19.6: Certified C++ Header Export and Disk Blocking", "[export][lut][certified]")
{
    auto meta = createSampleMetadata();
    auto points = createSampleMeasuredPoints(4);

    auto testFolder = abdaudiolab::test::scratchDir("ExportCertifiedHeader");

    SECTION("1. Rejected model without override is physically blocked from disk")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Rejected);
        auto targetHeader = testFolder.getChildFile("rejected_profile.h");
        if (targetHeader.existsAsFile()) targetHeader.deleteFile();

        bool ok = LutExporter::exportToCertifiedCppHeader(
            targetHeader.getFullPathName().toStdString(), meta, "rejected_table", points, eval, false);

        REQUIRE(ok == false);
        REQUIRE(targetHeader.existsAsFile() == false);
    }

    SECTION("2. Rejected model with explicit override is written with fatal #error guard")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Rejected);
        auto targetHeader = testFolder.getChildFile("rejected_override_profile.h");
        if (targetHeader.existsAsFile()) targetHeader.deleteFile();

        bool ok = LutExporter::exportToCertifiedCppHeader(
            targetHeader.getFullPathName().toStdString(), meta, "rejected_override_table", points, eval, true);

        REQUIRE(ok == true);
        REQUIRE(targetHeader.existsAsFile() == true);

        juce::String content = targetHeader.loadFileAsString();
        REQUIRE(content.contains("#ifndef ABD_ALLOW_UNVALIDATED_PROFILE"));
        REQUIRE(content.contains("#error \"[ABDAudioLab FATAL]"));
        REQUIRE(content.contains("ABD_PROFILE_STATUS \"Rejected\""));
    }

    SECTION("3. Accepted model exports cleanly with certified macros and embedded JSON metadata")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Accepted, "sha256_accepted_lut_001");
        auto targetHeader = testFolder.getChildFile("accepted_profile.h");
        if (targetHeader.existsAsFile()) targetHeader.deleteFile();

        bool ok = LutExporter::exportToCertifiedCppHeader(
            targetHeader.getFullPathName().toStdString(), meta, "od1_table", points, eval, false);

        REQUIRE(ok == true);
        REQUIRE(targetHeader.existsAsFile() == true);

        juce::String content = targetHeader.loadFileAsString();
        REQUIRE(content.contains("METADATA (RFC 8785 Traceable Verification"));
        REQUIRE(content.contains("sha256_accepted_lut_001"));
        REQUIRE(content.contains("#define ABD_PROFILE_STATUS \"Accepted\""));
        REQUIRE(content.contains("#define ABD_PROFILE_CERTIFIED_SAMPLE_RATE 96000.0"));
        REQUIRE(!content.contains("#error"));
        REQUIRE(!content.contains("#pragma message"));
        REQUIRE(content.contains("inline const alignas(16) AbdBatchedPoint od1_table[od1_table_SIZE]"));
    }

    SECTION("4. AcceptedWithWarnings model exports with #pragma message warning")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::AcceptedWithWarnings, "sha256_warn_lut_002");
        auto targetHeader = testFolder.getChildFile("warn_profile.h");
        if (targetHeader.existsAsFile()) targetHeader.deleteFile();

        bool ok = LutExporter::exportToCertifiedCppHeader(
            targetHeader.getFullPathName().toStdString(), meta, "warn_table", points, eval, false);

        REQUIRE(ok == true);
        REQUIRE(targetHeader.existsAsFile() == true);

        juce::String content = targetHeader.loadFileAsString();
        REQUIRE(content.contains("#pragma message"));
        REQUIRE(content.contains("[ABDAudioLab WARNING]"));
        REQUIRE(!content.contains("#error"));
    }
}

TEST_CASE("Phase 19.6: Certified JSON Report & Manifest Schema Conformity", "[export][json][schema]")
{
    auto meta = createSampleMetadata();
    auto points = createSampleMeasuredPoints(3);
    auto testFolder = abdaudiolab::test::scratchDir("ExportCertifiedJson");

    SECTION("1. Rejected model without override is blocked from JSON export")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Rejected);
        auto targetJson = testFolder.getChildFile("rejected_report.json");
        if (targetJson.existsAsFile()) targetJson.deleteFile();

        bool ok = LutExporter::exportToCertifiedJsonReport(
            targetJson.getFullPathName().toStdString(), meta, points, eval, false);

        REQUIRE(ok == false);
        REQUIRE(targetJson.existsAsFile() == false);
    }

    SECTION("2. Accepted model exports compliant JSON report with metrologicalCertification")
    {
        auto eval = createEvaluationWithStatus(SelectionStatus::Accepted, "sha256_json_cert_001");
        auto targetJson = testFolder.getChildFile("accepted_report.json");
        if (targetJson.existsAsFile()) targetJson.deleteFile();

        bool ok = LutExporter::exportToCertifiedJsonReport(
            targetJson.getFullPathName().toStdString(), meta, points, eval, false);

        REQUIRE(ok == true);
        REQUIRE(targetJson.existsAsFile() == true);

        juce::String content = targetJson.loadFileAsString();
        auto parsed = nlohmann::json::parse(content.toStdString());

        REQUIRE(parsed.contains("schemaVersion"));
        REQUIRE(parsed.contains("metrologicalCertification"));
        REQUIRE(parsed.contains("points"));
        REQUIRE(parsed["points"].size() == 3);

        const auto& cert = parsed["metrologicalCertification"];
        REQUIRE(cert["schemaVersion"] == "1.0.0");
        REQUIRE(cert["verdict"] == "Permitted");
        REQUIRE(cert["status"] == "Accepted");
        REQUIRE(cert["canonicalEvaluationHash"] == "sha256_json_cert_001");
        REQUIRE(cert["domainOfValidity"]["sampleRateHz"] == 96000.0);
        REQUIRE(cert["metrics"]["rSquared"] > 0.99);
        REQUIRE(cert["metrics"]["errorToSignalRatioDb"] < -40.0);
        REQUIRE(cert["metrics"]["peakAutocorrelation"] < 0.15);
        REQUIRE(cert["metrics"]["combinedUncertaintyDb"] > 0.0);
    }
}
