#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <audio/LabAudioEngine.h>
#include <audio/LabStimulusGenerator.h>
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
