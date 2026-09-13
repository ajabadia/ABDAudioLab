#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "audio/LabAudioEngine.h"
#include "audio/LabAudioReceiver.h"
#include "core/ProfilingAudioCapture.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>

using namespace abdaudiolab;

namespace
{

class MockLatencyPlugin : public juce::AudioPluginInstance
{
public:
    MockLatencyPlugin(int initialLatency = 0, float gain = 1.0f)
        : pluginGain(gain)
    {
        setLatencySamples(initialLatency);
    }

    void setMockLatency(int latency) noexcept { setLatencySamples(latency); }
    void setMockGain(float gain) noexcept { pluginGain = gain; }

    const juce::String getName() const override { return "MockLatencyPlugin"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain(pluginGain);
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    void fillInPluginDescription(juce::PluginDescription&) const override {}

private:
    float pluginGain { 1.0f };
};

} // namespace

TEST_CASE("LabAudioReceiver - Plugin Lookahead and Latency Compensation", "[PluginSafety]")
{
    audio::LabAudioReceiver receiver;
    receiver.prepare(48000.0, 2.0);

    SECTION("Zero latency compensation records verbatim without trimming")
    {
        receiver.setLatencyCompensationSamples(0);
        CHECK(receiver.getLatencyCompensationSamples() == 0);

        receiver.armCapture(500, 0.0f);

        std::vector<float> mockBlock(500, 0.75f);
        receiver.processBlock(mockBlock.data(), 500);

        REQUIRE(receiver.isFinished());

        std::vector<float> captured;
        REQUIRE(receiver.retrieveRecordedData(captured));
        REQUIRE(captured.size() == 500);
        CHECK_THAT(captured[0], Catch::Matchers::WithinAbs(0.75f, 1e-4f));
        CHECK_THAT(captured[499], Catch::Matchers::WithinAbs(0.75f, 1e-4f));
    }

    SECTION("Latency compensation extends target samples and discards initial pipeline delay")
    {
        constexpr int mockLatency = 128;
        constexpr int requestedSamples = 500;
        receiver.setLatencyCompensationSamples(mockLatency);
        CHECK(receiver.getLatencyCompensationSamples() == mockLatency);

        // armCapture should record requestedSamples + mockLatency = 628 samples
        receiver.armCapture(requestedSamples, 0.0f);

        // Simulate plugin output: first 128 samples are 0.0f (lookahead silence), followed by signal 0.8f
        std::vector<float> pluginOutput(requestedSamples + mockLatency, 0.0f);
        for (size_t i = mockLatency; i < pluginOutput.size(); ++i)
        {
            pluginOutput[i] = 0.8f;
        }

        // Deliver block to receiver
        receiver.processBlock(pluginOutput.data(), static_cast<int>(pluginOutput.size()));

        REQUIRE(receiver.isFinished());

        std::vector<float> captured;
        REQUIRE(receiver.retrieveRecordedData(captured));

        // The captured buffer should have exactly the requested samples (500)
        REQUIRE(captured.size() == requestedSamples);

        // Sample 0 of retrieved data MUST be the true signal (0.8f), NOT the initial delay (0.0f)
        CHECK_THAT(captured[0], Catch::Matchers::WithinAbs(0.8f, 1e-4f));
        CHECK_THAT(captured[requestedSamples - 1], Catch::Matchers::WithinAbs(0.8f, 1e-4f));
    }
}

TEST_CASE("LabAudioEngine - Dynamic Plugin Latency Inspection & Propagation", "[PluginSafety]")
{
    audio::LabAudioEngine engine;

    MockLatencyPlugin plugin(256, 1.0f);

    SECTION("Attaching plugin configures receiver latency compensation")
    {
        engine.setActivePluginInstance(&plugin, 48000.0, 512);
        CHECK(engine.getActivePluginInstance() == &plugin);
        CHECK(engine.getPluginLatencySamples() == 256);
        CHECK(engine.getResponseReceiver().getLatencyCompensationSamples() == 256);

        // Dynamic change of latency (e.g. plugin oversampling switched to 4x)
        plugin.setMockLatency(512);

        // Next audio processing block dynamically updates receiver latency compensation
        float dummyIn[512] = { 0.0f };
        float dummyOut[512] = { 0.0f };
        const float* inChannels[1] = { dummyIn };
        float* outChannels[1] = { dummyOut };
        juce::AudioIODeviceCallbackContext dummyContext;

        engine.audioDeviceIOCallbackWithContext(inChannels, 1, outChannels, 1, 512, dummyContext);

        CHECK(engine.getPluginLatencySamples() == 512);
        CHECK(engine.getResponseReceiver().getLatencyCompensationSamples() == 512);

        // Detaching plugin resets latency compensation
        engine.setActivePluginInstance(nullptr);
        CHECK(engine.getActivePluginInstance() == nullptr);
        CHECK(engine.getPluginLatencySamples() == 0);
        CHECK(engine.getResponseReceiver().getLatencyCompensationSamples() == 0);
    }
}

TEST_CASE("LabAudioEngine - Virtual Plugin Auto-Trim Digital to -3 dBFS", "[PluginSafety]")
{
    audio::LabAudioEngine engine;
    constexpr float targetHeadroomLinear = 0.70794578f; // -3.0 dBFS

    SECTION("High gain plugin (+6 dB / 2.0x) is normalized down to -3 dBFS")
    {
        MockLatencyPlugin hotPlugin(64, 2.0f);
        engine.setActivePluginInstance(&hotPlugin, 48000.0, 512);

        float calibratedGain = engine.calibratePluginDigitalTrim(-3.0f, 0.1);
        float expectedGain = targetHeadroomLinear / 2.0f; // ~0.35397

        CHECK_THAT(calibratedGain, Catch::Matchers::WithinAbs(expectedGain, 0.02f));
        CHECK_THAT(engine.getInputAutoTrim(), Catch::Matchers::WithinAbs(expectedGain, 0.02f));

        engine.setActivePluginInstance(nullptr);
    }

    SECTION("Low gain plugin (-12 dB / 0.25x) is normalized up to -3 dBFS")
    {
        MockLatencyPlugin quietPlugin(64, 0.25f);
        engine.setActivePluginInstance(&quietPlugin, 48000.0, 512);

        float calibratedGain = engine.calibratePluginDigitalTrim(-3.0f, 0.1);
        float expectedGain = targetHeadroomLinear / 0.25f; // ~2.83178

        CHECK_THAT(calibratedGain, Catch::Matchers::WithinAbs(expectedGain, 0.05f));
        CHECK_THAT(engine.getInputAutoTrim(), Catch::Matchers::WithinAbs(expectedGain, 0.05f));

        engine.setActivePluginInstance(nullptr);
    }

    SECTION("performAutoGainTrim dispatches to digital trim when plugin is active")
    {
        MockLatencyPlugin plugin(0, 1.4142f);
        engine.setActivePluginInstance(&plugin, 48000.0, 512);

        engine.performAutoGainTrim(-3.0f);
        float expectedGain = targetHeadroomLinear / 1.4142f; // ~0.5006

        CHECK_THAT(engine.getInputAutoTrim(), Catch::Matchers::WithinAbs(expectedGain, 0.03f));

        engine.setActivePluginInstance(nullptr);
    }
}

TEST_CASE("ProfilingAudioCapture - Latency Passthrough Accessors", "[PluginSafety]")
{
    audio::LabAudioReceiver receiver;
    audio::LabStimulusGenerator generator;
    core::ProfilingAudioCapture capture(receiver, generator);

    capture.setLatencyCompensationSamples(192);
    CHECK(capture.getLatencyCompensationSamples() == 192);
    CHECK(receiver.getLatencyCompensationSamples() == 192);

    capture.setLatencyCompensationSamples(0);
    CHECK(capture.getLatencyCompensationSamples() == 0);
}
