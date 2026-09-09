#include <catch2/catch_test_macros.hpp>
#include "export/LutExporter.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab;

TEST_CASE("LutExporter JavaScript ES6 Module Export", "[export][web][javascript]")
{
    core::ProfilingMetadata meta;
    meta.hardwareName = "Moog Sub 37";
    meta.targetModule = "Ladder VCF";
    meta.sampleRate = 48000.0;

    std::vector<exporting::MeasuredPoint> pts;
    for (int i = 0; i < 4; ++i)
    {
        exporting::MeasuredPoint p;
        p.param1Normalized = static_cast<float>(i) * 0.25f;
        p.param2Normalized = 0.5f;
        p.muSigmaValue.mean = 1000.0f + static_cast<float>(i) * 500.0f;
        p.muSigmaValue.stdDev = 15.0f;
        p.thdPercent = 0.2f;
        pts.push_back(p);
    }

    juce::File tempJs = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_lut_module.js");
    tempJs.deleteFile();

    bool ok = exporting::LutExporter::exportToJavaScriptModule(tempJs.getFullPathName().toStdString(), meta, "lut_test_filter", pts);
    REQUIRE(ok == true);
    REQUIRE(tempJs.existsAsFile());

    juce::String content = tempJs.loadFileAsString();
    REQUIRE(content.contains("Float32Array"));
    REQUIRE(content.contains("lut_test_filter_METADATA"));
    REQUIRE(content.contains("lut_test_filter_DATA"));
    REQUIRE(content.contains("Moog Sub 37"));
    REQUIRE(content.contains("export default"));

    tempJs.deleteFile();
}
