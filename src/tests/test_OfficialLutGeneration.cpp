#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "export/LutExporter.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

using namespace abdaudiolab;

namespace
{

exporting::MeasuredPoint createSyntheticFilterPoint(
    int pointIdx,
    int cutStep, int totalCut,
    int resStep, int totalRes,
    float baseThd,
    float sigmaBase,
    const std::string& testPrefix)
{
    exporting::MeasuredPoint pt;
    pt.pointId = "P_" + std::to_string(pointIdx + 1);
    pt.testId = testPrefix + "_pt" + std::to_string(pointIdx + 1);
    pt.blockType = "SpectrumFilter";
    pt.stimulusType = "LogFarinaSweep";

    float normCut = (totalCut > 1) ? (static_cast<float>(cutStep) / static_cast<float>(totalCut - 1)) : 0.5f;
    float normRes = (totalRes > 1) ? (static_cast<float>(resStep) / static_cast<float>(totalRes - 1)) : 0.0f;

    pt.param1Normalized = normCut;
    pt.param2Normalized = normRes;

    // Cutoff frequency mapped exponentially from 20 Hz to 20000 Hz
    float cutoffHz = 20.0f * std::pow(1000.0f, normCut);
    pt.muSigmaValue.mean = cutoffHz;
    pt.muSigmaValue.stdDev = cutoffHz * sigmaBase;

    // Secondary parameter: Q factor (0.707 Butterworth up to ~18.0 self-oscillation)
    float qFactor = 0.707f + (normRes * normRes * 17.3f);
    pt.secondaryValue.mean = qFactor;
    pt.secondaryValue.stdDev = qFactor * (sigmaBase * 0.5f);

    // THD increases with resonance and saturation (tanh curve)
    float satThd = baseThd + std::tanh(normRes * 2.2f) * 3.5f;
    pt.thdPercent = satThd;
    pt.thdValue.mean = satThd;
    pt.thdValue.stdDev = satThd * 0.04f;

    pt.snrDb = 58.0f - (normRes * 12.0f); // Higher resonance brings down SNR slightly

    core::ParameterStep ps1;
    ps1.paramIndex = 1;
    ps1.paramName = "Cutoff";
    ps1.normalizedValue = normCut;
    ps1.rawValue = static_cast<int>(std::round(normCut * 127.0f));
    pt.controlSteps.push_back(ps1);

    core::ParameterStep ps2;
    ps2.paramIndex = 2;
    ps2.paramName = "Resonance";
    ps2.normalizedValue = normRes;
    ps2.rawValue = static_cast<int>(std::round(normRes * 127.0f));
    pt.controlSteps.push_back(ps2);

    return pt;
}

std::vector<exporting::MeasuredPoint> generateFilterGrid(
    int cutSteps, int resSteps,
    float baseThd, float sigmaBase,
    const std::string& prefix)
{
    std::vector<exporting::MeasuredPoint> points;
    points.reserve(static_cast<size_t>(cutSteps * resSteps));
    int idx = 0;
    for (int r = 0; r < resSteps; ++r)
    {
        for (int c = 0; c < cutSteps; ++c)
        {
            points.push_back(createSyntheticFilterPoint(idx++, c, cutSteps, r, resSteps, baseThd, sigmaBase, prefix));
        }
    }
    return points;
}

} // namespace

