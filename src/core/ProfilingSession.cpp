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

ProfilingSession ProfilingSession::createFilterSuite(const std::string& hardwareName, const std::string& operatorMode, int cutSteps, int resSteps)
{
    ProfilingSession session;
    session.metadata.hardwareName = hardwareName;
    session.metadata.targetModule = "FILTER_SPECTRUM_SCAN";
    session.metadata.operatorMode = operatorMode;
    session.metadata.sampleRate = 96000.0;
    session.metadata.bitDepth = 24;

    int totalCuts = std::max(2, cutSteps);
    int totalRes = std::max(1, resSteps);
    int id = 1;

    for (int r = 0; r < totalRes; ++r)
    {
        float resVal = (totalRes > 1) ? (static_cast<float>(r) / static_cast<float>(totalRes - 1)) : 0.0f;
        for (int c = 0; c < totalCuts; ++c)
        {
            float cutVal = static_cast<float>(c) / static_cast<float>(totalCuts - 1);

            TestCase tc;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "TC_FLT_%03d", id++);
            tc.testId = buf;
            tc.functionalBlockType = "SpectrumFilter";
            tc.stimulusType = audio::StimulusType::LogFarinaSweep;
            tc.stimulusDurationSec = 1.0;
            tc.startFreqHz = 20.0f;
            tc.endFreqHz = 20000.0f;
            tc.numPasses = 3;
            tc.stabilizationWaitMs = 40.0;

            ParameterStep pCut { 1, "CUTOFF", cutVal, static_cast<int>(cutVal * 127.0f) };
            ParameterStep pRes { 2, "RESONANCE", resVal, static_cast<int>(resVal * 127.0f) };
            tc.parameterSteps.push_back(pCut);
            tc.parameterSteps.push_back(pRes);

            session.testCases.push_back(tc);
        }
    }
    return session;
}

ProfilingSession ProfilingSession::createAdsrSuite(const std::string& hardwareName, const std::string& operatorMode, int attackSteps, int decaySteps)
{
    ProfilingSession session;
    session.metadata.hardwareName = hardwareName;
    session.metadata.targetModule = "ADSR_ENVELOPE_SCAN";
    session.metadata.operatorMode = operatorMode;
    session.metadata.sampleRate = 96000.0;
    session.metadata.bitDepth = 24;

    int totalAtt = std::max(2, attackSteps);
    int totalDec = std::max(1, decaySteps);
    int id = 1;

    for (int d = 0; d < totalDec; ++d)
    {
        float decVal = (totalDec > 1) ? (static_cast<float>(d) / static_cast<float>(totalDec - 1)) : 0.3f;
        for (int a = 0; a < totalAtt; ++a)
        {
            float attVal = static_cast<float>(a) / static_cast<float>(totalAtt - 1);

            TestCase tc;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "TC_ADSR_%03d", id++);
            tc.testId = buf;
            tc.functionalBlockType = "TimeDynamic";
            tc.stimulusType = audio::StimulusType::DiracDelta; // Trigger pulse
            tc.stimulusDurationSec = 1.5;
            tc.numPasses = 3;
            tc.stabilizationWaitMs = 50.0;

            ParameterStep pAtt { 1, "ATTACK", attVal, static_cast<int>(attVal * 127.0f) };
            ParameterStep pDec { 2, "DECAY", decVal, static_cast<int>(decVal * 127.0f) };
            tc.parameterSteps.push_back(pAtt);
            tc.parameterSteps.push_back(pDec);

            session.testCases.push_back(tc);
        }
    }
    return session;
}

