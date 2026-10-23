#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "audio/LabAudioReceiver.h"
#include "audio/ScopedPhysicalLoopbackCapture.h"
#include "audio/LabAudioEngine.h"
#include "hardware/MockHardwareController.h"
#include "math/LoopbackCalibrator.h"
#include <vector>
#include <cmath>

using namespace abdaudiolab;
using namespace abdaudiolab::audio;
using namespace abdaudiolab::hardware;

TEST_CASE("CaptureRequirements Pure Factory", "[audio][receiver][lifecycle]")
{
    SECTION("44.1 kHz creates exactly 57,330 samples (1.0s sweep + 0.2s margin + 0.1s tail)")
    {
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(req.isValid());
        REQUIRE(req.sweepSamples == 44100);
        REQUIRE(req.latencyMarginSamples == 8820);
        REQUIRE(req.decayTailSamples == 4410);
        REQUIRE(req.requiredSamples == 57330);
        REQUIRE_FALSE(req.requireTrigger);
    }

    SECTION("48.0 kHz creates exactly 62,400 samples")
    {
        auto req = CaptureRequirements::makeLoopbackRequirements(48000.0);
        REQUIRE(req.isValid());
        REQUIRE(req.sweepSamples == 48000);
        REQUIRE(req.latencyMarginSamples == 9600);
        REQUIRE(req.decayTailSamples == 4800);
        REQUIRE(req.requiredSamples == 62400);
        REQUIRE_FALSE(req.requireTrigger);
    }

    SECTION("96.0 kHz creates exactly 124,800 samples")
    {
        auto req = CaptureRequirements::makeLoopbackRequirements(96000.0);
        REQUIRE(req.isValid());
        REQUIRE(req.sweepSamples == 96000);
        REQUIRE(req.latencyMarginSamples == 19200);
        REQUIRE(req.decayTailSamples == 9600);
        REQUIRE(req.requiredSamples == 124800);
        REQUIRE_FALSE(req.requireTrigger);
    }

    SECTION("Sample rate 0 returns invalid requirements")
    {
        auto req = CaptureRequirements::makeLoopbackRequirements(0.0);
        REQUIRE_FALSE(req.isValid());
        REQUIRE(req.requiredSamples == 0);
    }

    SECTION("Negative sample rate returns invalid requirements")
    {
        auto req = CaptureRequirements::makeLoopbackRequirements(-44100.0);
        REQUIRE_FALSE(req.isValid());
        REQUIRE(req.requiredSamples == 0);
    }

    SECTION("Zero or negative sweep duration returns invalid requirements")
    {
        auto reqZero = CaptureRequirements::makeLoopbackRequirements(44100.0, 0.0);
        REQUIRE_FALSE(reqZero.isValid());
        REQUIRE(reqZero.requiredSamples == 0);

        auto reqNeg = CaptureRequirements::makeLoopbackRequirements(44100.0, -1.0);
        REQUIRE_FALSE(reqNeg.isValid());
        REQUIRE(reqNeg.requiredSamples == 0);
    }

    SECTION("Negative margin or tail returns invalid requirements")
    {
        auto reqNegMargin = CaptureRequirements::makeLoopbackRequirements(44100.0, 1.0, -0.2, 0.1);
        REQUIRE_FALSE(reqNegMargin.isValid());

        auto reqNegTail = CaptureRequirements::makeLoopbackRequirements(44100.0, 1.0, 0.2, -0.1);
        REQUIRE_FALSE(reqNegTail.isValid());
    }

    SECTION("requiredSamples exceeding ring buffer capacity is rejected safely")
    {
        LabAudioReceiver receiver;
        // Allocate small buffer: 1.0s @ 44.1 kHz = ~48196 samples total
        receiver.prepare(44100.0, 1.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0); // requires 57,330 samples
        REQUIRE(req.requiredSamples > receiver.getRingBufferSize() - 1024);

        bool armed = receiver.armWithRequirements(req);
        REQUIRE_FALSE(armed);
        REQUIRE(receiver.getState() == ReceiverState::Idle);
    }
}

