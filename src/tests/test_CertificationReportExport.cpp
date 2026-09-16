/**
 * @file test_CertificationReportExport.cpp
 * @brief Unit and integration tests for CertificationReportExporter, ValidationUiSummary,
 *        and transactional staging integrity (Phase 20.8.6 T1).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "export/CertificationReportExporter.h"
#include "core/ValidationUiSummary.h"
#include "core/ModelHoldoutValidator.h"
#include "core/ExperimentStorage.h"
#include "core/LabDataDirectories.h"

namespace
{

abdaudiolab::core::ExperimentRecord createTestExperiment(const std::string& expId)
{
    abdaudiolab::core::ExperimentRecord rec;
    rec.schemaVersion = 1;
    rec.experimentId = expId;
    rec.status = abdaudiolab::core::ExperimentStatus::AuditedApproved;

    rec.target.targetId = "ReferenceSynth";
    rec.target.targetName = "Reference Ground Truth Synth VST3";
    rec.target.manufacturer = "ABDSynths";
    rec.target.version = "1.0.0";
    rec.target.format = "VST3";
    rec.target.isDeterministic = true;

    rec.capture.sampleRate = 48000.0;
    rec.capture.hostBufferSize = 480;
    rec.capture.processingBlockSize = 256;
    rec.capture.channels = 1;
    rec.capture.durationSeconds = 1.0;

    rec.provenance.timestampUtc = "2026-09-16T08:00:00Z";
    rec.provenance.executionMode = "InProcess";

    rec.evaluation.hasEvaluation = true;
    rec.evaluation.recommendedModelType = "AnalogLutFilterModule";
    rec.evaluation.selectionStatus = "Accepted";
    rec.evaluation.canonicalEvaluationHash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    rec.evaluation.validationEsrDb = -38.7;
    rec.evaluation.validationCorrelation = 0.9934;
    rec.evaluation.criteriaCompliancePercent = 100.0;
    rec.evaluation.hashVerified = true;

    return rec;
}

} // namespace

TEST_CASE("CertificationReportExporter: Holdout validation HTML and status badges", "[export][html_report]")
{
    juce::File tempDir = juce::File::createTempFile("test_report_html");
    tempDir.deleteFile();
    tempDir.createDirectory();

    abdaudiolab::exporting::SessionManifestData manifest;
    manifest.hardwareName = "Moog Sub 37 Filter Profile";
    manifest.sampleRate = 48000.0;
    manifest.averageSnrDb = 95.5f;
    manifest.noiseFloorRmsDb = -90.2f;

    std::vector<abdaudiolab::exporting::MeasuredPoint> points;
    abdaudiolab::exporting::MeasuredPoint pt;
    pt.muSigmaValue.mean = -6.0f;
    points.push_back(pt);

    SECTION("Scenario 1: Validation PASS with audio-ab-v1")
    {
        abdaudiolab::core::ValidationReport val;
        val.verdict = "PASS";
        val.verdictPolicy = "audio-ab-v1";
        val.reasonCode = "WITHIN_TOLERANCE";
        val.sampleOffset = 37;
        val.postAlignment.esrDb = -35.2f;
        val.postAlignment.correlationPeak = 0.9945f;
        val.preAlignment.rmse = 0.015f;
        val.postAlignment.rmse = 0.0012f;
        val.targetWavSha256 = "1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff";

        juce::File htmlFile = tempDir.getChildFile("report_pass.html");
        bool ok = abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifest,
            points,
            &val,
            "completed"
        );

        REQUIRE(ok);
        REQUIRE(htmlFile.existsAsFile());

        juce::String content = htmlFile.loadFileAsString();
        CHECK(content.contains("badge-pass"));
        CHECK(content.contains("VERDICT: PASS"));
        CHECK(content.contains("audio-ab-v1"));
        CHECK(content.contains("alignedTarget[n] = target[n - sampleOffset]"));
        CHECK(content.contains("-35.2 dB"));
        CHECK(content.contains("0.9945"));
        CHECK(content.contains("37 smp"));
    }

    SECTION("Scenario 2: Validation PASS_WITH_LIMITATIONS")
    {
        abdaudiolab::core::ValidationReport val;
        val.verdict = "PASS_WITH_LIMITATIONS";
        val.verdictPolicy = "audio-ab-v1";
        val.reasonCode = "MARGINAL_TOLERANCE";
        val.sampleOffset = 0;
        val.postAlignment.esrDb = -22.4f;
        val.postAlignment.correlationPeak = 0.9412f;

        juce::File htmlFile = tempDir.getChildFile("report_lim.html");
        bool ok = abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifest,
            points,
            &val,
            "completed"
        );

        REQUIRE(ok);
        juce::String content = htmlFile.loadFileAsString();
        CHECK(content.contains("badge-warn"));
        CHECK(content.contains("PASS WITH LIMITATIONS"));
    }

    SECTION("Scenario 3: Validation FAIL")
    {
        abdaudiolab::core::ValidationReport val;
        val.verdict = "FAIL";
        val.verdictPolicy = "audio-ab-v1";
        val.reasonCode = "EXCEEDS_TOLERANCE";
        val.sampleOffset = -15;
        val.postAlignment.esrDb = -12.1f;
        val.postAlignment.correlationPeak = 0.812f;

        juce::File htmlFile = tempDir.getChildFile("report_fail.html");
        bool ok = abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifest,
            points,
            &val,
            "completed"
        );

        REQUIRE(ok);
        juce::String content = htmlFile.loadFileAsString();
        CHECK(content.contains("badge-fail"));
        CHECK(content.contains("FAIL"));
    }

    SECTION("Scenario 4: Technical ERROR")
    {
        juce::File htmlFile = tempDir.getChildFile("report_err.html");
        bool ok = abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifest,
            points,
            nullptr,
            "error",
            "Named pipe IPC communication severed during sweep"
        );

        REQUIRE(ok);
        juce::String content = htmlFile.loadFileAsString();
        CHECK(content.contains("badge-error"));
        CHECK(content.contains("TECHNICAL ERROR"));
        CHECK(content.contains("Named pipe IPC communication severed"));
    }

    SECTION("Scenario 5: NOT_EXECUTED")
    {
        juce::File htmlFile = tempDir.getChildFile("report_not_exec.html");
        bool ok = abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifest,
            points,
            nullptr,
            "notExecuted"
        );

        REQUIRE(ok);
        juce::String content = htmlFile.loadFileAsString();
        CHECK(content.contains("badge-neutral"));
        CHECK(content.contains("NOT EXECUTED"));
    }

    SECTION("Scenario 6: Metrological Guardian - Synthetic placeholder without holdout is rejected")
    {
        abdaudiolab::core::ValidationReport dummyVal;
        dummyVal.verdict = "PASS_WITH_LIMITATIONS";
        dummyVal.postAlignment.esrDb = -120.0f;
        dummyVal.postAlignment.rmse = 0.0f;
        dummyVal.postAlignment.correlationPeak = 1.0f;

        juce::File htmlFile = tempDir.getChildFile("report_guardian.html");
        bool ok = abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifest,
            {}, // 0 measurement points
            &dummyVal,
            "completed"
        );

        REQUIRE(ok);
        juce::String content = htmlFile.loadFileAsString();
        CHECK(content.contains("NOT EXECUTED"));
        CHECK_FALSE(content.contains("PASS_WITH_LIMITATIONS"));
        CHECK_FALSE(content.contains("-120.0 dB"));
    }

    tempDir.deleteRecursively();
}

TEST_CASE("ValidationUiSummary and Staging Transactionality: FAIR integrity and Tampering", "[export][validation_summary]")
{
    juce::File baseDir = juce::File::createTempFile("test_storage_root");
    baseDir.deleteFile();
    baseDir.createDirectory();

    std::string expId = "20260916T080000Z_ReportAuditTest";
    auto rec = createTestExperiment(expId);

    SECTION("Transactional export publishes reports/certification_report.html and validation artifacts")
    {
        auto stagingHook = [](const juce::File& stagingDir, juce::String& /*stageErr*/) -> bool {
            // 1. Crear validation artifacts
            juce::File valDir = stagingDir.getChildFile("validation");
            valDir.createDirectory();

            juce::File tgtWav = valDir.getChildFile("target.wav");
            juce::File mdlWav = valDir.getChildFile("model.wav");
            juce::File resWav = valDir.getChildFile("residual.wav");
            juce::File repJsonFile = valDir.getChildFile("validation_report.json");

            tgtWav.replaceWithText("MOCK_TARGET_AUDIO_DATA_48KHZ");
            mdlWav.replaceWithText("MOCK_MODEL_AUDIO_DATA_48KHZ");
            resWav.replaceWithText("MOCK_RESIDUAL_AUDIO_DATA_48KHZ");

            abdaudiolab::core::ValidationReport rep;
            rep.schemaVersion = "audio-validation-report-1.0";
            rep.schemaUri = "urn:abdaudio:audio-validation-report:1.0";
            rep.reportId = "holdout-val-test-01";
            rep.status = "completed";
            rep.verdict = "PASS";
            rep.verdictPolicy = "audio-ab-v1";
            rep.reasonCode = "WITHIN_TOLERANCE";
            rep.sampleOffset = 42;
            rep.postAlignment.esrDb = -38.5f;
            rep.postAlignment.correlationPeak = 0.998f;
            rep.targetWavSha256 = abdaudiolab::core::ExperimentStorage::computeFileSha256(tgtWav);
            rep.modelWavSha256 = abdaudiolab::core::ExperimentStorage::computeFileSha256(mdlWav);
            rep.residualWavSha256 = abdaudiolab::core::ExperimentStorage::computeFileSha256(resWav);

            repJsonFile.replaceWithText(rep.toJson().dump(2));

            // 2. Crear reports/certification_report.html
            juce::File repDir = stagingDir.getChildFile("reports");
            repDir.createDirectory();
            juce::File htmlFile = repDir.getChildFile("certification_report.html");

            abdaudiolab::exporting::SessionManifestData m;
            m.hardwareName = "Reference Ground Truth Synth VST3";
            m.sampleRate = 48000.0;
            m.averageSnrDb = 98.4f;
            m.noiseFloorRmsDb = -92.1f;

            std::vector<abdaudiolab::exporting::MeasuredPoint> pts;
            return abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
                htmlFile.getFullPathName().toStdString(),
                m,
                pts,
                &rep,
                "completed"
            );
        };

        juce::String err;
        bool ok = abdaudiolab::core::ExperimentStorage::saveExperiment(baseDir, rec, {}, err, std::nullopt, stagingHook);
        REQUIRE(ok);

        juce::File finalExpFolder = baseDir.getChildFile(expId);
        REQUIRE(finalExpFolder.isDirectory());

        // Verificar que manifest.json contiene role audit_report_html
        juce::File manifestFile = finalExpFolder.getChildFile("manifest.json");
        REQUIRE(manifestFile.existsAsFile());
        nlohmann::json man = nlohmann::json::parse(manifestFile.loadFileAsString().toStdString());

        bool foundHtmlArtifact = false;
        bool foundValReport = false;
        for (const auto& item : man["artifacts"])
        {
            if (item.value("path", "") == "reports/certification_report.html")
            {
                foundHtmlArtifact = true;
                CHECK(item.value("role", "") == "audit_report_html");
                CHECK(!item.value("sha256", "").empty());
            }
            if (item.value("path", "") == "validation/validation_report.json")
            {
                foundValReport = true;
                CHECK(item.value("role", "") == "validation_report");
            }
        }
        CHECK(foundHtmlArtifact);
        CHECK(foundValReport);

        // Parsear con ValidationUiSummary::fromExperimentFolder
        auto summary = abdaudiolab::core::ValidationUiSummary::fromExperimentFolder(finalExpFolder);
        CHECK(summary.integrityVerified == true);
        CHECK(summary.status == abdaudiolab::core::ValidationUiSummary::Status::completed);
        CHECK(summary.verdict == abdaudiolab::core::ValidationUiSummary::Verdict::pass);
        CHECK(summary.sampleOffset == 42);
        CHECK_THAT(summary.esrDb, Catch::Matchers::WithinAbs(-38.5, 0.01));
        CHECK_THAT(summary.correlation, Catch::Matchers::WithinAbs(0.998, 0.001));
        CHECK(summary.targetAvailable == true);
        CHECK(summary.modelAvailable == true);
        CHECK(summary.residualAvailable == true);
        CHECK(summary.htmlReportAvailable == true);

        // Tamper test: Corromper 1 byte en certification_report.html
        juce::File canonicalHtml = finalExpFolder.getChildFile("reports").getChildFile("certification_report.html");
        canonicalHtml.appendText("<!-- TAMPERED_BYTE -->");

        auto tamperedSummary = abdaudiolab::core::ValidationUiSummary::fromExperimentFolder(finalExpFolder);
        CHECK(tamperedSummary.integrityVerified == false);
        CHECK(tamperedSummary.status == abdaudiolab::core::ValidationUiSummary::Status::corrupt);
        CHECK(tamperedSummary.verdict == abdaudiolab::core::ValidationUiSummary::Verdict::notAvailable);
    }

    SECTION("Staging hook failure cleanly rolls back with zero partial artifacts published")
    {
        std::string failExpId = "20260916T080000Z_RollbackTest";
        auto failRec = createTestExperiment(failExpId);

        auto failingHook = [](const juce::File&, juce::String& stageErr) -> bool {
            stageErr = "Simulated render pipeline timeout";
            return false;
        };

        juce::String err;
        bool ok = abdaudiolab::core::ExperimentStorage::saveExperiment(baseDir, failRec, {}, err, std::nullopt, failingHook);
        CHECK(!ok);
        CHECK(err.contains("Simulated render pipeline timeout"));

        // El directorio final no debe existir
        juce::File destDir = baseDir.getChildFile(failExpId);
        CHECK(!destDir.exists());

        // Ninguna carpeta temporal (.tmp_*) debe quedar huérfana
        juce::Array<juce::File> tmpDirs = baseDir.findChildFiles(juce::File::findDirectories, false, ".*.tmp_*");
        CHECK(tmpDirs.isEmpty());
    }

    baseDir.deleteRecursively();
}
