/**
 * @file test_Phase20_11_7_ComplexEnvelopes_T5.cpp
 * @brief Catch2 unit tests for Phase 20.11.7 Increment 5:
 *        FAIR/LNL Container Exporter, Safe Multi-Series SVG Renderer,
 *        and Offline-Safe Interactive 8-Stage HTML Report Generator.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "measurement/ComplexEnvelopeExportContracts.h"
#include "measurement/ComplexEnvelopeSvgRenderer.h"
#include "measurement/ComplexEnvelopeHtmlReportGenerator.h"
#include "measurement/ComplexEnvelopeFairExporter.h"
#include "measurement/ComplexEnvelopeOrchestrator.h"
#include "measurement/adapters/casio/CasioCz101NativeStateProvider.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <string>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::measurement::adapters::casio;

namespace
{

ComplexEnvelopeOrchestrationResult createDummyOrchestrationResult()
{
    ComplexEnvelopeOrchestrationResult res;
    res.orchestrationId = "orch_test_001";
    res.status = "success";
    res.sourceRawAudioSha256 = "11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff";
    res.compensatedAudioSha256 = "99887766554433221100ffeeddccbbaa99887766554433221100ffeeddccbbaa";

    res.timingResolution.type = TimingReferenceType::ProvidedEvent;
    res.timingResolution.offsetSamples = 2400;
    res.timingResolution.confidence = 1.0;
    res.timingResolution.ambiguityMargin = 1.0;
    res.timingResolution.peakRatio = 0.0;
    res.timingResolution.status = "resolved";

    res.captureRecord.temporalGrid.frameCount = 10;
    res.captureRecord.temporalGrid.hopMs = 20.0;
    res.captureRecord.totalDurationMs = 200.0;

    for (int i = 0; i < 10; ++i)
    {
        double t = i * 20.0;
        EnvelopeObservationPoint pt;
        pt.frameIndex = i;
        pt.timeMs = t;
        pt.value = 0.1 + 0.08 * i;
        pt.status = (i == 5) ? "silence" : "valid"; // Introduce deliberate gap at i = 5
        res.captureRecord.timbreTrajectory.points.push_back(pt);

        EnvelopeObservationPoint ptAmp = pt;
        ptAmp.value = 0.2 + 0.07 * i;
        res.captureRecord.amplitudeTrajectory.points.push_back(ptAmp);

        EnvelopeObservationPoint ptPitch = pt;
        ptPitch.value = 0.5;
        res.captureRecord.pitchTrajectory.points.push_back(ptPitch);
    }

    ObservableComparisonReport comp;
    comp.observableDomain = "Timbre";
    comp.nativeParameterPath = "line1.dcw.envelope";
    comp.comparisonStatus = "compared";
    comp.comparisonSpace = "normalized_0_1";
    comp.trajectoryRmse = 0.03;
    comp.correlation = 0.98;
    comp.meanAbsoluteError = 0.025;
    comp.coverageRatio = 1.0;
    comp.comparisonLabel = "observable_agreement";
    comp.phaseDistortionProxy = "not_claimed";
    res.comparisons.push_back(comp);

    return res;
}

std::vector<EnvelopeStageDescriptor> createDummyStages()
{
    std::vector<EnvelopeStageDescriptor> stages;
    for (int i = 1; i <= 8; ++i)
    {
        EnvelopeStageDescriptor st;
        st.stageIndex = i;
        st.rateOrSlope = 50.0 + i * 5.0;
        st.targetLevel = (i <= 4) ? (i * 24.0) : (100.0 - i * 10.0);
        st.durationMs = 25.0;
        st.isSustainPoint = (i == 4);
        st.isEndKeyOnPoint = (i == 8);
        stages.push_back(st);
    }
    return stages;
}

} // anonymous namespace

TEST_CASE("Phase 20.11.7 T5: Safe & Deterministic Multi-Series SVG Renderer",
          "[complex_envelopes][fair_export][svg_renderer]")
{
    auto orchResult = createDummyOrchestrationResult();
    auto stages = createDummyStages();

    SECTION("Render output is 100% byte-identical across repeated runs")
    {
        std::string svg1 = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
            orchResult.captureRecord, orchResult.comparisons, stages, EnvelopeDomain::Timbre);
        std::string svg2 = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
            orchResult.captureRecord, orchResult.comparisons, stages, EnvelopeDomain::Timbre);

        CHECK(svg1 == svg2);
        CHECK_FALSE(svg1.empty());
    }

    SECTION("Strictly passes SVG safety validation without scripts, remote hrefs, or invalid numbers")
    {
        std::string svg = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
            orchResult.captureRecord, orchResult.comparisons, stages, EnvelopeDomain::Timbre);

        std::string outError;
        bool valid = ComplexEnvelopeSvgRenderer::validateSvgSafety(svg, outError);
        INFO("Safety validation error: " << outError);
        CHECK(valid);
        CHECK(outError.empty());

        // Explicit security checks
        CHECK(svg.find("<script") == std::string::npos);
        CHECK(svg.find("href=\"http") == std::string::npos);
        CHECK(svg.find("href=\"https") == std::string::npos);
        CHECK(svg.find("foreignObject") == std::string::npos);
        CHECK(svg.find("onclick") == std::string::npos);
        CHECK(svg.find("url(") == std::string::npos);
        CHECK(svg.find("NaN") == std::string::npos);
        CHECK(svg.find("Infinity") == std::string::npos);
    }

    SECTION("Safety validator catches simulated malicious or invalid SVG elements")
    {
        std::string err;
        CHECK_FALSE(ComplexEnvelopeSvgRenderer::validateSvgSafety("<svg><script>alert(1)</script></svg>", err));
        CHECK_FALSE(ComplexEnvelopeSvgRenderer::validateSvgSafety("<svg><a href=\"http://evil.com\">click</a></svg>", err));
        CHECK_FALSE(ComplexEnvelopeSvgRenderer::validateSvgSafety("<svg><rect onclick=\"run()\"/></svg>", err));
        CHECK_FALSE(ComplexEnvelopeSvgRenderer::validateSvgSafety("<svg><circle r=\"NaN\"/></svg>", err));
        CHECK_FALSE(ComplexEnvelopeSvgRenderer::validateSvgSafety("<svg><circle r=\"Infinity\"/></svg>", err));
    }

    SECTION("Handles silent/unreliable intervals by breaking the path (no false zero fall)")
    {
        std::string svg = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
            orchResult.captureRecord, orchResult.comparisons, stages, EnvelopeDomain::Timbre);

        // Path should have at least two Move-To 'M' commands due to the gap at frame index 5
        size_t firstM = svg.find(" M ");
        CHECK(firstM != std::string::npos);
        size_t secondM = svg.find(" M ", firstM + 3);
        CHECK(secondM != std::string::npos); // Verified path was broken, not drawn to zero
    }

    SECTION("Displays mandatory honest metrological label and notice")
    {
        std::string svg = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
            orchResult.captureRecord, orchResult.comparisons, stages, EnvelopeDomain::Timbre);

        CHECK(svg.find("observable_agreement") != std::string::npos);
        CHECK(svg.find("phaseDistortionProxy: not_claimed") != std::string::npos);
    }
}

TEST_CASE("Phase 20.11.7 T5: Offline-Safe Interactive HTML Report Generator",
          "[complex_envelopes][fair_export][html_generator]")
{
    auto orchResult = createDummyOrchestrationResult();
    auto stages = createDummyStages();
    ComplexEnvelopeExportSpec spec;
    spec.timestampPolicy = TimestampPolicy::FixedForTest;

    SECTION("Report generation is 100% byte-identical across repeated runs")
    {
        std::string html1 = ComplexEnvelopeHtmlReportGenerator::generateInteractiveReportHtml(
            orchResult, spec, stages, "../audio/raw.wav", "../audio/comp.wav");
        std::string html2 = ComplexEnvelopeHtmlReportGenerator::generateInteractiveReportHtml(
            orchResult, spec, stages, "../audio/raw.wav", "../audio/comp.wav");

        CHECK(html1 == html2);
        CHECK_FALSE(html1.empty());
    }

    SECTION("Offline security: Zero external networks, strict CSP meta tag")
    {
        std::string html = ComplexEnvelopeHtmlReportGenerator::generateInteractiveReportHtml(
            orchResult, spec, stages, "../audio/raw.wav", "../audio/comp.wav");

        // Enforce strict CSP meta header
        CHECK(html.find("Content-Security-Policy") != std::string::npos);
        CHECK(html.find("default-src 'none'") != std::string::npos);

        // Forbid remote network calls and remote URLs
        CHECK(html.find("href=\"http") == std::string::npos);
        CHECK(html.find("href=\"https") == std::string::npos);
        CHECK(html.find("src=\"http") == std::string::npos);
        CHECK(html.find("src=\"https") == std::string::npos);
        CHECK(html.find("fetch(") == std::string::npos);
        CHECK(html.find("XMLHttpRequest") == std::string::npos);
        CHECK(html.find("WebSocket") == std::string::npos);
    }

    SECTION("Contains interactive multi-domain tabs and honest metrological disclosures")
    {
        std::string html = ComplexEnvelopeHtmlReportGenerator::generateInteractiveReportHtml(
            orchResult, spec, stages, "../audio/raw.wav", "../audio/comp.wav");

        CHECK(html.find("Timbre (DCW)") != std::string::npos);
        CHECK(html.find("Pitch (DCO)") != std::string::npos);
        CHECK(html.find("Amplitude (DCA)") != std::string::npos);
        CHECK(html.find("observable_agreement") != std::string::npos);
        CHECK(html.find("phaseDistortionProxy: not_claimed") != std::string::npos);
        CHECK(html.find("Acoustic spectral centroid and high-frequency rolloff serve as physically observable proxies") != std::string::npos);
    }
}

TEST_CASE("Phase 20.11.7 T5: Transactional FAIR/LNL Container Export & Rollback",
          "[complex_envelopes][fair_export][container_exporter]")
{
    auto orchResult = createDummyOrchestrationResult();
    auto stages = createDummyStages();

    std::vector<float> rawAudio(4800, 0.2f);
    std::vector<float> compAudio(4800, 0.2f);

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_T5_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));

    auto cleanup = [&]() {
        if (tempDir.exists()) tempDir.deleteRecursively();
        juce::File stagingDir = tempDir.getParentDirectory().getChildFile(tempDir.getFileName() + "_staging");
        if (stagingDir.exists()) stagingDir.deleteRecursively();
    };

    cleanup();

    SECTION("Exports complete valid container with manifest and verifies non-circular hashes")
    {
        ComplexEnvelopeExportSpec spec;
        spec.experimentId = "test_cz101_env";
        spec.timestampPolicy = TimestampPolicy::FixedForTest;

        juce::String outError;
        bool ok = ComplexEnvelopeFairExporter::exportContainer(
            tempDir, orchResult, spec, stages, rawAudio, compAudio, outError);

        INFO("ExportContainer failed: " << outError.toStdString());
        REQUIRE(ok);
        CHECK(tempDir.exists());
        CHECK(tempDir.isDirectory());

        // Check required files exist
        CHECK(tempDir.getChildFile("manifest.json").existsAsFile());
        CHECK(tempDir.getChildFile("spec.json").existsAsFile());
        CHECK(tempDir.getChildFile("data/envelope_record.json").existsAsFile());
        CHECK(tempDir.getChildFile("data/comparison_report.json").existsAsFile());
        CHECK(tempDir.getChildFile("audio/raw_capture.wav").existsAsFile());
        CHECK(tempDir.getChildFile("audio/compensated_capture.wav").existsAsFile());
        CHECK(tempDir.getChildFile("reports/complex_envelope_overlay.svg").existsAsFile());
        CHECK(tempDir.getChildFile("reports/envelope_report.html").existsAsFile());

        // Audit integrity
        juce::String auditError;
        bool valid = ComplexEnvelopeFairExporter::validateContainerIntegrity(tempDir, auditError);
        INFO("Validation error: " << auditError.toStdString());
        CHECK(valid);

        // Verify non-circularity: manifest.json is NOT in artifacts list
        auto manifestJson = nlohmann::json::parse(tempDir.getChildFile("manifest.json").loadFileAsString().toStdString());
        REQUIRE(manifestJson.contains("artifacts"));
        for (const auto& art : manifestJson["artifacts"])
        {
            CHECK(art["relativePath"] != "manifest.json");
        }
    }

    SECTION("Integrity validator detects tampered files")
    {
        ComplexEnvelopeExportSpec spec;
        spec.timestampPolicy = TimestampPolicy::FixedForTest;
        juce::String outError;
        REQUIRE(ComplexEnvelopeFairExporter::exportContainer(
            tempDir, orchResult, spec, stages, rawAudio, compAudio, outError));

        // Tamper with spec.json
        juce::File specFile = tempDir.getChildFile("spec.json");
        specFile.appendText("/* corrupted */");

        juce::String auditError;
        bool valid = ComplexEnvelopeFairExporter::validateContainerIntegrity(tempDir, auditError);
        CHECK_FALSE(valid);
        CHECK(auditError.contains("mismatch"));
    }

    SECTION("Integrity validator detects missing artifacts")
    {
        ComplexEnvelopeExportSpec spec;
        spec.timestampPolicy = TimestampPolicy::FixedForTest;
        juce::String outError;
        REQUIRE(ComplexEnvelopeFairExporter::exportContainer(
            tempDir, orchResult, spec, stages, rawAudio, compAudio, outError));

        // Delete svg file
        tempDir.getChildFile("reports/complex_envelope_overlay.svg").deleteFile();

        juce::String auditError;
        bool valid = ComplexEnvelopeFairExporter::validateContainerIntegrity(tempDir, auditError);
        CHECK_FALSE(valid);
        CHECK(auditError.contains("missing"));
    }

    SECTION("Exporting without native state provider works and records not_compared")
    {
        ComplexEnvelopeOrchestrationResult noProviderResult = orchResult;
        noProviderResult.comparisons.clear();
        ObservableComparisonReport rep;
        rep.comparisonStatus = "not_compared";
        rep.limitations = "No native state provider supplied.";
        noProviderResult.comparisons.push_back(rep);

        ComplexEnvelopeExportSpec spec;
        spec.timestampPolicy = TimestampPolicy::FixedForTest;
        juce::String outError;

        bool ok = ComplexEnvelopeFairExporter::exportContainer(
            tempDir, noProviderResult, spec, {}, rawAudio, compAudio, outError);
        REQUIRE(ok);

        juce::String auditError;
        CHECK(ComplexEnvelopeFairExporter::validateContainerIntegrity(tempDir, auditError));
    }

    cleanup();
}