TEST_CASE("CaptureRequirements Derived UI Deadline", "[audio][receiver][lifecycle]")
{
    SECTION("1,300 ms window + 250 ms scheduler grace yields exactly 1,550 ms at all sample rates")
    {
        auto req44 = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(req44, 44100.0, 250) == 1550);

        auto req48 = CaptureRequirements::makeLoopbackRequirements(48000.0);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(req48, 48000.0, 250) == 1550);

        auto req96 = CaptureRequirements::makeLoopbackRequirements(96000.0);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(req96, 96000.0, 250) == 1550);
    }

    SECTION("Grace is configurable and shifts deadline only by that margin")
    {
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(req, 44100.0, 0) == 1300);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(req, 44100.0, 100) == 1400);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(req, 44100.0, 500) == 1800);
    }

    SECTION("Invalid requirements or invalid sample rates do not return valid deadline")
    {
        CaptureRequirements invalidReq;
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(invalidReq, 44100.0) == 0);

        auto validReq = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(validReq, 0.0) == 0);
        REQUIRE(CaptureRequirements::computeUiDeadlineMs(validReq, -44100.0) == 0);
    }
}

TEST_CASE("LabAudioReceiver Lifecycle State Transitions", "[audio][receiver][lifecycle]")
{
    SECTION("Continuous capture transitions Idle -> Recording and starts on first available block")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        REQUIRE(receiver.getState() == ReceiverState::Idle);

        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(receiver.armWithRequirements(req));
        REQUIRE(receiver.getState() == ReceiverState::Recording);

        std::vector<float> block(512, 0.25f);
        receiver.processBlock(block.data(), 512);
        REQUIRE(receiver.getRecordedSampleCount() == 512);
    }

    SECTION("Complete capture transitions Recording -> Finished when requiredSamples reached")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0); // 57330 samples
        REQUIRE(receiver.armWithRequirements(req));

        std::vector<float> block(1024, 0.1f);
        int fed = 0;
        while (fed + 1024 <= req.requiredSamples)
        {
            receiver.processBlock(block.data(), 1024);
            fed += 1024;
        }
        int remainder = req.requiredSamples - fed;
        if (remainder > 0)
        {
            receiver.processBlock(block.data(), remainder);
        }

        REQUIRE(receiver.getRecordedSampleCount() == req.requiredSamples);
        REQUIRE(receiver.isFinished());
    }

    SECTION("Incomplete capture does not reach requiredSamples and remains Recording")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0); // 57330 samples
        REQUIRE(receiver.armWithRequirements(req));

        std::vector<float> block(1000, 0.1f);
        receiver.processBlock(block.data(), 1000);

        REQUIRE(receiver.getRecordedSampleCount() < req.requiredSamples);
        REQUIRE(receiver.getState() == ReceiverState::Recording);
    }

    SECTION("Finalization requested transitions Recording -> FinalizeRequested")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(receiver.armWithRequirements(req));

        receiver.requestFinalizeCapture();
        REQUIRE(receiver.getState() == ReceiverState::FinalizeRequested);
    }

    SECTION("Audio thread handshake acknowledges FinalizeRequested -> Finalized and sets snapshotReady")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(receiver.armWithRequirements(req));

        std::vector<float> block(512, 0.1f);
        receiver.processBlock(block.data(), 512);

        receiver.requestFinalizeCapture();
        REQUIRE(receiver.getState() == ReceiverState::FinalizeRequested);
        REQUIRE_FALSE(receiver.isSnapshotReady());

        // Process next block in audio thread: recognizes FinalizeRequested -> transitions to Finalized
        receiver.processBlock(block.data(), 512);
        REQUIRE(receiver.getState() == ReceiverState::Finalized);
        REQUIRE(receiver.isSnapshotReady());
        // Confirm no further audio was appended after finalization was handled
        REQUIRE(receiver.getRecordedSampleCount() == 512);
    }

    SECTION("Snapshot retrieval before Finalized is rejected without returning samples")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(receiver.armWithRequirements(req));

        std::vector<float> dest = { 1.0f, 2.0f, 3.0f };
        auto status = receiver.retrieveFinalizedSnapshot(dest);

        REQUIRE(status.result == CaptureResult::Invalid);
        REQUIRE(dest.empty());
    }

    SECTION("Incomplete capture snapshot clears destination and reports TimedOutIncomplete")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0); // 57330 samples
        REQUIRE(receiver.armWithRequirements(req));

        std::vector<float> block(10000, 0.2f);
        receiver.processBlock(block.data(), static_cast<int>(block.size())); // 10000 < 57330

        receiver.requestFinalizeCapture();
        std::vector<float> dummy(256, 0.0f);
        receiver.processBlock(dummy.data(), static_cast<int>(dummy.size())); // Acknowledge finalization

        REQUIRE(receiver.isSnapshotReady());

        std::vector<float> dest = { 1.0f, 2.0f, 3.0f };
        auto status = receiver.retrieveFinalizedSnapshot(dest);

        REQUIRE(status.result == CaptureResult::TimedOutIncomplete);
        REQUIRE(status.samplesCaptured == 10000);
        REQUIRE(status.requiredSamples == 57330);
        REQUIRE(dest.empty()); // Guaranteed empty to prevent truncated sweep from reaching Farina
    }

    SECTION("Complete capture snapshot contains exactly the recorded samples and reports Complete")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0); // 57330 samples
        REQUIRE(receiver.armWithRequirements(req));

        std::vector<float> samples(req.requiredSamples);
        for (size_t i = 0; i < samples.size(); ++i)
        {
            samples[i] = static_cast<float>(i) * 0.00001f;
        }

        // Feed exact block
        receiver.processBlock(samples.data(), req.requiredSamples);
        REQUIRE(receiver.isFinished());

        receiver.requestFinalizeCapture();
        REQUIRE(receiver.isSnapshotReady());

        std::vector<float> dest;
        auto status = receiver.retrieveFinalizedSnapshot(dest);

        REQUIRE(status.result == CaptureResult::Complete);
        REQUIRE(status.samplesCaptured == req.requiredSamples);
        REQUIRE(dest.size() == static_cast<size_t>(req.requiredSamples));
        REQUIRE(dest.front() == samples.front());
        REQUIRE(dest[100] == samples[100]);
        REQUIRE(dest.back() == samples.back());
    }

    SECTION("Sustained clipping aborts capture and returns empty destination with Aborted result")
    {
        LabAudioReceiver receiver;
        receiver.prepare(44100.0, 3.0);
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(receiver.armWithRequirements(req));

        // Feed sustained clipping (> 700 samples at 1.0f)
        std::vector<float> clippingBlock(800, 1.0f);
        receiver.processBlock(clippingBlock.data(), static_cast<int>(clippingBlock.size()));

        REQUIRE(receiver.isOverloadTriggered());
        REQUIRE(receiver.isSnapshotReady());

        std::vector<float> dest = { 0.5f };
        auto status = receiver.retrieveFinalizedSnapshot(dest);

        REQUIRE(status.result == CaptureResult::Aborted);
        REQUIRE(status.abortReason == CaptureAbortReason::SustainedClipping);
        REQUIRE(dest.empty());
    }
}

