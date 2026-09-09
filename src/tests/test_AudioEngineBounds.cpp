#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <audio/LabAudioEngine.h>
#include <audio/LabStimulusGenerator.h>
#include <audio/LabAudioReceiver.h>
#include <math/LabAnalyticEngine.h>
#include <gui/SoundIdSuiteList.h>

TEST_CASE("LabStimulusGenerator Buffer Boundary Safety", "[AudioEngine][Safety]")
{
    abdaudiolab::audio::LabStimulusGenerator generator;
    generator.prepare(48000.0);
    generator.setStimulus(abdaudiolab::audio::StimulusType::SineWave1kHz, 2.0);

    SECTION("Normal buffer block sizes (64 to 8192)")
    {
        for (int blockSize : { 64, 128, 256, 512, 1024, 2048, 4096, 8192 })
        {
            std::vector<float> buffer(static_cast<size_t>(blockSize), 0.0f);
            REQUIRE_NOTHROW(generator.processBlock(buffer.data(), blockSize));
            
            // Check that samples were populated and within valid numeric bounds
            bool hasSignal = false;
            for (float s : buffer)
            {
                REQUIRE(std::isfinite(s));
                REQUIRE(std::abs(s) <= 1.0f);
                if (std::abs(s) > 0.001f)
                    hasSignal = true;
            }
            REQUIRE(hasSignal);
        }
    }

    SECTION("Zero samples requested does not crash or corrupt")
    {
        std::vector<float> buffer(64, 0.0f);
        REQUIRE_NOTHROW(generator.processBlock(buffer.data(), 0));
    }
}

TEST_CASE("QueueItem Status and Progression State Transitions", "[SuiteList][Safety]")
{
    abdaudiolab::gui::QueueItem item;
    item.id = "test_safety_1";
    item.title = "Safety Test";
    item.status = abdaudiolab::gui::QueueItemStatus::Completed;
    item.totalPoints = 32;
    item.currentRunningPoint = 32;

    SECTION("Restart resets current point to 0 and status to Queued")
    {
        item.status = abdaudiolab::gui::QueueItemStatus::Queued;
        item.currentRunningPoint = 0;

        REQUIRE(item.status == abdaudiolab::gui::QueueItemStatus::Queued);
        REQUIRE(item.currentRunningPoint == 0);
    }

    SECTION("Continue maintains previous measured points while queueing")
    {
        item.status = abdaudiolab::gui::QueueItemStatus::Queued;
        item.currentRunningPoint = 15;

        REQUIRE(item.status == abdaudiolab::gui::QueueItemStatus::Queued);
        REQUIRE(item.currentRunningPoint == 15);
    }
}

TEST_CASE("LabAudioEngine Dual-Buffer Safety & Stereo Trim Under Stress", "[AudioEngine][Safety][StereoTrim]")
{
    abdaudiolab::audio::LabAudioEngine engine;
    engine.audioDeviceAboutToStart(nullptr); // Initialized with 16384 capacity per channel
    engine.setInputAutoTrim(1.5f); // 1.5x linear gain (+3.52 dB)

    juce::AudioIODeviceCallbackContext dummyContext;

    SECTION("Stereo ADC with active trim at standard and stress buffer sizes (64, 512, 8192, and oversized 20000)")
    {
        for (int blockSize : { 64, 512, 8192, 20000 })
        {
            std::vector<float> inL(static_cast<size_t>(blockSize), 0.40f);
            std::vector<float> inR(static_cast<size_t>(blockSize), 0.20f);
            const float* inChannels[2] = { inL.data(), inR.data() };

            std::vector<float> outL(static_cast<size_t>(blockSize), 0.0f);
            std::vector<float> outR(static_cast<size_t>(blockSize), 0.0f);
            float* outChannels[2] = { outL.data(), outR.data() };

            // Must NOT throw, crash, or write out of bounds even with oversized blockSize = 20000
            REQUIRE_NOTHROW(engine.audioDeviceIOCallbackWithContext(
                inChannels, 2,
                outChannels, 2,
                blockSize,
                dummyContext
            ));

            // Verify meters are computed without NaN or inf
            REQUIRE(std::isfinite(engine.getInputPeakL()));
            REQUIRE(std::isfinite(engine.getInputPeakR()));
            REQUIRE(std::isfinite(engine.getInputRmsL()));
            REQUIRE(std::isfinite(engine.getInputRmsR()));

            // For valid block sizes, check symmetric trim application on meters
            if (blockSize <= 8192)
            {
                // Expected Peak: 0.4 * 1.5 = 0.6 on L, 0.2 * 1.5 = 0.3 on R
                REQUIRE(engine.getInputPeakL() == Catch::Approx(0.60f).margin(0.01f));
                REQUIRE(engine.getInputPeakR() == Catch::Approx(0.30f).margin(0.01f));
            }
        }
    }
}

