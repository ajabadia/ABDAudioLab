/**
 * @file test_ScopeContractsAndLifetime.cpp
 * @brief Comprehensive unit and integration test suite for ABDScope:
 *        RT lock-free audio tap contracts, SPSC stress, JSON wire schema,
 *        TriggerDetector sub-bass hysteresis, and ownership/lifetime teardown.
 * @author ABDAudioLab & ABDSharedCode
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <Core/ScopeTap.h>
#include <Core/ScopeDataCollector.h>
#include <Core/ScopeFrameSerializer.h>
#include <Core/TriggerDetector.h>
#include <Core/SpscRingBuffer.h>

#include "../audio/LabAudioEngine.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

// ==============================================================================
// 1. RT SAFETY & SPSC RING BUFFER / SCOPETAP STRESS
// ==============================================================================

TEST_CASE("ScopeTap RT Contracts and Multi-Block Stress", "[Scope][RT][Contract]")
{
    // Compile-time checks for noexcept contract on the audio thread
    static_assert(noexcept(std::declval<abd::scope::ScopeTap&>().write(nullptr, 0)),
                  "ScopeTap::write must be noexcept for RT safety");
    static_assert(noexcept(std::declval<abd::scope::ScopeTap&>().writeStereo(nullptr, nullptr, 0)),
                  "ScopeTap::writeStereo must be noexcept for RT safety");
    static_assert(noexcept(std::declval<abd::scope::ScopeTap&>().read(nullptr, nullptr, 0)),
                  "ScopeTap::read must be noexcept");

    abd::scope::ScopeTap tap("Test Hardware", abd::scope::ScopeTapType::StereoAudio, 8192, "test_hw");

    SECTION("Inactive tap ignores writes and costs zero buffer allocation")
    {
        REQUIRE_FALSE(tap.isActive());
        REQUIRE(tap.getAvailableRead() == 0);

        std::vector<float> dummyL(512, 0.75f);
        std::vector<float> dummyR(512, -0.75f);

        tap.writeStereo(dummyL.data(), dummyR.data(), dummyL.size());
        REQUIRE(tap.getAvailableRead() == 0);

        std::vector<float> outL(512, 0.0f);
        std::vector<float> outR(512, 0.0f);
        size_t readCount = tap.read(outL.data(), outR.data(), 512);
        REQUIRE(readCount == 0);
    }

    SECTION("Activation enables capture and deactivation clears buffer")
    {
        tap.setActive(true);
        REQUIRE(tap.isActive());

        std::vector<float> dummyL(256, 0.42f);
        std::vector<float> dummyR(256, -0.42f);

        tap.writeStereo(dummyL.data(), dummyR.data(), dummyL.size());
        REQUIRE(tap.getAvailableRead() == 256);

        // Deactivating must reset buffers so old stale audio does not leak into next activation
        tap.setActive(false);
        REQUIRE_FALSE(tap.isActive());
        REQUIRE(tap.getAvailableRead() == 0);
    }

    SECTION("Multi-block stress with varying block sizes (64 to 8192)")
    {
        tap.setActive(true);
        const std::vector<size_t> blockSizes = { 64, 128, 256, 512, 1024, 2048, 4096 };

        for (size_t bSize : blockSizes)
        {
            std::vector<float> inL(bSize);
            std::vector<float> inR(bSize);
            for (size_t i = 0; i < bSize; ++i)
            {
                inL[i] = static_cast<float>(i) * 0.001f;
                inR[i] = -static_cast<float>(i) * 0.001f;
            }

            tap.writeStereo(inL.data(), inR.data(), bSize);
            REQUIRE(tap.getAvailableRead() == bSize);

            std::vector<float> readL(bSize, 0.0f);
            std::vector<float> readR(bSize, 0.0f);
            size_t nRead = tap.read(readL.data(), readR.data(), bSize);

            REQUIRE(nRead == bSize);
            REQUIRE(tap.getAvailableRead() == 0);

            // Verify sample fidelity without corruption
            for (size_t i = 0; i < bSize; ++i)
            {
                REQUIRE(readL[i] == Catch::Approx(inL[i]));
                REQUIRE(readR[i] == Catch::Approx(inR[i]));
            }
        }
    }
}

// ==============================================================================
// 2. SCOPEDATACOLLECTOR TAP MANAGEMENT & WIRE SLUG RESOLUTION
// ==============================================================================

TEST_CASE("ScopeDataCollector Tap Management and Wire Slug Resolution", "[Scope][Collector][Contract]")
{
    abd::scope::ScopeDataCollector collector;

    auto* tap1 = collector.registerTap("Hardware In (DUT)", abd::scope::ScopeTapType::StereoAudio, 4096, "hardware_in");
    auto* tap2 = collector.registerTap("Stimulus Generator", abd::scope::ScopeTapType::StereoAudio, 4096, "stimulus");
    auto* tap3 = collector.registerTap("LFO Modulation Probe", abd::scope::ScopeTapType::ControlSignal, 2048, "lfo_probe");

    REQUIRE(collector.getTapCount() == 3);
    REQUIRE(tap1 != nullptr);
    REQUIRE(tap2 != nullptr);
    REQUIRE(tap3 != nullptr);

    // Auto-select first tap rule
    REQUIRE(tap1->isActive());
    REQUIRE_FALSE(tap2->isActive());
    REQUIRE_FALSE(tap3->isActive());
    REQUIRE(collector.getActiveTap() == tap1);

    SECTION("Select by index activates only targeted tap")
    {
        collector.selectTap(1);
        REQUIRE_FALSE(tap1->isActive());
        REQUIRE(tap2->isActive());
        REQUIRE_FALSE(tap3->isActive());
        REQUIRE(collector.getActiveTap() == tap2);
    }

    SECTION("Select by wire ID slug and lenient queries")
    {
        // 1. Explicit ID
        REQUIRE(collector.selectTap("stimulus"));
        REQUIRE(collector.getActiveTap() == tap2);

        // 2. Case-insensitive slug query & exact ID
        REQUIRE(collector.selectTap("lfo_probe"));
        REQUIRE(collector.getActiveTap() == tap3);
        REQUIRE(collector.selectTap("LFO_PROBE"));
        REQUIRE(collector.getActiveTap() == tap3);

        // 3. Substring fallback
        REQUIRE(collector.selectTap("Hardware"));
        REQUIRE(collector.getActiveTap() == tap1);

        // 4. Unknown query returns false without mutating state
        REQUIRE_FALSE(collector.selectTap("NonExistentTap"));
        REQUIRE(collector.getActiveTap() == tap1);
    }

    SECTION("Non-const and const getTap overloads")
    {
        const auto& constCollector = collector;
        const abd::scope::ScopeTap* constPtr = constCollector.getTap(0);
        REQUIRE(constPtr == tap1);

        // Non-const overload allows calling non-const methods without const_cast
        abd::scope::ScopeTap* nonConstPtr = collector.getTap(0);
        REQUIRE(nonConstPtr == tap1);
        nonConstPtr->setActive(true);
        REQUIRE(nonConstPtr->isActive());
    }

    SECTION("Deactivate all clears active flag across all taps")
    {
        collector.deactivateAll();
        REQUIRE_FALSE(tap1->isActive());
        REQUIRE_FALSE(tap2->isActive());
        REQUIRE_FALSE(tap3->isActive());
    }
}

// ==============================================================================
// 3. SCOPEFRAMESERIALIZER WIRE PROTOCOL & CANONICAL JSON SCHEMA
// ==============================================================================

TEST_CASE("ScopeFrameSerializer Wire Protocol and Canonical JSON Schema", "[Scope][Serializer][JSON]")
{
    abd::scope::ScopeDataCollector collector;
    auto* tapStereo = collector.registerTap("Stereo Master", abd::scope::ScopeTapType::StereoAudio, 4096, "master_out");
    auto* tapControl = collector.registerTap("Modulation Env", abd::scope::ScopeTapType::ControlSignal, 4096, "mod_env");

    abd::scope::ScopeFrameSerializer serializer(512);

    SECTION("Returns empty string when buffer has insufficient samples")
    {
        tapStereo->setActive(true);
        std::vector<float> smallBuf(256, 0.5f);
        tapStereo->writeStereo(smallBuf.data(), smallBuf.data(), smallBuf.size());

        std::string jsonStr = serializer.serializeActiveFrame(tapStereo, 48000.0f);
        REQUIRE(jsonStr.empty());
    }

    SECTION("Serializes full stereo frame into valid schema-compliant JSON")
    {
        tapStereo->setActive(true);
        std::vector<float> inL(512);
        std::vector<float> inR(512);
        for (size_t i = 0; i < 512; ++i)
        {
            inL[i] = 0.50f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 * (static_cast<double>(i) / 48000.0)));
            inR[i] = 0.25f * static_cast<float>(std::cos(2.0 * std::numbers::pi * 1000.0 * (static_cast<double>(i) / 48000.0)));
        }
        tapStereo->writeStereo(inL.data(), inR.data(), 512);

        std::string jsonStr = serializer.serializeActiveFrame(tapStereo, 48000.0f);
        REQUIRE_FALSE(jsonStr.empty());

        // Validate JSON parsing and canonical fields
        nlohmann::json parsed;
        REQUIRE_NOTHROW(parsed = nlohmann::json::parse(jsonStr));

        REQUIRE(parsed.contains("signalType"));
        REQUIRE(parsed["signalType"] == "audio");

        REQUIRE(parsed.contains("tapId"));
        REQUIRE(parsed["tapId"] == "master_out");

        REQUIRE(parsed.contains("sampleRate"));
        REQUIRE(parsed["sampleRate"] == 48000);

        REQUIRE(parsed.contains("numSamples"));
        REQUIRE(parsed["numSamples"] == 512);

        REQUIRE(parsed.contains("peakL"));
        REQUIRE(parsed["peakL"].get<float>() == Catch::Approx(0.50f).margin(0.01f));

        REQUIRE(parsed.contains("peakR"));
        REQUIRE(parsed["peakR"].get<float>() == Catch::Approx(0.25f).margin(0.01f));

        REQUIRE(parsed.contains("timeDataL"));
        REQUIRE(parsed["timeDataL"].is_array());
        REQUIRE(parsed["timeDataL"].size() == 512);

        REQUIRE(parsed.contains("timeDataR"));
        REQUIRE(parsed["timeDataR"].is_array());
        REQUIRE(parsed["timeDataR"].size() == 512);
    }

    SECTION("Control signal tap omits timeDataR")
    {
        tapControl->setActive(true);
        std::vector<float> ctrl(512, 0.8f);
        tapControl->write(ctrl.data(), 512);

        std::string jsonStr = serializer.serializeActiveFrame(tapControl, 48000.0f);
        REQUIRE_FALSE(jsonStr.empty());

        nlohmann::json parsed = nlohmann::json::parse(jsonStr);
        REQUIRE(parsed["signalType"] == "control");
        REQUIRE(parsed["tapId"] == "mod_env");
        REQUIRE(parsed.contains("timeDataL"));
        REQUIRE_FALSE(parsed.contains("timeDataR"));
    }

    SECTION("Handles extreme values (NaN and Inf) without crashing")
    {
        tapStereo->setActive(true);
        std::vector<float> nanBuf(512, 0.0f);
        nanBuf[10] = std::numeric_limits<float>::quiet_NaN();
        nanBuf[20] = std::numeric_limits<float>::infinity();

        tapStereo->writeStereo(nanBuf.data(), nanBuf.data(), 512);
        std::string jsonStr;
        REQUIRE_NOTHROW(jsonStr = serializer.serializeActiveFrame(tapStereo, 48000.0f));
        REQUIRE_FALSE(jsonStr.empty());
    }
}

// ==============================================================================
// 4. TRIGGERDETECTOR SUB-BASS TRACKING (20-140 HZ) & HYSTERESIS RESILIENCE
// ==============================================================================

TEST_CASE("TriggerDetector Sub-Bass Tracking and Hysteresis Resilience", "[Scope][Trigger][DSP]")
{
    constexpr float kSampleRate = 48000.0f;

    SECTION("Silence below 0.005f peak returns default empty result")
    {
        std::vector<float> silence(4096, 0.002f);
        auto result = abd::scope::TriggerDetector::process(silence.data(), silence.size(), kSampleRate);
        REQUIRE(result.triggerIndex == 0);
        REQUIRE(result.estimatedFrequencyHz == 0.0f);
        REQUIRE(result.noteName.empty());
    }

    SECTION("Sub-threshold fluctuation around zero does not rearm or oscillate")
    {
        // Fixed hysteresis = 0.05f. Signal amplitude is 0.02f, so it oscillates in [-0.02, +0.02].
        // It never dips below -0.05f, hence cannot arm the trigger.
        constexpr float kHysteresis = 0.05f;
        std::vector<float> ripple(4096);
        for (size_t i = 0; i < ripple.size(); ++i)
        {
            ripple[i] = 0.02f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 100.0 * (static_cast<double>(i) / kSampleRate)));
        }
        auto resultUnarmed = abd::scope::TriggerDetector::process(ripple.data(), ripple.size(), kSampleRate, kHysteresis);
        REQUIRE(resultUnarmed.triggerIndex == 0);

        // When amplitude increases to 0.10f, it dips to -0.10f < -0.05f (arms), then crosses zero rising -> triggers!
        std::vector<float> armedSignal(4096);
        for (size_t i = 0; i < armedSignal.size(); ++i)
        {
            armedSignal[i] = 0.10f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 100.0 * (static_cast<double>(i) / kSampleRate)));
        }
        auto resultArmed = abd::scope::TriggerDetector::process(armedSignal.data(), armedSignal.size(), kSampleRate, kHysteresis);
        REQUIRE(resultArmed.triggerIndex > 0);
    }

    SECTION("Sub-bass 55 Hz (A1) detection within 2 Hz tolerance")
    {
        constexpr double targetFreq = 55.0; // Note A1
        std::vector<float> subBass(4096);
        for (size_t i = 0; i < subBass.size(); ++i)
        {
            subBass[i] = 0.85f * static_cast<float>(std::sin(2.0 * std::numbers::pi * targetFreq * (static_cast<double>(i) / kSampleRate)));
        }

        auto result = abd::scope::TriggerDetector::process(subBass.data(), subBass.size(), kSampleRate);

        REQUIRE(result.triggerIndex > 0);
        REQUIRE(result.estimatedFrequencyHz == Catch::Approx(55.0f).margin(2.0f));
        REQUIRE(result.noteName == "A1");
    }

    SECTION("Sub-bass 110 Hz (A2) detection")
    {
        constexpr double targetFreq = 110.0; // Note A2
        std::vector<float> subBass(4096);
        for (size_t i = 0; i < subBass.size(); ++i)
        {
            subBass[i] = 0.70f * static_cast<float>(std::sin(2.0 * std::numbers::pi * targetFreq * (static_cast<double>(i) / kSampleRate)));
        }

        auto result = abd::scope::TriggerDetector::process(subBass.data(), subBass.size(), kSampleRate);

        REQUIRE(result.triggerIndex > 0);
        REQUIRE(result.estimatedFrequencyHz == Catch::Approx(110.0f).margin(2.0f));
        REQUIRE(result.noteName == "A2");
    }

    SECTION("Standard 440 Hz (A4) detection")
    {
        constexpr double targetFreq = 440.0;
        std::vector<float> tone(4096);
        for (size_t i = 0; i < tone.size(); ++i)
        {
            tone[i] = 0.60f * static_cast<float>(std::sin(2.0 * std::numbers::pi * targetFreq * (static_cast<double>(i) / kSampleRate)));
        }

        auto result = abd::scope::TriggerDetector::process(tone.data(), tone.size(), kSampleRate);

        REQUIRE(result.triggerIndex > 0);
        REQUIRE(result.estimatedFrequencyHz == Catch::Approx(440.0f).margin(3.0f));
        REQUIRE(result.noteName == "A4");
    }

    SECTION("frequencyToNoteName bounds checking")
    {
        REQUIRE(abd::scope::TriggerDetector::frequencyToNoteName(10.0f).empty());
        REQUIRE(abd::scope::TriggerDetector::frequencyToNoteName(25000.0f).empty());
        REQUIRE(abd::scope::TriggerDetector::frequencyToNoteName(261.63f) == "C4");
        REQUIRE(abd::scope::TriggerDetector::frequencyToNoteName(440.0f) == "A4");
    }
}

// ==============================================================================
// 5. LIFETIME, CONCURRENCY & TEARDOWN CONTRACTS
// ==============================================================================

TEST_CASE("Scope Lifetime, Concurrency and Teardown Contracts", "[Scope][Lifetime][Concurrency]")
{
    SECTION("Concurrent audio thread writes while main thread deactivates taps")
    {
        abd::scope::ScopeDataCollector collector;
        auto* tap = collector.registerTap("ConcurrentTap", abd::scope::ScopeTapType::StereoAudio, 8192, "concurrent");
        tap->setActive(true);

        std::atomic<bool> audioRunning{true};
        std::atomic<bool> audioStarted{false};
        std::atomic<size_t> totalWrites{0};

        // Simulate real-time audio callback thread pushing samples continuously
        std::thread audioThread([&] {
            std::vector<float> block(256, 0.33f);
            audioStarted.store(true, std::memory_order_release);
            while (audioRunning.load(std::memory_order_relaxed))
            {
                tap->writeStereo(block.data(), block.data(), block.size());
                totalWrites.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });

        // Wait until audio thread has definitely started
        while (!audioStarted.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Main/UI thread rapidly toggles active state and calls deactivateAll
        for (int i = 0; i < 30; ++i)
        {
            tap->setActive(false);
            collector.deactivateAll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            tap->setActive(true);
        }

        audioRunning.store(false, std::memory_order_release);
        audioThread.join();

        REQUIRE(totalWrites.load(std::memory_order_relaxed) > 0);
        collector.deactivateAll();
        REQUIRE_FALSE(tap->isActive());
    }

    SECTION("LabAudioEngine owns collector and performs clean shutdown")
    {
        {
            abdaudiolab::audio::LabAudioEngine engine;

            // Verify three canonical taps are registered
            auto& coll = engine.getScopeCollector();
            REQUIRE(coll.getTapCount() == 3);

            REQUIRE(coll.findTapIndex("hardware_in") != abd::scope::ScopeDataCollector::npos);
            REQUIRE(coll.findTapIndex("stimulus") != abd::scope::ScopeDataCollector::npos);
            REQUIRE(coll.findTapIndex("diag_tone") != abd::scope::ScopeDataCollector::npos);

            // Shutdown sequence: deactivate taps before destruction
            coll.deactivateAll();
            for (size_t i = 0; i < coll.getTapCount(); ++i)
            {
                REQUIRE_FALSE(coll.getTap(i)->isActive());
            }
        }
        // LabAudioEngine destroyed cleanly without dangling callbacks
        SUCCEED("LabAudioEngine destroyed safely with Scope taps deactivated");
    }
}