TEST_CASE("ScopedPhysicalLoopbackCapture Mock Isolation and Restoration", "[audio][receiver][lifecycle][mock]")
{
    LabAudioEngine engine;
    hardware::MockHardwareController mockController;

    SECTION("Engine outside calibration retains its configured mock hardware")
    {
        engine.setMockHardware(&mockController);
        REQUIRE(engine.getMockHardware() == &mockController);
    }

    SECTION("Scoped guard temporarily isolates engine and restores previous mock on scope exit")
    {
        engine.setMockHardware(&mockController);
        REQUIRE(engine.getMockHardware() == &mockController);

        {
            ScopedPhysicalLoopbackCapture guard(engine);
            REQUIRE(guard.isActive());
            REQUIRE(guard.getPreviousMock() == &mockController);
            // Engine is strictly isolated from mock
            REQUIRE(engine.getMockHardware() == nullptr);
        }

        // Automatic RAII restore
        REQUIRE(engine.getMockHardware() == &mockController);
    }

    SECTION("Scoped guard handles nullptr initial mock gracefully")
    {
        engine.setMockHardware(nullptr);
        REQUIRE(engine.getMockHardware() == nullptr);

        {
            ScopedPhysicalLoopbackCapture guard(engine);
            REQUIRE(guard.isActive());
            REQUIRE(guard.getPreviousMock() == nullptr);
            REQUIRE(engine.getMockHardware() == nullptr);
        }

        REQUIRE(engine.getMockHardware() == nullptr);
    }

    SECTION("Explicit restore() restores mock immediately and is idempotent")
    {
        engine.setMockHardware(&mockController);

        ScopedPhysicalLoopbackCapture guard(engine);
        REQUIRE(engine.getMockHardware() == nullptr);

        guard.restore();
        REQUIRE_FALSE(guard.isActive());
        REQUIRE(engine.getMockHardware() == &mockController);

        // Second restore does nothing
        guard.restore();
        REQUIRE(engine.getMockHardware() == &mockController);
    }

    SECTION("Mock isolation during capture lifecycle: success, abort, and timeout")
    {
        engine.setMockHardware(&mockController);

        // Simulate successful calibration run
        {
            ScopedPhysicalLoopbackCapture guard(engine);
            REQUIRE(engine.getMockHardware() == nullptr);

            auto& receiver = engine.getResponseReceiver();
            receiver.prepare(44100.0, 3.0);
            auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
            REQUIRE(receiver.armWithRequirements(req));

            std::vector<float> physicalInput(static_cast<size_t>(req.requiredSamples), 0.1f);
            receiver.processBlock(physicalInput.data(), static_cast<int>(physicalInput.size()));

            std::vector<float> snapshot;
            auto status = receiver.retrieveFinalizedSnapshot(snapshot);
            REQUIRE(status.result == CaptureResult::Complete);
            REQUIRE(snapshot.size() == static_cast<size_t>(req.requiredSamples));
        }
        REQUIRE(engine.getMockHardware() == &mockController);

        // Simulate clipping abort run
        {
            ScopedPhysicalLoopbackCapture guard(engine);
            REQUIRE(engine.getMockHardware() == nullptr);

            auto& receiver = engine.getResponseReceiver();
            receiver.prepare(44100.0, 3.0);
            auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
            REQUIRE(receiver.armWithRequirements(req));

            std::vector<float> clip(800, 1.0f);
            receiver.processBlock(clip.data(), static_cast<int>(clip.size()));

            std::vector<float> snapshot;
            auto status = receiver.retrieveFinalizedSnapshot(snapshot);
            REQUIRE(status.result == CaptureResult::Aborted);
        }
        REQUIRE(engine.getMockHardware() == &mockController);

        // Simulate timeout incomplete run
        {
            ScopedPhysicalLoopbackCapture guard(engine);
            REQUIRE(engine.getMockHardware() == nullptr);

            auto& receiver = engine.getResponseReceiver();
            receiver.prepare(44100.0, 3.0);
            auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
            REQUIRE(receiver.armWithRequirements(req));

            receiver.requestFinalizeCapture();
            std::vector<float> dummy(256, 0.0f);
            receiver.processBlock(dummy.data(), static_cast<int>(dummy.size()));
            REQUIRE(receiver.isSnapshotReady());

            std::vector<float> snapshot;
            auto status = receiver.retrieveFinalizedSnapshot(snapshot);
            REQUIRE(status.result == CaptureResult::TimedOutIncomplete);
        }
        REQUIRE(engine.getMockHardware() == &mockController);
    }
}

