#include <catch2/catch_test_macros.hpp>
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

    // Cleanup
    tempDir.deleteRecursively();
}
