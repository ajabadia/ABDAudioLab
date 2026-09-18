#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/SessionReportManager.h"
#include "core/SessionManager.h"
#include "export/LutExporter.h"

using namespace abdaudiolab;

TEST_CASE("SessionReportManager: Report export and directory handling", "[gui][session][report_manager]")
{
    gui::SessionReportManager reportManager;
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_ReportManagerTest_" + juce::String(juce::Random::getSystemRandom().nextInt(100000)));

    if (tempDir.exists())
        tempDir.deleteRecursively();

    tempDir.createDirectory();

    core::ProfilingMetadata meta;
    meta.hardwareName = "Torcido Tube Characterizer";
    meta.targetModule = "aira_07_tube_clipper";
    meta.sampleRate = 48000.0;
    meta.timestamp = "2026-09-08T14:00:00Z";

    std::vector<exporting::MeasuredPoint> points;
    exporting::MeasuredPoint pt;
    pt.pointId = "P_001";
    pt.param1Normalized = 0.5f;
    pt.snrDb = 42.0f;
    pt.thdPercent = 0.05f;
    points.push_back(pt);

    SECTION("Exports LUT and JSON report files")
    {
        bool ok = reportManager.exportLutAndJsonReports(tempDir, "Torcido_Test", meta, points);
        REQUIRE(ok);

        juce::File lutFile = tempDir.getChildFile("Torcido_Test_LUT.h");
        juce::File jsonFile = tempDir.getChildFile("Torcido_Test_Report.json");

        REQUIRE(lutFile.existsAsFile());
        REQUIRE(jsonFile.existsAsFile());
        REQUIRE(lutFile.getSize() > 0);
        REQUIRE(jsonFile.getSize() > 0);
    }

    SECTION("Exports HTML Certification Report")
    {
        exporting::SessionManifestData manifest;
        manifest.hardwareId = "aira_07_tube_clipper";
        manifest.hardwareName = "Roland AIRA: Torcido";
        manifest.functionId = "tube_warmth";
        manifest.functionName = "Vacuum Tube Warmth & Clipper";
        manifest.deviceType = "AUTOMATED_SYSEX";
        manifest.sampleRate = 48000.0;

        juce::File outHtml;
        bool ok = reportManager.exportCertificationHtmlReport(tempDir, "Torcido_Cert", manifest, points, outHtml);
        REQUIRE(ok);
        REQUIRE(outHtml.existsAsFile());
        REQUIRE(outHtml.getSize() > 0);
    }

    SECTION("launchHtmlReportInDefaultViewer rejects non-existent file safely without launching process")
    {
        juce::File nonExistent = tempDir.getChildFile("does_not_exist_report.html");
        juce::String err;
        bool ok = gui::SessionReportManager::launchHtmlReportInDefaultViewer(nonExistent, err);
        REQUIRE_FALSE(ok);
        REQUIRE(err.isNotEmpty());
        REQUIRE(err.contains("no existe"));
    }

    SECTION("Characterization: exportReport with HtmlCertification")
    {
        gui::ReportExportRequest req;
        req.format = gui::ReportFormat::HtmlCertification;
        req.destination = tempDir;
        req.baseName = "Test_Characterization";
        req.hardwareId = "aira_07_tube_clipper";
        req.hardwareName = "Roland AIRA: Torcido";
        req.functionId = "tube_warmth";
        req.functionName = "Vacuum Tube Warmth & Clipper";
        req.sampleRate = 48000.0;

        auto result = reportManager.exportReport(req, points);
        REQUIRE(result.succeeded);
        REQUIRE(result.status == gui::ExportStatus::Succeeded);
        REQUIRE(result.outputPath.existsAsFile());
        REQUIRE(result.outputPath.getFileName() == "Test_Characterization_Certification_Report.html");
        REQUIRE(result.outputPath.getSize() > 0);
        REQUIRE(result.artifactPaths.size() == 1);
    }

    SECTION("Characterization: exportReport with ProductionPackage uses atomic staging and computes SHA-256 fixity")
    {
        gui::ReportExportRequest req;
        req.format = gui::ReportFormat::ProductionPackage;
        req.destination = tempDir;
        req.baseName = "Production_Seam1_Test";
        req.hardwareId = "dexed_vst3";
        req.hardwareName = "Dexed FM Synthesizer";
        req.functionId = "algorithm_01";
        req.functionName = "DX7 Alg 1 FM Matrix";
        req.sampleRate = 44100.0;
        req.ambientTemperatureC = 22.5;
        req.warmupTimeMinutes = 20.0;
        req.operatorNotes = "Characterization audit test";

        auto result = reportManager.exportReport(req, points);
        REQUIRE(result.succeeded);
        REQUIRE(result.status == gui::ExportStatus::Succeeded);
        REQUIRE(result.artifactPaths.size() == 4);

        // Verify exact naming conventions
        juce::File lutFile = tempDir.getChildFile("Production_Seam1_Test_lut.h");
        juce::File jsonFile = tempDir.getChildFile("Production_Seam1_Test_telemetry.json");
        juce::File htmlFile = tempDir.getChildFile("Production_Seam1_Test_Certification_Report.html");
        juce::File manifestFile = tempDir.getChildFile("Production_Seam1_Test_manifest.json");

        REQUIRE(lutFile.existsAsFile());
        REQUIRE(jsonFile.existsAsFile());
        REQUIRE(htmlFile.existsAsFile());
        REQUIRE(manifestFile.existsAsFile());

        REQUIRE(lutFile.getSize() > 0);
        REQUIRE(jsonFile.getSize() > 0);
        REQUIRE(htmlFile.getSize() > 0);
        REQUIRE(manifestFile.getSize() > 0);

        // Fixity SHA-256 must be a valid 64-character hexadecimal digest
        REQUIRE(result.manifestSha256.length() == 64);
        for (char c : result.manifestSha256)
        {
            REQUIRE(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
        }

        // Verify staging directory was cleaned up and does not linger
        auto stagingDirs = tempDir.findChildFiles(juce::File::findDirectories, false, ".staging_*");
        REQUIRE(stagingDirs.isEmpty());
    }

    SECTION("Characterization: exportReport handles invalid destination gracefully")
    {
        gui::ReportExportRequest req;
        req.destination = juce::File(); // empty/invalid
        auto result = reportManager.exportReport(req, points);
        REQUIRE_FALSE(result.succeeded);
        REQUIRE(result.status == gui::ExportStatus::ValidationFailed);
        REQUIRE(result.errorCode == "ERR_NO_DESTINATION");
    }

    SECTION("Characterization: buildAuditionLutGrid dimensions and value constraints")
    {
        // 1. With empty points: baseline deterministic curve
        auto emptyLut = gui::SessionReportManager::buildAuditionLutGrid({}, 8);
        REQUIRE(emptyLut.size() == 64);
        for (const auto& cell : emptyLut)
        {
            REQUIRE(cell.mu >= 0.05f);
            REQUIRE(cell.mu <= 1.0f);
            REQUIRE(cell.sigma == 0.0f);
        }

        // 2. With measured points: gain & distortion mapping
        std::vector<exporting::MeasuredPoint> testPts;
        for (int i = 0; i < 4; ++i)
        {
            exporting::MeasuredPoint p;
            p.secondaryValue.mean = -6.0; // ~0.501 gain
            p.thdPercent = 1.5f;          // 0.015 sigma
            testPts.push_back(p);
        }
        auto measuredLut = gui::SessionReportManager::buildAuditionLutGrid(testPts, 8);
        REQUIRE(measuredLut.size() == 64);
        for (const auto& cell : measuredLut)
        {
            REQUIRE(cell.mu >= 0.01f);
            REQUIRE(cell.mu <= 1.0f);
            REQUIRE(cell.sigma == Catch::Approx(0.015f));
        }
    }

    SECTION("Characterization: calculateSessionMetrics math accuracy")
    {
        // Empty dataset fallback defaults
        float avgSnr = 0.0f, noiseFloor = 0.0f, avgThd = 0.0f, durationSec = 0.0f;
        int count = 0;
        gui::SessionReportManager::calculateSessionMetrics({}, 0.0f, avgSnr, noiseFloor, avgThd, count, durationSec);
        REQUIRE(count == 0);
        REQUIRE(avgSnr == Catch::Approx(38.5f));
        REQUIRE(avgThd == Catch::Approx(0.015f));
        REQUIRE(noiseFloor == Catch::Approx(-90.0f));
        REQUIRE(durationSec == Catch::Approx(10.0f));

        // Populated dataset
        std::vector<exporting::MeasuredPoint> dataset;
        exporting::MeasuredPoint p1, p2;
        p1.snrDb = 50.0f; p1.thdPercent = 0.02f;
        p2.snrDb = 40.0f; p2.thdPercent = 0.04f;
        dataset.push_back(p1);
        dataset.push_back(p2);

        gui::SessionReportManager::calculateSessionMetrics(dataset, 0.5f, avgSnr, noiseFloor, avgThd, count, durationSec);
        REQUIRE(count == 2);
        REQUIRE(avgSnr == Catch::Approx(45.0f));
        REQUIRE(avgThd == Catch::Approx(0.03f));
        REQUIRE(noiseFloor == Catch::Approx(-84.2f)); // with inputTrim > 1e-4
        REQUIRE(durationSec == Catch::Approx(5.0f));  // 2 * 2.5s
    }

    // Cleanup
    tempDir.deleteRecursively();
}