TEST_CASE("Noise Baseline Two-Tier Safety Abort Protocol", "[audio][receiver][baseline][safety]")
{
    LabAudioReceiver receiver;
    receiver.prepare(44100.0, 3.0);
    constexpr int kBaselineSamples = 17640; // 400 ms @ 44.1k

    SECTION("Baseline limpio se captura y finaliza completamente")
    {
        receiver.armBaselineCapture(kBaselineSamples);
        REQUIRE(receiver.isBaselineMode());

        std::vector<float> cleanSilence(static_cast<size_t>(kBaselineSamples), 0.0001f); // ~ -80 dBFS
        receiver.processBlock(cleanSilence.data(), static_cast<int>(cleanSilence.size()));

        REQUIRE(receiver.isFinished());
        REQUIRE_FALSE(receiver.isOverloadTriggered());

        std::vector<float> dest;
        receiver.forceFinish();
        auto status = receiver.retrieveFinalizedSnapshot(dest);
        REQUIRE(status.result == CaptureResult::Complete);
        REQUIRE(status.abortReason == CaptureAbortReason::None);
        REQUIRE(dest.size() == static_cast<size_t>(kBaselineSamples));
    }

    SECTION("Pico de emergencia > -6 dBFS en baseline aborta inmediatamente como PossibleFeedbackLoop en bloque 1")
    {
        receiver.armBaselineCapture(kBaselineSamples);
        REQUIRE(receiver.isBaselineMode());

        // Block with a burst of 0.6f (> -6 dBFS ≈ 0.501)
        std::vector<float> feedbackBurst(256, 0.0f);
        feedbackBurst[10] = 0.65f; // Possible feedback peak
        receiver.processBlock(feedbackBurst.data(), static_cast<int>(feedbackBurst.size()));

        REQUIRE(receiver.isOverloadTriggered());
        REQUIRE(receiver.isSnapshotReady());

        std::vector<float> dest;
        auto status = receiver.retrieveFinalizedSnapshot(dest);
        REQUIRE(status.result == CaptureResult::Aborted);
        REQUIRE(status.abortReason == CaptureAbortReason::PossibleFeedbackLoop);
        REQUIRE(dest.empty());
    }

    SECTION("Clipping digital durante baseline aborta inmediatamente como SustainedClipping en bloque 1")
    {
        receiver.armBaselineCapture(kBaselineSamples);
        REQUIRE(receiver.isBaselineMode());

        std::vector<float> clipBurst(256, 0.0f);
        clipBurst[5] = 1.0f; // Saturated sample
        receiver.processBlock(clipBurst.data(), static_cast<int>(clipBurst.size()));

        REQUIRE(receiver.isOverloadTriggered());
        REQUIRE(receiver.isSnapshotReady());

        std::vector<float> dest;
        auto status = receiver.retrieveFinalizedSnapshot(dest);
        REQUIRE(status.result == CaptureResult::Aborted);
        REQUIRE(status.abortReason == CaptureAbortReason::SustainedClipping);
        REQUIRE(dest.empty());
    }

    SECTION("En modo sweep normal una muestra a -4.4 dBFS (0.6f) NO aborta")
    {
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(receiver.armWithRequirements(req));
        REQUIRE_FALSE(receiver.isBaselineMode());

        // A single block containing 0.6f should NOT abort normal sweep (sweeps typically peak at -3 dBFS = 0.707)
        std::vector<float> sweepBlock(256, 0.0f);
        sweepBlock[10] = 0.65f;
        receiver.processBlock(sweepBlock.data(), static_cast<int>(sweepBlock.size()));

        REQUIRE_FALSE(receiver.isOverloadTriggered());
        REQUIRE(receiver.getState() == ReceiverState::Recording);
    }
}

