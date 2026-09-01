#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../audio/LabStimulusGenerator.h"

namespace abdaudiolab::core
{

struct ParameterStep
{
    int paramIndex { 1 };
    std::string paramName;
    float normalizedValue { 0.0f };
    int rawValue { 0 };
};

struct TestCase
{
    std::string testId;
    std::string functionalBlockType; // TimeDynamic, SpectrumFilter, AmplitudeGain, WaveShaper, CyclicModulator
    audio::StimulusType stimulusType { audio::StimulusType::LogFarinaSweep };
    double stimulusDurationSec { 2.0 };
    float startFreqHz { 20.0f };
    float endFreqHz { 20000.0f };
    int numPasses { 3 };
    double stabilizationWaitMs { 50.0 };

    std::vector<ParameterStep> parameterSteps;
};

struct ProfilingMetadata
{
    std::string hardwareName;
    std::string targetModule;
    std::string operatorMode; // "AUTOMATIC_ROLAND_SYSEX", "AUTOMATIC_MIDI_CC", "MANUAL_EURORACK", "MOCK_DSP"
    double sampleRate { 96000.0 };
    int bitDepth { 24 };
    std::string timestamp;
};

class ProfilingSession
{
public:
    ProfilingSession() = default;
    ~ProfilingSession() = default;

    bool loadProfileFromJson(const std::string& jsonString);
    bool loadProfileFromFile(const std::string& filePath);

    [[nodiscard]] const ProfilingMetadata& getMetadata() const noexcept { return metadata; }
    [[nodiscard]] const std::vector<TestCase>& getTestCases() const noexcept { return testCases; }

    void setMetadata(const ProfilingMetadata& meta) { metadata = meta; }

    // Pre-built profiling test suites
    static ProfilingSession createFilterSuite(const std::string& hardwareName, const std::string& operatorMode, int cutSteps = 8, int resSteps = 4);
    static ProfilingSession createAdsrSuite(const std::string& hardwareName, const std::string& operatorMode, int attackSteps = 6, int decaySteps = 4);
    static ProfilingSession createDelaySuite(const std::string& hardwareName, const std::string& operatorMode, int timeSteps = 8, int fbSteps = 4);
    static ProfilingSession createWaveShaperSuite(const std::string& hardwareName, const std::string& operatorMode, int driveSteps = 10);
    static ProfilingSession createGainVcaSuite(const std::string& hardwareName, const std::string& operatorMode, int gainSteps = 10);
    static ProfilingSession createDefaultMockSession();

private:
    ProfilingMetadata metadata;
    std::vector<TestCase> testCases;
};

} // namespace abdaudiolab::core
