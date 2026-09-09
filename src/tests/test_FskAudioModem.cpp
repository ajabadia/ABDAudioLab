#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "hardware/FskAudioModem.h"
#include "hardware/AiraSysExController.h"
#include "hardware/MidiIdentityDetector.h"
#include <numbers>

using namespace abdaudiolab::hardware;

TEST_CASE("FskAudioModem - Goertzel Spectral Power Discriminator", "[fsk][goertzel][dsp]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int numSamples = 240; // 5ms window
    std::vector<float> sine12k(numSamples);
    std::vector<float> sine14k(numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        sine12k[i] = static_cast<float>(std::sin(2.0 * std::numbers::pi_v<double> * 12000.0 * t));
        sine14k[i] = static_cast<float>(std::sin(2.0 * std::numbers::pi_v<double> * 14000.0 * t));
    }

    // 12 kHz tone test
    float p12k_at_12k = FskAudioModem::computeGoertzelPower(sine12k.data(), numSamples, sampleRate, 12000.0f);
    float p12k_at_14k = FskAudioModem::computeGoertzelPower(sine12k.data(), numSamples, sampleRate, 14000.0f);

    CHECK(p12k_at_12k > 0.05f);
    CHECK(p12k_at_14k < 0.02f);
    CHECK(p12k_at_12k > p12k_at_14k * 5.0f);

    // 14 kHz tone test
    float p14k_at_14k = FskAudioModem::computeGoertzelPower(sine14k.data(), numSamples, sampleRate, 14000.0f);
    float p14k_at_12k = FskAudioModem::computeGoertzelPower(sine14k.data(), numSamples, sampleRate, 12000.0f);

    CHECK(p14k_at_14k > 0.05f);
    CHECK(p14k_at_12k < 0.02f);
    CHECK(p14k_at_14k > p14k_at_12k * 5.0f);
}

TEST_CASE("FskAudioModem - Carrier Detection and Noise Rejection", "[fsk][carrier][airamodular]")
{
    FskAudioModem modem;
    constexpr double sampleRate = 48000.0;

    // 1. Modulate a test payload to generate real AIRA FSK audio
    std::vector<uint8_t> payload = { 0x17, 0x01, 0x02, 0xFF };
    auto fskBuffer = modem.modulate(payload, sampleRate);
    REQUIRE(fskBuffer.getNumSamples() > 0);

    auto det = modem.detectCarrier(fskBuffer, sampleRate, 8.0f);
    CHECK(det.detected == true);
    CHECK(det.snrDb > 8.0f);

    // 2. Pure silence should NOT be detected
    juce::AudioBuffer<float> silentBuffer(1, 4800);
    silentBuffer.clear();
    auto detSilence = modem.detectCarrier(silentBuffer, sampleRate, 8.0f);
    CHECK(detSilence.detected == false);

    // 3. Out-of-band 1 kHz audio tone should NOT trigger false positive
    juce::AudioBuffer<float> toneBuffer(1, 4800);
    auto* ptr = toneBuffer.getWritePointer(0);
    for (int i = 0; i < 4800; ++i)
    {
        ptr[i] = static_cast<float>(std::sin(2.0 * std::numbers::pi_v<double> * 1000.0 * (i / sampleRate)));
    }
    auto detTone = modem.detectCarrier(toneBuffer, sampleRate, 8.0f);
    CHECK(detTone.detected == false);
}

TEST_CASE("FskAudioModem - Continuous Phase Modulation & Demodulation Roundtrip", "[fsk][roundtrip]")
{
    FskModemConfig cfg;
    cfg.baudRate = 1200.0f;
    cfg.markFrequencyHz = 12000.0f;
    cfg.spaceFrequencyHz = 14000.0f;
    cfg.preambleBits = 16;
    FskAudioModem modem(cfg);

    constexpr double sampleRate = 48000.0;
    const std::vector<uint8_t> originalData = { 0x17, 0x42, 0xA5, 0x5A, 0xFF };

    auto modulated = modem.modulate(originalData, sampleRate);
    REQUIRE(modulated.getNumSamples() > 0);

    auto recovered = modem.demodulate(modulated, sampleRate);
    REQUIRE(recovered.size() == originalData.size());
    for (size_t i = 0; i < originalData.size(); ++i)
    {
        CHECK(recovered[i] == originalData[i]);
    }
}

TEST_CASE("AiraSysExController - Audio In FSK Export and Detection", "[fsk][airasysex]")
{
    AiraSysExController aira(AiraModel::Torcido);
    constexpr double sampleRate = 48000.0;

    auto fskAudio = aira.exportPatchAsFskAudio(sampleRate);
    REQUIRE(fskAudio.getNumSamples() > 0);

    // Controller should confirm FSK carrier signature in the audio
    CHECK(aira.detectFskAudioResponse(fskAudio, sampleRate) == true);
}

TEST_CASE("Hardware Detection - Pre-Warming Asynchronous Execution", "[hwid][prewarm]")
{
    MidiIdentityDetector detector;
    CHECK(detector.isPreWarmed() == false);

    bool callbackInvoked = false;
    detector.preWarmAsync([&callbackInvoked](const std::vector<DiscoveredDevice>&) {
        callbackInvoked = true;
    });

    // Allow background thread to start and run scan
    int waitLoops = 0;
    while (!detector.isPreWarmed() && waitLoops < 120)
    {
        juce::Thread::sleep(50);
        waitLoops++;
    }

    CHECK(detector.isPreWarmed() == true);
}