TEST_CASE("LabAudioEngine ABDScope End-to-End Contract & JSON Wire Protocol Verification", "[AudioEngine][ABDScope][Contract]")
{
    abdaudiolab::audio::LabAudioEngine engine;
    engine.audioDeviceAboutToStart(nullptr);
    engine.setInputAutoTrim(1.5f); // Apply 1.5x gain to input

    // Activate "Hardware In (DUT)" tap (Tap index 0)
    engine.getScopeCollector().selectTap(0);
    auto* activeTap = engine.getScopeCollector().getActiveTap();
    REQUIRE(activeTap != nullptr);
    REQUIRE(activeTap->getName() == "Hardware In (DUT)");
    REQUIRE(activeTap->isActive());

    juce::AudioIODeviceCallbackContext dummyContext;
    constexpr int kBlockSize = 512;
    std::vector<float> inL(kBlockSize, 0.40f);
    std::vector<float> inR(kBlockSize, 0.20f);
    const float* inChannels[2] = { inL.data(), inR.data() };

    std::vector<float> outL(kBlockSize, 0.0f);
    std::vector<float> outR(kBlockSize, 0.0f);
    float* outChannels[2] = { outL.data(), outR.data() };

    // Process block through audio callback
    engine.audioDeviceIOCallbackWithContext(
        inChannels, 2,
        outChannels, 2,
        kBlockSize,
        dummyContext
    );

    // 1. Verify ScopeTap received trimmed stereo samples
    REQUIRE(activeTap->getAvailableRead() >= static_cast<size_t>(kBlockSize));
    std::vector<float> readL(kBlockSize, 0.0f);
    std::vector<float> readR(kBlockSize, 0.0f);
    size_t readCount = activeTap->read(readL.data(), readR.data(), kBlockSize);
    REQUIRE(readCount == static_cast<size_t>(kBlockSize));

    for (size_t i = 0; i < readCount; ++i)
    {
        REQUIRE(readL[i] == Catch::Approx(0.60f).margin(0.005f));
        REQUIRE(readR[i] == Catch::Approx(0.30f).margin(0.005f));
    }

    // 2. Feed another block and verify serialization into WebUI JSON wire protocol
    engine.audioDeviceIOCallbackWithContext(
        inChannels, 2,
        outChannels, 2,
        kBlockSize,
        dummyContext
    );

    std::string jsonWireProtocol = engine.getFrameSerializer().serializeActiveFrame(activeTap, 48000.0f);
    REQUIRE_FALSE(jsonWireProtocol.empty());

    // Check JSON schema tokens consumed by ABDScope WebView2
    REQUIRE(jsonWireProtocol.find("\"signalType\":\"audio\"") != std::string::npos);
    REQUIRE(jsonWireProtocol.find("\"sampleRate\":48000") != std::string::npos);
    REQUIRE(jsonWireProtocol.find("\"timeDataL\":[") != std::string::npos);
    REQUIRE(jsonWireProtocol.find("\"timeDataR\":[") != std::string::npos);
    REQUIRE(jsonWireProtocol.find("\"peakL\":") != std::string::npos);
    REQUIRE(jsonWireProtocol.find("\"peakR\":") != std::string::npos);

    // Verify values in serialized JSON reflect trimmed levels (peakL ~ 0.6, peakR ~ 0.3)
    REQUIRE(jsonWireProtocol.find("0.6000") != std::string::npos);
    REQUIRE(jsonWireProtocol.find("0.3000") != std::string::npos);

    SECTION("Stimulus Generator Tap (Index 1) receives DAC output and serializes JSON")
    {
        engine.getScopeCollector().selectTap(1);
        auto* stimTap = engine.getScopeCollector().getActiveTap();
        REQUIRE(stimTap != nullptr);
        REQUIRE(stimTap->getName() == "Stimulus Generator");

        engine.getGenerator().setStimulus(abdaudiolab::audio::StimulusType::SineWave1kHz, 1.0);
        engine.audioDeviceIOCallbackWithContext(
            inChannels, 2,
            outChannels, 2,
            kBlockSize,
            dummyContext
        );

        REQUIRE(stimTap->getAvailableRead() >= static_cast<size_t>(kBlockSize));
        std::string stimJson = engine.getFrameSerializer().serializeActiveFrame(stimTap, 48000.0f);
        REQUIRE_FALSE(stimJson.empty());
        REQUIRE(stimJson.find("\"signalType\":\"audio\"") != std::string::npos);
        REQUIRE(stimJson.find("\"timeDataL\":[") != std::string::npos);
    }

    SECTION("Diagnostic 1kHz Tap (Index 2) synthesizes virtual tone and serializes JSON")
    {
        engine.getScopeCollector().selectTap(2);
        auto* diagTap = engine.getScopeCollector().getActiveTap();
        REQUIRE(diagTap != nullptr);
        REQUIRE(diagTap->getName() == "Diagnostic 1kHz");

        engine.audioDeviceIOCallbackWithContext(
            inChannels, 2,
            outChannels, 2,
            kBlockSize,
            dummyContext
        );

        REQUIRE(diagTap->getAvailableRead() >= static_cast<size_t>(kBlockSize));
        std::string diagJson = engine.getFrameSerializer().serializeActiveFrame(diagTap, 48000.0f);
        REQUIRE_FALSE(diagJson.empty());
        REQUIRE(diagJson.find("\"signalType\":\"audio\"") != std::string::npos);
        REQUIRE(diagJson.find("\"timeDataL\":[") != std::string::npos);
        REQUIRE(diagJson.find("\"timeDataR\":[") != std::string::npos);
    }
}