ProfilingSession ProfilingSession::createDelaySuite(const std::string& hardwareName, const std::string& operatorMode, int timeSteps, int fbSteps)
{
    ProfilingSession session;
    session.metadata.hardwareName = hardwareName;
    session.metadata.targetModule = "DELAY_TIME_SCAN";
    session.metadata.operatorMode = operatorMode;
    session.metadata.sampleRate = 96000.0;
    session.metadata.bitDepth = 24;

    int totalTime = std::max(2, timeSteps);
    int totalFb = std::max(1, fbSteps);
    int id = 1;

    for (int f = 0; f < totalFb; ++f)
    {
        float fbVal = (totalFb > 1) ? (static_cast<float>(f) / static_cast<float>(totalFb - 1)) : 0.2f;
        for (int t = 0; t < totalTime; ++t)
        {
            float timeVal = static_cast<float>(t) / static_cast<float>(totalTime - 1);

            TestCase tc;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "TC_DLY_%03d", id++);
            tc.testId = buf;
            tc.functionalBlockType = "TimeDynamic";
            tc.stimulusType = audio::StimulusType::DiracDelta;
            tc.stimulusDurationSec = 2.0;
            tc.numPasses = 3;
            tc.stabilizationWaitMs = 60.0;

            ParameterStep pTime { 1, "TIME", timeVal, static_cast<int>(timeVal * 127.0f) };
            ParameterStep pFb   { 2, "FEEDBACK", fbVal, static_cast<int>(fbVal * 127.0f) };
            tc.parameterSteps.push_back(pTime);
            tc.parameterSteps.push_back(pFb);

            session.testCases.push_back(tc);
        }
    }
    return session;
}

ProfilingSession ProfilingSession::createWaveShaperSuite(const std::string& hardwareName, const std::string& operatorMode, int driveSteps)
{
    ProfilingSession session;
    session.metadata.hardwareName = hardwareName;
    session.metadata.targetModule = "WAVESHAPER_DISTORTION_SCAN";
    session.metadata.operatorMode = operatorMode;
    session.metadata.sampleRate = 96000.0;
    session.metadata.bitDepth = 24;

    int totalSteps = std::max(2, driveSteps);
    int id = 1;

    for (int s = 0; s < totalSteps; ++s)
    {
        float driveVal = static_cast<float>(s) / static_cast<float>(totalSteps - 1);

        TestCase tc;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "TC_SAT_%03d", id++);
        tc.testId = buf;
        tc.functionalBlockType = "WaveShaper";
        tc.stimulusType = audio::StimulusType::AmplitudeRamp; // Linear amplitude ramp
        tc.stimulusDurationSec = 1.0;
        tc.numPasses = 3;
        tc.stabilizationWaitMs = 30.0;

        ParameterStep pDrive { 1, "DRIVE", driveVal, static_cast<int>(driveVal * 127.0f) };
        tc.parameterSteps.push_back(pDrive);

        session.testCases.push_back(tc);
    }
    return session;
}

ProfilingSession ProfilingSession::createGainVcaSuite(const std::string& hardwareName, const std::string& operatorMode, int gainSteps)
{
    ProfilingSession session;
    session.metadata.hardwareName = hardwareName;
    session.metadata.targetModule = "VCA_ATTENUATION_SCAN";
    session.metadata.operatorMode = operatorMode;
    session.metadata.sampleRate = 96000.0;
    session.metadata.bitDepth = 24;

    int totalSteps = std::max(2, gainSteps);
    int id = 1;

    for (int s = 0; s < totalSteps; ++s)
    {
        float gainVal = static_cast<float>(s) / static_cast<float>(totalSteps - 1);

        TestCase tc;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "TC_VCA_%03d", id++);
        tc.testId = buf;
        tc.functionalBlockType = "AmplitudeGain";
        tc.stimulusType = audio::StimulusType::SineWave1kHz; // 1kHz continuous tone
        tc.stimulusDurationSec = 0.5;
        tc.numPasses = 3;
        tc.stabilizationWaitMs = 30.0;

        ParameterStep pGain { 1, "GAIN", gainVal, static_cast<int>(gainVal * 127.0f) };
        tc.parameterSteps.push_back(pGain);

        session.testCases.push_back(tc);
    }
    return session;
}

ProfilingSession ProfilingSession::createDefaultMockSession()
{
    return createFilterSuite("MOCK_VA_RESONANT_FILTER", "MOCK_DSP", 5, 3);
}

} // namespace abdaudiolab::core
