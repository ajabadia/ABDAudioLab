#include "ProfilingSession.h"
#include <fstream>
#include <iostream>
#include <cmath>

namespace abdaudiolab::core
{

bool ProfilingSession::loadProfileFromJson(const std::string& jsonString)
{
    try
    {
        auto j = nlohmann::json::parse(jsonString);

        if (j.contains("profile_metadata"))
        {
            const auto& meta = j["profile_metadata"];
            metadata.hardwareName = meta.value("hardware_name", "UNKNOWN_HARDWARE");
            metadata.targetModule = meta.value("target_module", "MAIN");
            metadata.operatorMode = meta.value("operator_mode", "MOCK_DSP");
            metadata.sampleRate = meta.value("sample_rate", 96000.0);
            metadata.bitDepth = meta.value("bit_depth", 24);
            metadata.timestamp = meta.value("timestamp", "");
        }

        testCases.clear();
        if (j.contains("test_cases") && j["test_cases"].is_array())
        {
            for (const auto& tcJson : j["test_cases"])
            {
                TestCase tc;
                tc.testId = tcJson.value("test_id", "TC_001");
                tc.functionalBlockType = tcJson.value("functional_block_type", "SpectrumFilter");
                
                std::string stim = tcJson.value("stimulus_type", "LogFarinaSweep");
                if (stim == "DiracDelta") tc.stimulusType = audio::StimulusType::DiracDelta;
                else if (stim == "SyncPulses3") tc.stimulusType = audio::StimulusType::SyncPulses3;
                else if (stim == "WhiteNoise") tc.stimulusType = audio::StimulusType::WhiteNoise;
                else if (stim == "PinkNoise") tc.stimulusType = audio::StimulusType::PinkNoise;
                else if (stim == "SineWave1kHz") tc.stimulusType = audio::StimulusType::SineWave1kHz;
                else if (stim == "SquareWave1kHz") tc.stimulusType = audio::StimulusType::SquareWave1kHz;
                else if (stim == "AmplitudeRamp") tc.stimulusType = audio::StimulusType::AmplitudeRamp;
                else if (stim == "Silence") tc.stimulusType = audio::StimulusType::Silence;
                else tc.stimulusType = audio::StimulusType::LogFarinaSweep;

                tc.stimulusDurationSec = tcJson.value("duration_sec", 1.5);
                tc.startFreqHz = tcJson.value("start_freq_hz", 20.0f);
                tc.endFreqHz = tcJson.value("end_freq_hz", 20000.0f);
                tc.numPasses = tcJson.value("num_passes", 3);
                tc.stabilizationWaitMs = tcJson.value("stabilization_wait_ms", 50.0);

                if (tcJson.contains("parameters") && tcJson["parameters"].is_array())
                {
                    for (const auto& pJson : tcJson["parameters"])
                    {
                        ParameterStep ps;
                        ps.paramIndex = pJson.value("index", 1);
                        ps.paramName = pJson.value("name", "PARAM");
                        ps.normalizedValue = pJson.value("normalized_value", 0.5f);
                        ps.rawValue = pJson.value("raw_value", static_cast<int>(ps.normalizedValue * 127.0f));
                        tc.parameterSteps.push_back(ps);
                    }
                }
                testCases.push_back(tc);
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ProfilingSession::loadProfileFromJson error: " << e.what() << std::endl;
        return false;
    }
}

bool ProfilingSession::loadProfileFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
        return false;

    std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return loadProfileFromJson(str);
}

namespace
{

ProfilingSession createTwoParamSuite(const std::string& hardwareName,
                                      const std::string& targetModule,
                                      const std::string& operatorMode,
                                      const std::string& testPrefix,
                                      const std::string& blockType,
                                      abdaudiolab::audio::StimulusType stimulusType,
                                      double durationSec,
                                      int numPasses,
                                      double waitMs,
                                      const std::string& p1Name, int p1Steps,
                                      const std::string& p2Name, int p2Steps,
                                      float p2DefaultNorm = 0.0f)
{
    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = hardwareName;
    meta.targetModule = targetModule;
    meta.operatorMode = operatorMode;
    meta.sampleRate = 96000.0;
    meta.bitDepth = 24;
    session.setMetadata(meta);

    int total1 = std::max(2, p1Steps);
    int total2 = std::max(1, p2Steps);
    int id = 1;

    for (int j = 0; j < total2; ++j)
    {
        float val2 = (total2 > 1) ? (static_cast<float>(j) / static_cast<float>(total2 - 1)) : p2DefaultNorm;
        for (int i = 0; i < total1; ++i)
        {
            float val1 = static_cast<float>(i) / static_cast<float>(total1 - 1);

            TestCase tc;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%s_%03d", testPrefix.c_str(), id++);
            tc.testId = buf;
            tc.functionalBlockType = blockType;
            tc.stimulusType = stimulusType;
            tc.stimulusDurationSec = durationSec;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = numPasses;
            tc.stabilizationWaitMs = waitMs;

            ParameterStep pStep1 { 1, p1Name, val1, static_cast<int>(val1 * 127.0f) };
            ParameterStep pStep2 { 2, p2Name, val2, static_cast<int>(val2 * 127.0f) };
            tc.parameterSteps.reserve(2);
            tc.parameterSteps.push_back(pStep1);
            tc.parameterSteps.push_back(pStep2);

            session.addTestCase(tc);
        }
    }
    return session;
}

ProfilingSession createOneParamSuite(const std::string& hardwareName,
                                      const std::string& targetModule,
                                      const std::string& operatorMode,
                                      const std::string& testPrefix,
                                      const std::string& blockType,
                                      abdaudiolab::audio::StimulusType stimulusType,
                                      double durationSec,
                                      int numPasses,
                                      double waitMs,
                                      const std::string& pName, int steps)
{
    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = hardwareName;
    meta.targetModule = targetModule;
    meta.operatorMode = operatorMode;
    meta.sampleRate = 96000.0;
    meta.bitDepth = 24;
    session.setMetadata(meta);

    int totalSteps = std::max(2, steps);
    int id = 1;

    for (int s = 0; s < totalSteps; ++s)
    {
        float val = static_cast<float>(s) / static_cast<float>(totalSteps - 1);

        TestCase tc;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s_%03d", testPrefix.c_str(), id++);
        tc.testId = buf;
        tc.functionalBlockType = blockType;
        tc.stimulusType = stimulusType;
        tc.stimulusDurationSec = durationSec;
        tc.numPasses = numPasses;
        tc.stabilizationWaitMs = waitMs;

        ParameterStep pStep { 1, pName, val, static_cast<int>(val * 127.0f) };
        tc.parameterSteps.reserve(1);
        tc.parameterSteps.push_back(pStep);

        session.addTestCase(tc);
    }
    return session;
}

} // anonymous namespace

ProfilingSession ProfilingSession::createFilterSuite(const std::string& hardwareName, const std::string& operatorMode, int cutSteps, int resSteps)
{
    return createTwoParamSuite(hardwareName, "FILTER_SPECTRUM_SCAN", operatorMode, "TC_FLT", "SpectrumFilter",
                               audio::StimulusType::LogFarinaSweep, 1.0, 3, 40.0,
                               "CUTOFF", cutSteps, "RESONANCE", resSteps, 0.0f);
}

ProfilingSession ProfilingSession::createAdsrSuite(const std::string& hardwareName, const std::string& operatorMode, int attackSteps, int decaySteps)
{
    return createTwoParamSuite(hardwareName, "ADSR_ENVELOPE_SCAN", operatorMode, "TC_ADSR", "TimeDynamic",
                               audio::StimulusType::DiracDelta, 1.5, 3, 50.0,
                               "ATTACK", attackSteps, "DECAY", decaySteps, 0.3f);
}

ProfilingSession ProfilingSession::createDelaySuite(const std::string& hardwareName, const std::string& operatorMode, int timeSteps, int fbSteps)
{
    return createTwoParamSuite(hardwareName, "DELAY_TIME_SCAN", operatorMode, "TC_DLY", "TimeDynamic",
                               audio::StimulusType::DiracDelta, 2.0, 3, 60.0,
                               "TIME", timeSteps, "FEEDBACK", fbSteps, 0.2f);
}

ProfilingSession ProfilingSession::createWaveShaperSuite(const std::string& hardwareName, const std::string& operatorMode, int driveSteps)
{
    return createOneParamSuite(hardwareName, "WAVESHAPER_DISTORTION_SCAN", operatorMode, "TC_SAT", "WaveShaper",
                               audio::StimulusType::AmplitudeRamp, 1.0, 3, 30.0, "DRIVE", driveSteps);
}

ProfilingSession ProfilingSession::createGainVcaSuite(const std::string& hardwareName, const std::string& operatorMode, int gainSteps)
{
    return createOneParamSuite(hardwareName, "VCA_ATTENUATION_SCAN", operatorMode, "TC_VCA", "AmplitudeGain",
                               audio::StimulusType::SineWave1kHz, 0.5, 3, 30.0, "GAIN", gainSteps);
}

ProfilingSession ProfilingSession::createChorusModulatorSuite(const std::string& hardwareName, const std::string& operatorMode, int rateSteps, int depthSteps)
{
    return createTwoParamSuite(hardwareName, "CYCLIC_MODULATOR_SCAN", operatorMode, "TC_MOD", "CyclicModulator",
                               audio::StimulusType::SineWave1kHz, 2.0, 2, 50.0,
                               "RATE", rateSteps, "DEPTH", depthSteps, 0.0f);
}

ProfilingSession ProfilingSession::createDefaultMockSession()
{
    return createFilterSuite("MOCK_VA_RESONANT_FILTER", "MOCK_DSP", 5, 3);
}

} // namespace abdaudiolab::core
