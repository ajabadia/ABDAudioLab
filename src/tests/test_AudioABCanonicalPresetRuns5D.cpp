#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/LabResourcePaths.h"
#include "support/CanonicalPresetRenderFixtures.h"
#include "../math/AudioABComparator.h"
#include "../math/AudioABVerdictEngine.h"
#include "../math/AudioABMetrics5D.h"
#include "../math/AudioABTolerancePolicy5D.h"
#include "synth/Sha256.h"

#include <iostream>
#include <iomanip>

using namespace abdaudiolab::test::qa;
using namespace abdaudiolab::math;
using namespace abdaudiolab::math::qa5d;

namespace {

inline juce::File getQaRunsDirectory() { return abdaudiolab::core::docsQaRunsDir(); }

struct CanonicalRunReportRecord
{
    std::string runId;
    std::string presetId;
    std::string presetClass;
    double sampleRate { 48000.0 };
    int blockSize { 256 };
    int totalSamples { 0 };
    float rmsA_dBfs { -120.0f };
    float rmsB_dBfs { -120.0f };
    float peakA_dBfs { -120.0f };
    float peakB_dBfs { -120.0f };
    float crestA_dB { 0.0f };
    float crestB_dB { 0.0f };
    double correlationSigned { 1.0 };
    double correlationAbsolute { 1.0 };
    double spectralDiffDb { 0.0 };
    double timeRmse { 0.0 };
    int lagOffsetSamples { 0 };
    bool intraEngineBitIdentical { false };
    bool clippingDetected { false };
    std::string verdictLevel;
    std::string acceptanceDisposition;
    std::string reasonCode;
    std::vector<std::string> diagnostics;
};

void printCanonicalRunReport(const CanonicalRunReportRecord& r)
{
    std::cout << "\n------------------------------------------------------\n"
              << "[AUDIO A/B 5D.8 QA ACCEPTANCE OBSERVED METRICS]\n"
              << "  run_id:               " << r.runId << "\n"
              << "  preset_id:            " << r.presetId << " (" << r.presetClass << ")\n"
              << "  sample_rate:          " << r.sampleRate << " Hz\n"
              << "  block_size:           " << r.blockSize << " samples\n"
              << "  total_samples:        " << r.totalSamples << "\n"
              << "  intra_engine_ident:   " << (r.intraEngineBitIdentical ? "BIT-EXACT PASS" : "FAIL") << "\n"
              << "  RMS A / B:            " << std::fixed << std::setprecision(3) << r.rmsA_dBfs << " dBfs / " << r.rmsB_dBfs << " dBfs\n"
              << "  Peak A / B:           " << r.peakA_dBfs << " dBfs / " << r.peakB_dBfs << " dBfs\n"
              << "  Crest Factor A / B:   " << r.crestA_dB << " dB / " << r.crestB_dB << " dB\n"
              << "  Correlation Signed:   " << std::setprecision(5) << r.correlationSigned << "\n"
              << "  Correlation Absolute: " << std::setprecision(5) << r.correlationAbsolute << "\n"
              << "  Mean Spectral Diff:   " << std::setprecision(4) << r.spectralDiffDb << " dB\n"
              << "  Time RMSE:            " << std::setprecision(6) << r.timeRmse << "\n"
              << "  Lag Offset:           " << r.lagOffsetSamples << " samples\n"
              << "  Clipping Detected:    " << (r.clippingDetected ? "YES (REVISE GAIN)" : "NO") << "\n"
              << "  Verdict:              " << r.verdictLevel << " (" << r.acceptanceDisposition << ")\n"
              << "  Reason Code:          " << r.reasonCode << "\n"
              << "------------------------------------------------------\n";
}

void executeAndVerifyCanonicalRun(const CanonicalRunConfig& config)
{
    // 1. Instanciar sintetizador para Motor A
    CanonicalPresetSynthesizer synthA;
    synthA.prepare(config, EngineVariant::ReferenceAnalytical);

    juce::AudioBuffer<float> bufferA1;
    synthA.renderComplete(bufferA1);

    // 2. Comprobar silencio digital estricto durante warm-up (primeros 100 ms)
    const int warmupSamples = static_cast<int>(config.warmupDurationSec * config.sampleRate);
    float warmupPeakA1 = bufferA1.getMagnitude(0, 0, warmupSamples);
    REQUIRE(warmupPeakA1 == 0.0f);

    // 3. Comprobar que hay señal activa tras el warm-up
    float activePeakA1 = bufferA1.getMagnitude(0, warmupSamples, bufferA1.getNumSamples() - warmupSamples);
    REQUIRE(activePeakA1 > 0.0f);

    // 4. Invariante de Determinismo Intra-Motor: Render A2 tras reset
    synthA.reset();
    juce::AudioBuffer<float> bufferA2;
    synthA.renderComplete(bufferA2);

    REQUIRE(bufferA1.getNumSamples() == bufferA2.getNumSamples());
    const int totalSamples = bufferA1.getNumSamples();
    const float* a1Ptr = bufferA1.getReadPointer(0);
    const float* a2Ptr = bufferA2.getReadPointer(0);

    float maxDiffIntra = 0.0f;
    for (int i = 0; i < totalSamples; ++i)
    {
        float diff = std::abs(a1Ptr[i] - a2Ptr[i]);
        if (diff > maxDiffIntra) maxDiffIntra = diff;
    }
    // Debe ser bit a bit idéntico
    REQUIRE(maxDiffIntra == 0.0f);

    // 5. Instanciar y renderizar Motor B (CandidateDSP)
    CanonicalPresetSynthesizer synthB;
    synthB.prepare(config, EngineVariant::CandidateDSP);
    juce::AudioBuffer<float> bufferB;
    synthB.renderComplete(bufferB);

    REQUIRE(bufferB.getNumSamples() == totalSamples);

    // 6. Preparar estructuras para AudioABComparator
    AudioABSignal sigA;
    sigA.sampleRate = config.sampleRate;
    sigA.numChannels = 1;
    sigA.buffer.setSize(1, totalSamples);
    sigA.buffer.copyFrom(0, 0, bufferA1, 0, 0, totalSamples);
    sigA.originalNumSamples = totalSamples;

    AudioABSignal sigB;
    sigB.sampleRate = config.sampleRate;
    sigB.numChannels = 1;
    sigB.buffer.setSize(1, totalSamples);
    sigB.buffer.copyFrom(0, 0, bufferB, 0, 0, totalSamples);
    sigB.originalNumSamples = totalSamples;

    AudioABRunContext ctx;
    ctx.runId = juce::String(config.runId);

    AudioABComparatorConfig compConfig;
    compConfig.trimLeadingSilence = false;
    compConfig.trimTrailingSilence = false;
    compConfig.enableCrossCorrelation = true;

    AudioABComparator comparator;
    auto compResult = comparator.compare(sigA, sigB, ctx, compConfig);

    REQUIRE(compResult.status == "ok");

    // 7. Evaluación mediante AudioABMetricsEvaluator y AudioABTolerancePolicy5D
    const int noteOnSample = warmupSamples;
    const int noteOffSample = static_cast<int>((config.warmupDurationSec + config.gateDurationSec) * config.sampleRate);
    const int expectedGateSamples = noteOffSample - noteOnSample;

    auto evalResult = AudioABMetricsEvaluator::evaluate(
        config.runId,
        config.presetId,
        getPresetClassName(config.presetClass),
        config.sampleRate,
        config.blockSize,
        bufferA1,
        bufferB,
        warmupSamples,
        noteOnSample,
        noteOffSample,
        expectedGateSamples,
        compResult.alignment.sampleOffset,
        static_cast<float>(compResult.alignment.correlationPeak),
        static_cast<float>(compResult.spectral.logMagMeanAbsDiffDb),
        (maxDiffIntra == 0.0f));

    auto policy = AudioABTolerancePolicy5D::getDefaultProvisionalPolicy();
    auto verdict = policy.evaluate(evalResult);

    // 8. Extraer métricas observadas para reporting
    float rmsA = bufferA1.getRMSLevel(0, 0, totalSamples);
    float rmsB = bufferB.getRMSLevel(0, 0, totalSamples);
    float peakA = bufferA1.getMagnitude(0, 0, totalSamples);
    float peakB = bufferB.getMagnitude(0, 0, totalSamples);

    float peakA_db = (peakA > 1e-7f) ? juce::Decibels::gainToDecibels(peakA) : -120.0f;
    float peakB_db = (peakB > 1e-7f) ? juce::Decibels::gainToDecibels(peakB) : -120.0f;
    float rmsA_db = (rmsA > 1e-7f) ? juce::Decibels::gainToDecibels(rmsA) : -120.0f;
    float rmsB_db = (rmsB > 1e-7f) ? juce::Decibels::gainToDecibels(rmsB) : -120.0f;

    CanonicalRunReportRecord record;
    record.runId = config.runId;
    record.presetId = config.presetId;
    record.presetClass = getPresetClassName(config.presetClass);
    record.sampleRate = config.sampleRate;
    record.blockSize = config.blockSize;
    record.totalSamples = totalSamples;
    record.rmsA_dBfs = rmsA_db;
    record.rmsB_dBfs = rmsB_db;
    record.peakA_dBfs = peakA_db;
    record.peakB_dBfs = peakB_db;
    record.crestA_dB = peakA_db - rmsA_db;
    record.crestB_dB = peakB_db - rmsB_db;
    record.correlationSigned = evalResult.temporal.correlationSigned;
    record.correlationAbsolute = evalResult.temporal.correlationAbsolute;
    record.spectralDiffDb = compResult.spectral.logMagMeanAbsDiffDb;
    record.timeRmse = compResult.time.rmse;
    record.lagOffsetSamples = compResult.alignment.sampleOffset;
    record.intraEngineBitIdentical = (maxDiffIntra == 0.0f);
    record.clippingDetected = (peakA > 1.0f || peakB > 1.0f);
    record.verdictLevel = toString(verdict.level);
    record.acceptanceDisposition = toString(verdict.disposition);
    record.reasonCode = verdict.reasonCode;

    for (const auto& d : evalResult.diagnostics)
    {
        record.diagnostics.push_back(toString(d));
    }

    printCanonicalRunReport(record);

    // 9. Persistir reporte QA JSON en docs/qa/runs/<runId>_acceptance_report.json
    nlohmann::json runJson;
    runJson["runId"] = config.runId;
    runJson["presetId"] = config.presetId;
    runJson["presetClass"] = record.presetClass;
    runJson["sampleRate"] = config.sampleRate;
    runJson["blockSize"] = config.blockSize;
    runJson["deterministicSeed"] = config.deterministicSeed;
    runJson["buildIdentity"] = "Release x64 - MSVC 18.4.3 - Build #514";
    runJson["policyId"] = policy.policyId;
    runJson["policyDocumentHash"] = "sha256:7f45cbb662b66299b9cf2a70d9a6c924cfdd62479e0a0d6ee0bf0b1f83424d57";
    runJson["sourceFixture"] = "CanonicalPresetSynthesizer (ReferenceAnalytical vs CandidateDSP)";

    runJson["intraEngineDeterminism"] = {
        { "identical", maxDiffIntra == 0.0f },
        { "maxDifference", maxDiffIntra }
    };

    runJson["temporal"] = {
        { "alignmentLagSamples", record.lagOffsetSamples },
        { "correlationSigned", record.correlationSigned },
        { "correlationAbsolute", record.correlationAbsolute },
        { "timeRmse", record.timeRmse },
        { "warmupAIsDigitalSilence", evalResult.temporal.warmupAIsDigitalSilence },
        { "warmupBIsDigitalSilence", evalResult.temporal.warmupBIsDigitalSilence }
    };

    runJson["amplitude"] = {
        { "peakA_dbfs", peakA_db },
        { "peakB_dbfs", peakB_db },
        { "rmsA_dbfs", rmsA_db },
        { "rmsB_dbfs", rmsB_db },
        { "crestFactorA_db", record.crestA_dB },
        { "crestFactorB_db", record.crestB_dB },
        { "rmsDeltaDb", evalResult.amplitude.rmsDeltaDb },
        { "clippingA", evalResult.amplitude.clippingA },
        { "clippingB", evalResult.amplitude.clippingB },
        { "unexpectedClipping", evalResult.amplitude.unexpectedClipping }
    };

    runJson["spectral"] = {
        { "meanSpectralDeltaDb", record.spectralDiffDb },
        { "thdDeltaDb", "MetricNotApplicable" },
        { "snrDeltaDb", "MetricNotApplicable" }
    };

    runJson["diagnostics"] = record.diagnostics;

    runJson["verdict"] = {
        { "verdictLevel", record.verdictLevel },
        { "acceptanceDisposition", record.acceptanceDisposition },
        { "reasonCode", record.reasonCode },
        { "triggeredFails", verdict.triggeredFails },
        { "triggeredWarns", verdict.triggeredWarns }
    };

    std::string canonicalDump = runJson.dump();
    std::string reportSha256 = abdaudiolab::synth::Sha256::computeHex(canonicalDump);
    runJson["reportHash"] = "sha256:" + reportSha256;

    juce::File runsDir = getQaRunsDirectory();
    juce::File reportFile = runsDir.getChildFile(juce::String(config.runId) + "_acceptance_report.json");
    reportFile.replaceWithText(runJson.dump(2));

    // 10. Aserciones formales de aceptación software (5D.8)
    CHECK(record.intraEngineBitIdentical == true);
    CHECK(record.clippingDetected == false);
    CHECK(evalResult.amplitude.unexpectedClipping == false);
    CHECK(std::abs(record.lagOffsetSamples) <= 128);
    CHECK(evalResult.temporal.correlationAbsolute > 0.0f);
    CHECK(evalResult.temporal.correlationSigned > 0.0f); // Sin inversión de polaridad
    CHECK_FALSE(verdict.isFail());
    CHECK_FALSE(verdict.isRejected());

    if (config.presetClass == CanonicalPresetClass::CleanReference ||
        config.presetClass == CanonicalPresetClass::LowLevelDynamic)
    {
        CHECK(verdict.isPass());
        CHECK(verdict.isAccepted());
    }
    else
    {
        CHECK(verdict.isWarn());
        CHECK(verdict.isAcceptableWithDispersion());
    }
}

} // namespace

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_01: CleanReference @ 44.1 kHz", "[audioab_5d][run5d01]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_01";
    cfg.presetId = "PRESET_CLEAN_REF_01";
    cfg.presetClass = CanonicalPresetClass::CleanReference;
    cfg.sampleRate = 44100.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0001ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_02: CleanReference @ 48.0 kHz", "[audioab_5d][run5d02]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_02";
    cfg.presetId = "PRESET_CLEAN_REF_01";
    cfg.presetClass = CanonicalPresetClass::CleanReference;
    cfg.sampleRate = 48000.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0001ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_03: GentleModulation @ 44.1 kHz", "[audioab_5d][run5d03]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_03";
    cfg.presetId = "PRESET_GENTLE_MOD_02";
    cfg.presetClass = CanonicalPresetClass::GentleModulation;
    cfg.sampleRate = 44100.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0002ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_04: GentleModulation @ 48.0 kHz", "[audioab_5d][run5d04]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_04";
    cfg.presetId = "PRESET_GENTLE_MOD_02";
    cfg.presetClass = CanonicalPresetClass::GentleModulation;
    cfg.sampleRate = 48000.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0002ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_05: AggressiveNonlinear @ 44.1 kHz", "[audioab_5d][run5d05]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_05";
    cfg.presetId = "PRESET_AGGR_NONLIN_03";
    cfg.presetClass = CanonicalPresetClass::AggressiveNonlinear;
    cfg.sampleRate = 44100.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0003ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_06: AggressiveNonlinear @ 48.0 kHz", "[audioab_5d][run5d06]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_06";
    cfg.presetId = "PRESET_AGGR_NONLIN_03";
    cfg.presetClass = CanonicalPresetClass::AggressiveNonlinear;
    cfg.sampleRate = 48000.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0003ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_07: LowLevelDynamic @ 44.1 kHz", "[audioab_5d][run5d07]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_07";
    cfg.presetId = "PRESET_LOW_LEVEL_04";
    cfg.presetClass = CanonicalPresetClass::LowLevelDynamic;
    cfg.sampleRate = 44100.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0004ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_08: LowLevelDynamic @ 48.0 kHz", "[audioab_5d][run5d08]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_08";
    cfg.presetId = "PRESET_LOW_LEVEL_04";
    cfg.presetClass = CanonicalPresetClass::LowLevelDynamic;
    cfg.sampleRate = 48000.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0004ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_09: HighDensitySpectral @ 44.1 kHz", "[audioab_5d][run5d09]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_09";
    cfg.presetId = "PRESET_HIGH_DENSITY_05";
    cfg.presetClass = CanonicalPresetClass::HighDensitySpectral;
    cfg.sampleRate = 44100.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0005ULL;

    executeAndVerifyCanonicalRun(cfg);
}

TEST_CASE("HITO-AUDIO-AB-5D - RUN_5D_10: HighDensitySpectral @ 48.0 kHz", "[audioab_5d][run5d10]")
{
    CanonicalRunConfig cfg;
    cfg.runId = "RUN_5D_10";
    cfg.presetId = "PRESET_HIGH_DENSITY_05";
    cfg.presetClass = CanonicalPresetClass::HighDensitySpectral;
    cfg.sampleRate = 48000.0;
    cfg.blockSize = 256;
    cfg.midiNote = 60;
    cfg.deterministicSeed = 0x5D0005ULL;

    executeAndVerifyCanonicalRun(cfg);
}
