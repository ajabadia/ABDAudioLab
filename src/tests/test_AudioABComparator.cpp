#include <catch2/catch_test_macros.hpp>
#include "../math/AudioABComparator.h"
#include "../math/AudioABVerdictEngine.h"
#include "../gui/AudioABVerificationModal.h"
#include <cmath>
#include <numbers>

using namespace abdaudiolab::math;

TEST_CASE("AudioABComparator - Exact Signal Match & Verdict Pass", "[comparator][audioab][verdict]")
{
    AudioABSignal refSignal;
    refSignal.sampleRate = 44100.0;
    refSignal.numChannels = 1;
    refSignal.buffer.setSize(1, 44100);

    // 1 kHz pure sine wave
    float* refData = refSignal.buffer.getWritePointer(0);
    for (int i = 0; i < 44100; ++i)
    {
        refData[i] = std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * static_cast<float>(i) / 44100.0f);
    }
    refSignal.originalNumSamples = 44100;

    AudioABSignal capSignal;
    capSignal.sampleRate = 44100.0;
    capSignal.numChannels = 1;
    capSignal.buffer.setSize(1, 44100);

    float* capData = capSignal.buffer.getWritePointer(0);
    for (int i = 0; i < 44100; ++i)
    {
        capData[i] = std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f * static_cast<float>(i) / 44100.0f);
    }
    capSignal.originalNumSamples = 44100;

    AudioABRunContext ctx;
    ctx.runId = "test-perfect-match";

    AudioABComparatorConfig config;
    config.trimLeadingSilence = false;
    config.trimTrailingSilence = false;

    AudioABComparator comparator;
    auto result = comparator.compare(refSignal, capSignal, ctx, config);

    CHECK(result.status == "ok");
    CHECK(result.alignment.sampleOffset == 0);
    CHECK(result.alignment.correlationPeak > 0.99);
    CHECK(result.time.rmse < 1e-4);

    AudioABVerdictEngine verdictEngine;
    AudioABVerdictTolerances tolerances;
    auto verdict = verdictEngine.evaluate(result, tolerances);

    CHECK(verdict.level == "pass");
    CHECK(verdict.reasonCode == "WITHIN_TOLERANCE");
}

TEST_CASE("AudioABComparator - Sample Rate Mismatch Rejection", "[comparator][audioab][safety]")
{
    AudioABSignal refSignal;
    refSignal.sampleRate = 44100.0;
    refSignal.buffer.setSize(1, 1000);
    refSignal.buffer.clear();

    AudioABSignal capSignal;
    capSignal.sampleRate = 48000.0; // Mismatch
    capSignal.buffer.setSize(1, 1000);
    capSignal.buffer.clear();

    AudioABRunContext ctx;
    AudioABComparatorConfig config;
    AudioABComparator comparator;

    auto result = comparator.compare(refSignal, capSignal, ctx, config);
    CHECK(result.status == "error");
    CHECK(result.reasonCode == "SR_MISMATCH");

    AudioABVerdictEngine verdictEngine;
    AudioABVerdictTolerances tolerances;
    auto verdict = verdictEngine.evaluate(result, tolerances);
    CHECK(verdict.level == "fail");
    CHECK(verdict.reasonCode == "SR_MISMATCH");
}

TEST_CASE("AudioABComparator - Latency Alignment by Cross-Correlation", "[comparator][audioab][alignment]")
{
    AudioABSignal refSignal;
    refSignal.sampleRate = 44100.0;
    refSignal.numChannels = 1;
    refSignal.buffer.setSize(1, 10000);
    refSignal.buffer.clear();
    float* refData = refSignal.buffer.getWritePointer(0);
    refData[100] = 1.0f;
    refSignal.originalNumSamples = 10000;

    AudioABSignal capSignal;
    capSignal.sampleRate = 44100.0;
    capSignal.numChannels = 1;
    capSignal.buffer.setSize(1, 10000);
    capSignal.buffer.clear();
    float* capData = capSignal.buffer.getWritePointer(0);
    // Same impulse delayed by 50 samples
    capData[150] = 1.0f;
    capSignal.originalNumSamples = 10000;

    AudioABRunContext ctx;
    ctx.runId = "test-offset-alignment";

    AudioABComparatorConfig config;
    config.trimLeadingSilence = false;
    config.trimTrailingSilence = false;
    config.enableCrossCorrelation = true;

    AudioABComparator comparator;
    auto result = comparator.compare(refSignal, capSignal, ctx, config);

    CHECK(result.status == "ok");
    CHECK(result.alignment.correlationPeak > 0.99);
}

TEST_CASE("AudioABVerificationModal - GUI Lifecycle and Signal Loading", "[gui][modal][audioab]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    abdaudiolab::gui::AudioABVerificationModal modal;
    modal.setSize(800, 600);

    juce::AudioBuffer<float> refBuf(1, 4096);
    refBuf.clear();
    float* ref = refBuf.getWritePointer(0);
    for (int i = 0; i < 4096; ++i)
        ref[i] = std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * static_cast<float>(i) / 44100.0f);

    juce::AudioBuffer<float> candBuf(1, 4096);
    candBuf.clear();
    float* cand = candBuf.getWritePointer(0);
    for (int i = 0; i < 4096; ++i)
        cand[i] = ref[i] * 0.95f; // Slight gain attenuation (-0.45 dB)

    modal.setReferenceSignal(refBuf, 44100.0, "Ref Tone 440Hz");
    modal.setCandidateSignal(candBuf, 44100.0, "Cand Tone 440Hz");

    // Modal can be shown and dismissed cleanly without errors
    modal.showDialog(nullptr);
    modal.dismissDialog();

    // Verify manual key press handling (escape closes dialog)
    juce::KeyPress escKey(juce::KeyPress::escapeKey);
    CHECK(modal.keyPressed(escKey) == true);
}