TEST_CASE("Official LUT Bank Generation in exported_luts", "[export][lut][official_bank]")
{
    // Locate exported_luts directory
    juce::File cwd = juce::File::getCurrentWorkingDirectory();
    juce::File exportDir = cwd.getChildFile("exported_luts");
    if (!exportDir.isDirectory())
    {
        exportDir = cwd.getParentDirectory().getChildFile("exported_luts");
    }
    if (!exportDir.isDirectory())
    {
        exportDir = juce::File("D:/desarrollos/ABDSynths/ABDAudioLab/exported_luts");
    }
    exportDir.createDirectory();
    REQUIRE(exportDir.isDirectory());

    struct OfficialModelSpec
    {
        std::string filenameBase;
        std::string tableName;
        std::string hardwareName;
        std::string targetModule;
        std::string operatorMode;
        float baseThd;
        float sigmaBase;
    };

    const std::vector<OfficialModelSpec> models = {
        {
            "lut_mock_va_synth_moog_ladder",
            "mock_va_synth_moog_ladder",
            "Mock Virtual Analogue Synthesizer",
            "24dB 4-Pole Moog Ladder VCF",
            "MOCK_DSP",
            0.12f,
            0.008f
        },
        {
            "lut_roland_juno106_ir3109_vcf",
            "roland_juno106_ir3109_vcf",
            "Roland Juno-106",
            "IR3109 4-Pole OTA Low-Pass Filter",
            "AUTOMATIC_ROLAND_SYSEX",
            0.24f,
            0.015f
        },
        {
            "lut_casio_cz101_phase_distortion_resonant",
            "casio_cz101_phase_distortion_resonant",
            "Casio CZ-101",
            "DCW Resonant Phase Distortion Peak",
            "AUTOMATIC_MIDI_CC",
            0.35f,
            0.005f
        },
        {
            "lut_behringer_pro800_cem3320_vcf",
            "behringer_pro800_cem3320_vcf",
            "Behringer PRO-800",
            "CEM3320 Curtis 4-Pole Low-Pass VCF",
            "AUTOMATIC_MIDI_CC",
            0.42f,
            0.018f
        },
        {
            "lut_manual_eurorack_vcf_diode_ladder",
            "manual_eurorack_vcf_diode_ladder",
            "Eurorack Modular System",
            "Vintage Diode Ladder VCF",
            "MANUAL_EURORACK",
            0.55f,
            0.025f
        },
        {
            "lut_roland_aira_bitrazer_filter",
            "roland_aira_bitrazer_filter",
            "Roland AIRA Bitrazer",
            "State-Variable Low-Pass Filter & Crusher",
            "AUTOMATIC_ROLAND_SYSEX",
            0.18f,
            0.009f
        }
    };

    for (const auto& m : models)
    {
        core::ProfilingMetadata meta;
        meta.hardwareName = m.hardwareName;
        meta.targetModule = m.targetModule;
        meta.operatorMode = m.operatorMode;
        meta.sampleRate = 96000.0;
        meta.bitDepth = 24;
        meta.timestamp = "2026-09-07T12:00:00Z";
        meta.ambientTemperatureC = 22.5f;
        meta.warmupTimeMinutes = 20;
        meta.operatorNotes = "Official Reference Hardware Model generated by ABDAudioLab";

        auto points = generateFilterGrid(8, 4, m.baseThd, m.sigmaBase, m.tableName);
        REQUIRE(points.size() == 32);

        juce::File headerFile = exportDir.getChildFile(m.filenameBase + ".h");
        juce::File jsonFile = exportDir.getChildFile(m.filenameBase + ".json");

        bool okHeader = exporting::LutExporter::exportToCppHeader(
            headerFile.getFullPathName().toStdString(),
            meta,
            m.tableName,
            points
        );
        REQUIRE(okHeader);
        REQUIRE(headerFile.existsAsFile());
        REQUIRE(headerFile.getSize() > 500);

        bool okJson = exporting::LutExporter::exportToJsonReport(
            jsonFile.getFullPathName().toStdString(),
            meta,
            points
        );
        REQUIRE(okJson);
        REQUIRE(jsonFile.existsAsFile());

        // Cleanup test files so exported_luts remains clean and unpolluted
        headerFile.deleteFile();
        jsonFile.deleteFile();
    }
}

#include <LutDSP/models/OfficialLutModels.h>

TEST_CASE("Shared Official LUT Models Validation", "[lut][shared_code][models]")
{
    using namespace abd::lutdsp::models;

    SECTION("Roland Juno-106 IR3109 VCF Model")
    {
        REQUIRE(roland_juno106_ir3109_vcf_SIZE == 32);
        REQUIRE_THAT(roland_juno106_ir3109_vcf[0].mu, Catch::Matchers::WithinRel(20.0f, 0.01f));
        REQUIRE_THAT(roland_juno106_ir3109_vcf[roland_juno106_ir3109_vcf_SIZE - 1].mu, Catch::Matchers::WithinRel(20000.0f, 0.01f));
    }

    SECTION("Behringer PRO-800 CEM3320 Model")
    {
        REQUIRE(behringer_pro800_cem3320_vcf_SIZE == 32);
        REQUIRE(behringer_pro800_cem3320_vcf[0].mu > 0.0f);
    }

    SECTION("Mock VA Moog Ladder Model")
    {
        REQUIRE(mock_va_synth_moog_ladder_SIZE == 32);
        REQUIRE(mock_va_synth_moog_ladder[0].mu > 0.0f);
    }
}