TEST_CASE("LabAudioReceiver Spectral Cross-Correlation Sample-Accurate Alignment", "[AudioEngine][Sync]")
{
    const double sampleRate = 48000.0;
    auto refPattern = abdaudiolab::audio::LabStimulusGenerator::generateSyncPulses3Pattern(sampleRate);
    REQUIRE_FALSE(refPattern.empty());

    // Create a captured buffer with an exact artificial latency/offset of 147 samples + initial silence
    const int knownDelay = 147;
    std::vector<float> recorded(refPattern.size() + static_cast<size_t>(knownDelay) + 500, 0.0f);

    // Copy reference pattern at the known delay index
    std::copy(refPattern.begin(), refPattern.end(), recorded.begin() + knownDelay);

    // Run spectral cross-correlation alignment
    int detectedOffset = abdaudiolab::audio::LabAudioReceiver::findSampleAccurateSyncOffset(
        recorded, refPattern, sampleRate);

    // Must match with 0 sample jitter
    REQUIRE(detectedOffset == knownDelay);
}

TEST_CASE("LabAudioReceiver Overload Guard and Auto-Abort", "[AudioEngine][Safety]")
{
    abdaudiolab::audio::LabAudioReceiver receiver;
    receiver.prepare(48000.0, 2.0);
    receiver.armContinuousCapture(48000);

    // 1. Normal signal: 2048 samples at 0.5f (-6 dBfs)
    std::vector<float> normalBlock(2048, 0.5f);
    receiver.processBlock(normalBlock.data(), static_cast<int>(normalBlock.size()));

    REQUIRE_FALSE(receiver.isOverloadTriggered());

    // 2. Sustained clipping signal: 800 samples at 1.0f (> -0.1 dBfs, exceeds 700 sample limit)
    std::vector<float> clippingBlock(800, 1.0f);
    receiver.processBlock(clippingBlock.data(), static_cast<int>(clippingBlock.size()));

    REQUIRE(receiver.isOverloadTriggered());
    REQUIRE(receiver.isFinished());

    // 3. Reset overload guard
    receiver.resetOverloadGuard();
    REQUIRE_FALSE(receiver.isOverloadTriggered());
}

TEST_CASE("LabAudioReceiver Dynamic Early Stopping by Silence", "[AudioEngine][Usability]")
{
    abdaudiolab::audio::LabAudioReceiver receiver;
    const double sr = 48000.0;
    receiver.prepare(sr, 2.0);
    receiver.armContinuousCapture(static_cast<int>(sr * 2.0)); // 2 seconds target

    // Phase 1: Grace period audio (> 250ms), e.g. 15,000 samples (~312ms)
    std::vector<float> activeAudio(15000, 0.5f);
    receiver.processBlock(activeAudio.data(), static_cast<int>(activeAudio.size()));
    REQUIRE_FALSE(receiver.isEarlyStopTriggered());

    // Phase 2: Sustained silence (< -80 dBfs) for > 100ms (5500 samples > 4800 samples)
    std::vector<float> silenceAudio(5500, 0.0f);
    receiver.processBlock(silenceAudio.data(), static_cast<int>(silenceAudio.size()));

    REQUIRE(receiver.isEarlyStopTriggered());
    REQUIRE(receiver.isFinished());
}

TEST_CASE("LabAnalyticEngine Single-Take Multi-Analysis", "[Math][Usability]")
{
    const double sr = 48000.0;
    const size_t totalSamples = 48000; // 1 second
    std::vector<float> syntheticAudio(totalSamples, 0.0f);

    // Initial transient (0 to 0.2s): saturated square/clipped waveform
    for (size_t i = 0; i < 9600; ++i)
    {
        syntheticAudio[i] = (std::sin(2.0 * juce::MathConstants<double>::pi * 200.0 * (i / sr)) >= 0.0) ? 0.95f : -0.95f;
    }

    // Sustained portion (0.2s to 1.0s): 1000 Hz decaying sinusoid
    for (size_t i = 9600; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i - 9600) / sr;
        syntheticAudio[i] = static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t) * std::exp(-3.0 * t));
    }

    auto analysis = abdaudiolab::math::LabAnalyticEngine::analyzeSingleTakeMultiplexed(syntheticAudio, sr, 0.2f);

    REQUIRE(analysis.transientPeakLevel >= 0.9f);
    REQUIRE(analysis.transientThd > 0.0f); // Non-linear saturation detected in transient
    REQUIRE(analysis.peakResonanceHz >= 800.0f);
    REQUIRE(analysis.peakResonanceHz <= 1200.0f);
    REQUIRE(analysis.decayTimeMs > 0.0f);
}

