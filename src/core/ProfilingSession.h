#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "HardwareContractRegistry.h"
#include "../audio/LabStimulusGenerator.h"

namespace abdaudiolab::core
{

struct ParameterStep
{
    int paramIndex { 1 };
    std::string paramName;
    float normalizedValue { 0.0f };
    int rawValue { 0 };
    std::string controlType { "Knob" }; // "Knob", "Slider", "JackPort", "Switch"
    float minNormalized { 0.0f };       // 0.0 to 1.0 (Start range bound)
    float maxNormalized { 1.0f };       // 0.0 to 1.0 (End range bound)
    std::string id;                     // Unique control ID
    int sortOrder { 0 };                // Order priority weight
};

struct TestCase
{
    int queueItemIndex { 0 };
    int pointIndexInTest { 0 };
    int totalPointsInTest { 1 };
    std::string testId;
    std::string functionalBlockType; // TimeDynamic, SpectrumFilter, AmplitudeGain, WaveShaper, CyclicModulator, NoiseFloor
    audio::StimulusType stimulusType { audio::StimulusType::LogFarinaSweep };
    double stimulusDurationSec { 2.0 };
    float startFreqHz { 20.0f };
    float endFreqHz { 20000.0f };
    int numPasses { 1 };
    double stabilizationWaitMs { 50.0 };
    std::string captureMode { "FIXED_TIME" }; // "FIXED_TIME", "ADAPTIVE_ENVELOPE"

    // 1.7.13 Autonomous synthesizer excitation & MIDI articulation
    bool isAutonomousSynth { false };
    ExcitationMode excitationMode { ExcitationMode::AudioSweep };
    int midiChannel { 1 };
    int midiNoteNumber { 60 };           // Middle C (C4)
    float midiVelocity { 0.8f };         // Velocity ~100
    float noteGateDurationSec { 0.0f };  // 0 = use stimulusDurationSec

    // 1.7.16 Dynamic Preset Configuration & Measurement Recipe
    MeasurementPresetRecipe presetRecipe;

    // 1.7.2 Targeted Point Patching
    int globalPointIndex { -1 };
    std::string pointId;

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

    // 1.7.12 Laboratory environmental conditions & observations
    std::string operatorNotes;
    float ambientTemperatureC { 22.0f };
    int warmupTimeMinutes { 15 };
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
    void addTestCase(const TestCase& tc) { testCases.push_back(tc); }
    void setTestCases(const std::vector<TestCase>& tcs) { testCases = tcs; }
    void clearTestCases() { testCases.clear(); }

    void setIsPatchSession(bool isPatch) noexcept { isPatchSessionFlag = isPatch; }
    [[nodiscard]] bool isPatchSession() const noexcept { return isPatchSessionFlag; }

    // Pre-built profiling test suites
    static ProfilingSession createFilterSuite(const std::string& hardwareName, const std::string& operatorMode, int cutSteps = 8, int resSteps = 4);
    static ProfilingSession createAdsrSuite(const std::string& hardwareName, const std::string& operatorMode, int attackSteps = 6, int decaySteps = 4);
    static ProfilingSession createDelaySuite(const std::string& hardwareName, const std::string& operatorMode, int timeSteps = 8, int fbSteps = 4);
    static ProfilingSession createWaveShaperSuite(const std::string& hardwareName, const std::string& operatorMode, int driveSteps = 10);
    static ProfilingSession createGainVcaSuite(const std::string& hardwareName, const std::string& operatorMode, int gainSteps = 10);
    static ProfilingSession createChorusModulatorSuite(const std::string& hardwareName, const std::string& operatorMode, int rateSteps = 8, int depthSteps = 4);
    static ProfilingSession createDefaultMockSession();

    /**
     * @brief Generates a completely automated live scanning suite tailored for the Casio CZ emulated core (VES / MAME).
     * Integrates raw SysEx templates, closed-loop PCM bursts, and adaptive envelope triggers across DCW, ENV, and DCO.
     */
    static ProfilingSession createCasioCzSuite(const std::string& hardwareName = "casio_cz101_mame_ves",
                                               const std::string& operatorMode = "VIRTUAL_LOOPBACK_ASIO",
                                               int dcwSteps = 100,
                                               int envSteps = 8);

    juce::var toDynamicVar() const;
    std::string saveProfileToJson() const;
    bool exportSessionToJsonFile(const std::string& filePath) const;


private:
    ProfilingMetadata metadata;
    std::vector<TestCase> testCases;
    bool isPatchSessionFlag { false };
};

} // namespace abdaudiolab::core
