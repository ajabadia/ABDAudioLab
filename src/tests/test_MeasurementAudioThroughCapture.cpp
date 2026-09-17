/**
 * @file test_MeasurementAudioThroughCapture.cpp
 * @brief Catch2 unit tests for audio-through sweep capture and Farina coordination (Phase 20.10.2 - T2).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "measurement/MeasurementCaptureCoordinator.h"
#include "measurement/adapters/FilterMeasurementAdapter.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <numbers>
#include <limits>
#include <vector>

using namespace abdaudiolab::measurement;

namespace
{

class MockAudioFilterProcessor : public juce::AudioProcessor
{
public:
    enum class FaultMode
    {
        None,
        ThrowOnPrepare,
        OutputSilence,
        OutputNaN,
        SimulateLatency
    };

    FaultMode faultMode { FaultMode::None };
    int simulatedLatency { 0 };

    // Simple 2-pole LowPass IIR Filter
    float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f };
    float a1 { 0.0f }, a2 { 0.0f };
    float z1 { 0.0f }, z2 { 0.0f };

    MockAudioFilterProcessor()
        : AudioProcessor(BusesProperties()
                            .withInput("Input", juce::AudioChannelSet::stereo(), true)
                            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    void setLowPass(double sampleRate, double cutoffHz, double q = 0.7071)
    {
        double w0 = 2.0 * std::numbers::pi * cutoffHz / sampleRate;
        double alpha = std::sin(w0) / (2.0 * q);
        double cosw = std::cos(w0);

        double a0 = 1.0 + alpha;
        b0 = static_cast<float>((1.0 - cosw) / (2.0 * a0));
        b1 = static_cast<float>((1.0 - cosw) / a0);
        b2 = static_cast<float>((1.0 - cosw) / (2.0 * a0));
        a1 = static_cast<float>((-2.0 * cosw) / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        juce::ignoreUnused(sampleRate, samplesPerBlock);
        if (faultMode == FaultMode::ThrowOnPrepare)
            throw std::runtime_error("Hardware bus connection failed in prepareToPlay");

        z1 = 0.0f;
        z2 = 0.0f;

        if (faultMode == FaultMode::SimulateLatency)
            setLatencySamples(simulatedLatency);
        else
            setLatencySamples(0);
    }

    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        int numSamples = buffer.getNumSamples();
        if (numSamples == 0)
            return;

        if (faultMode == FaultMode::OutputSilence)
        {
            buffer.clear();
            return;
        }

        if (faultMode == FaultMode::OutputNaN)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = 0.5f;
                if (numSamples > 5)
                    data[5] = std::numeric_limits<float>::quiet_NaN();
            }
            return;
        }

        // Normal filter processing (mono/stereo)
        float* chL = buffer.getWritePointer(0);
        float* chR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float inSample = chL[i];
            float y = b0 * inSample + z1;
            z1 = b1 * inSample - a1 * y + z2;
            z2 = b2 * inSample - a2 * y;

            chL[i] = y;
            if (chR != nullptr)
                chR[i] = y;
        }
    }

    // Boilerplate Juce AudioProcessor overrides
    const juce::String getName() const override { return "MockAudioFilterProcessor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
};

} // namespace

TEST_CASE("MeasurementCaptureCoordinator - Audio-Through Known IIR LowPass Filter", "[capture][filter][through]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 1.0;

    MeasurementSpec spec;
    spec.measurementId = "meas-capture-through-butterworth";
    spec.measurementType = "filter";
    spec.measurementDomain = "directTransferFunction";
    spec.filterTopology = "lowPass";
    spec.stimulus.type = StimulusType::logSineSweep;
    spec.stimulus.startFreqHz = 20.0f;
    spec.stimulus.endFreqHz = 20000.0f;
    spec.stimulus.durationSec = durationSec;
    spec.stimulus.levelDbfs = -6.0f;
    spec.execution.sampleRateHz = sampleRate;
    spec.execution.blockSize = 512;

    MockAudioFilterProcessor processor;
    processor.setLowPass(sampleRate, 1000.0, 0.7071);

    auto capture = MeasurementCaptureCoordinator::captureAudioThrough(&processor, spec, 512);

    CHECK(capture.status == MeasurementStatus::completed);
    CHECK(capture.measurementDomain == "directTransferFunction");
    CHECK(capture.numSamples == static_cast<int64_t>(sampleRate * durationSec));
    CHECK(capture.capturedAudio.size() == capture.stimulusAudio.size());
    CHECK_FALSE(capture.stimulusSha256.empty());
    CHECK_FALSE(capture.capturedAudioSha256.empty());

    // Stimulus and captured audio must have distinct hashes since signal was filtered
    CHECK(capture.stimulusSha256 != capture.capturedAudioSha256);

    // End-to-end integration: feed captured audio directly into FilterMeasurementAdapter
    auto measResult = FilterMeasurementAdapter::measure(spec, capture.capturedAudio, sampleRate);
    CHECK(measResult.status == MeasurementStatus::completed);
    CHECK(measResult.observability.status == "observed");

    auto itCutoff = std::find_if(measResult.metrics.begin(), measResult.metrics.end(),
        [](const MeasurementMetric& m) { return m.name == "cutoffFrequency"; });
    REQUIRE(itCutoff != measResult.metrics.end());
    CHECK(itCutoff->status.toStdString() == "observed");
    CHECK(itCutoff->value >= 900.0);
    CHECK(itCutoff->value <= 1150.0);
}

TEST_CASE("MeasurementCaptureCoordinator - Variable and Partial Block Sizes", "[capture][filter][blocks]")
{
    const double sampleRate = 48000.0;
    const double durationSec = 0.35; // 16800 samples -> not a multiple of 512

    MeasurementSpec spec;
    spec.measurementId = "meas-capture-partial-blocks";
    spec.measurementType = "filter";
    spec.stimulus.type = StimulusType::logSineSweep;
    spec.stimulus.durationSec = durationSec;
    spec.execution.sampleRateHz = sampleRate;

    MockAudioFilterProcessor processor;
    processor.setLowPass(sampleRate, 2000.0, 1.0);

    // Use odd non-power-of-two block size (137 samples) to force multiple partial blocks
    auto capture = MeasurementCaptureCoordinator::captureAudioThrough(&processor, spec, 137);

    CHECK(capture.status == MeasurementStatus::completed);
    size_t expectedSamples = static_cast<size_t>(std::lround(sampleRate * durationSec));
    CHECK(capture.capturedAudio.size() == expectedSamples);
    CHECK(capture.stimulusAudio.size() == expectedSamples);
    CHECK(capture.numSamples == static_cast<int64_t>(expectedSamples));
}

TEST_CASE("MeasurementCaptureCoordinator - Latency Registration", "[capture][filter][latency]")
{
    const double sampleRate = 48000.0;
    MeasurementSpec spec;
    spec.stimulus.durationSec = 0.2;
    spec.execution.sampleRateHz = sampleRate;

    MockAudioFilterProcessor processor;
    processor.faultMode = MockAudioFilterProcessor::FaultMode::SimulateLatency;
    processor.simulatedLatency = 64; // e.g. 64 samples FIR/lookahead latency

    auto capture = MeasurementCaptureCoordinator::captureAudioThrough(&processor, spec, 512);

    CHECK(capture.status == MeasurementStatus::completed);
    CHECK(capture.latencySamples == 64);
}

TEST_CASE("MeasurementCaptureCoordinator - Error Isolation and Fault Rejection", "[capture][filter][faults]")
{
    const double sampleRate = 48000.0;
    MeasurementSpec spec;
    spec.stimulus.durationSec = 0.2;
    spec.execution.sampleRateHz = sampleRate;

    SECTION("Null processor returns failed without crash")
    {
        auto capture = MeasurementCaptureCoordinator::captureAudioThrough(nullptr, spec);
        CHECK(capture.status == MeasurementStatus::failed);
        CHECK(capture.reason == "target_null");
        CHECK(capture.capturedAudio.empty());
    }

    SECTION("PrepareToPlay failure is safely isolated")
    {
        MockAudioFilterProcessor processor;
        processor.faultMode = MockAudioFilterProcessor::FaultMode::ThrowOnPrepare;

        auto capture = MeasurementCaptureCoordinator::captureAudioThrough(&processor, spec);
        CHECK(capture.status == MeasurementStatus::failed);
        CHECK(capture.reason.find("prepare_failed") != std::string::npos);
        CHECK(capture.capturedAudio.empty());
    }

    SECTION("Non-finite NaN samples return invalid with zero leakage")
    {
        MockAudioFilterProcessor processor;
        processor.faultMode = MockAudioFilterProcessor::FaultMode::OutputNaN;

        auto capture = MeasurementCaptureCoordinator::captureAudioThrough(&processor, spec);
        CHECK(capture.status == MeasurementStatus::invalid);
        CHECK(capture.reason == "non_finite_audio_samples");
        CHECK(capture.capturedAudio.empty()); // Zero leakage of corrupt data
    }

    SECTION("Silent output classified as unreliable")
    {
        MockAudioFilterProcessor processor;
        processor.faultMode = MockAudioFilterProcessor::FaultMode::OutputSilence;

        auto capture = MeasurementCaptureCoordinator::captureAudioThrough(&processor, spec);
        CHECK(capture.status == MeasurementStatus::unreliable);
        CHECK(capture.reason == "silent_or_flat_signal");
        CHECK_FALSE(capture.capturedAudio.empty());
        CHECK_FALSE(capture.capturedAudioSha256.empty());
    }
}
