#include <catch2/catch_test_macros.hpp>
#include "export/CertificationReportExporter.h"
#include <juce_core/juce_core.h>

TEST_CASE("CertificationReportExporter HTML & SVG Generation", "[export][report]")
{
    abdaudiolab::exporting::SessionManifestData manifest;
    manifest.hardwareName = "TEST_SYNTH_HARDWARE";
    manifest.sampleRate = 96000.0;
    manifest.averageSnrDb = 88.5f;
    manifest.noiseFloorRmsDb = -92.0f;

    std::vector<abdaudiolab::exporting::MeasuredPoint> points;
    for (int i = 0; i < 16; ++i)
    {
        abdaudiolab::exporting::MeasuredPoint p;
        p.pointId = "P_" + std::to_string(i + 1);
        p.blockType = "AnalogFilter";
        p.stimulusType = "LogFarinaSweep";
        p.param1Normalized = static_cast<float>(i) / 15.0f;
        p.param2Normalized = 0.5f;
        p.thdPercent = 0.05f + static_cast<float>(i) * 0.01f;
        p.snrDb = 85.0f + static_cast<float>(i) * 0.2f;
        points.push_back(p);
    }

    juce::File tempHtmlFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("test_certification_report.html");
    if (tempHtmlFile.existsAsFile())
        tempHtmlFile.deleteFile();

    bool exportOk = abdaudiolab::exporting::CertificationReportExporter::exportReportToHtml(
        tempHtmlFile.getFullPathName().toStdString(), manifest, points);

    REQUIRE(exportOk);
    REQUIRE(tempHtmlFile.existsAsFile());
    REQUIRE(tempHtmlFile.getSize() > 1000);

    juce::String htmlContent = tempHtmlFile.loadFileAsString();
    REQUIRE(htmlContent.contains("ABDAUDIOLAB CERTIFICATION REPORT"));
    REQUIRE(htmlContent.contains("TEST_SYNTH_HARDWARE"));
    REQUIRE(htmlContent.contains("<svg"));
    REQUIRE(htmlContent.contains("FREQUENCY RESPONSE MAGNITUDE"));
    REQUIRE(htmlContent.contains("2D PARAMETER MATRIX HEATMAP"));

    tempHtmlFile.deleteFile();
}