TEST_CASE("Step 2A / 2B Two-Stage Physical Loopback Safety and Isolation", "[audio][receiver][baseline][step2]")
{
    LabAudioEngine engine;
    hardware::MockHardwareController mockController;

    SECTION("MockHardware active outside Step 2, strictly null and isolated during 2A and 2B, and restored upon finish")
    {
        engine.setMockHardware(&mockController);
        REQUIRE(engine.getMockHardware() == &mockController);
        REQUIRE_FALSE(engine.isPhysicalLoopbackIsolationActive());

        {
            // Enter Step 2 (both 2A and 2B)
            ScopedPhysicalLoopbackCapture guard(engine);
            REQUIRE(engine.getMockHardware() == nullptr);
            REQUIRE(engine.isPhysicalLoopbackIsolationActive());
            REQUIRE(engine.isCaptureSourcePhysicalAdc());

            // Substep 2A: Baseline
            auto& receiver = engine.getResponseReceiver();
            receiver.prepare(44100.0, 3.0);
            receiver.armBaselineCapture(17640);
            REQUIRE(receiver.isBaselineMode());

            std::vector<float> quiet(17640, 0.0001f);
            receiver.processBlock(quiet.data(), static_cast<int>(quiet.size()));
            REQUIRE(receiver.isFinished());

            std::vector<float> baselineSnap;
            auto baseStatus = receiver.retrieveFinalizedSnapshot(baselineSnap);
            REQUIRE(baseStatus.result == CaptureResult::Complete);

            // Transition gap between 2A and 2B: guard remains active!
            REQUIRE(engine.getMockHardware() == nullptr);
            REQUIRE(engine.isPhysicalLoopbackIsolationActive());
            REQUIRE(engine.isCaptureSourcePhysicalAdc());

            // Substep 2B: Loopback Sweep
            auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
            REQUIRE(receiver.armWithRequirements(req));
            REQUIRE_FALSE(receiver.isBaselineMode());

            std::vector<float> sweepData(static_cast<size_t>(req.requiredSamples), 0.5f);
            receiver.processBlock(sweepData.data(), static_cast<int>(sweepData.size()));
            REQUIRE(receiver.isFinished());

            std::vector<float> loopbackSnap;
            auto loopStatus = receiver.retrieveFinalizedSnapshot(loopbackSnap);
            REQUIRE(loopStatus.result == CaptureResult::Complete);
            REQUIRE(loopbackSnap.size() == static_cast<size_t>(req.requiredSamples));
        }

        // Exited Step 2: original mock hardware is restored
        REQUIRE(engine.getMockHardware() == &mockController);
        REQUIRE_FALSE(engine.isPhysicalLoopbackIsolationActive());
    }

    SECTION("Active plugin instance cannot hijack capture source during physical isolation")
    {
        engine.setPhysicalLoopbackIsolation(true);
        REQUIRE(engine.isPhysicalLoopbackIsolationActive());
        REQUIRE(engine.isCaptureSourcePhysicalAdc());

        // Under physical isolation, capture source MUST be physical ADC
        REQUIRE(engine.isCaptureSourcePhysicalAdc());

        engine.setPhysicalLoopbackIsolation(false);
        REQUIRE_FALSE(engine.isPhysicalLoopbackIsolationActive());
    }

    SECTION("Missing loopback cable in 2B passes 2A baseline but fails 2B by peak level without crashing")
    {
        ScopedPhysicalLoopbackCapture guard(engine);
        auto& receiver = engine.getResponseReceiver();
        receiver.prepare(44100.0, 3.0);

        // 2A: Silence passes cleanly
        receiver.armBaselineCapture(17640);
        std::vector<float> silence(17640, 0.0f);
        receiver.processBlock(silence.data(), static_cast<int>(silence.size()));
        REQUIRE(receiver.isFinished());

        std::vector<float> baseSnap;
        auto baseStatus = receiver.retrieveFinalizedSnapshot(baseSnap);
        REQUIRE(baseStatus.result == CaptureResult::Complete);

        // 2B: Farina sweep played into empty input (cable missing -> pure silence returned)
        auto req = CaptureRequirements::makeLoopbackRequirements(44100.0);
        REQUIRE(receiver.armWithRequirements(req));
        std::vector<float> missingCableInput(static_cast<size_t>(req.requiredSamples), 0.0f);
        receiver.processBlock(missingCableInput.data(), static_cast<int>(missingCableInput.size()));
        REQUIRE(receiver.isFinished());

        std::vector<float> sweepSnap;
        auto sweepStatus = receiver.retrieveFinalizedSnapshot(sweepSnap);
        REQUIRE(sweepStatus.result == CaptureResult::Complete);

        // Analysis fails by low peak level, does not crash or corrupt state
        auto calib = math::LoopbackCalibrator::analyzeLoopback(
            sweepSnap, 44100.0, 1.0, 20.0f, 20000.0f, -3.0f, -100.0f);
        REQUIRE_FALSE(calib.isCalibrated);
        CHECK(calib.peakInDbfs < -40.0f);
    }
}

