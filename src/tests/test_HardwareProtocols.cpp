#include <catch2/catch_test_macros.hpp>
#include "../hardware/SysExCodec.h"
#include "../hardware/NRPNParser.h"
#include <vector>
#include <random>

using namespace abdaudiolab::hardware;

TEST_CASE("SysExCodec - 7-to-8 and 8-to-7 Bit Universal Packing", "[midi][sysex][codec]")
{
    SECTION("Empty input safety")
    {
        std::vector<uint8_t> out;
        CHECK_FALSE(SysExCodec::pack8to7(nullptr, 0, out));
        CHECK_FALSE(SysExCodec::unpack7to8(nullptr, 0, out));
    }

    SECTION("Known byte roundtrip with MSB set")
    {
        // 7 raw bytes with mixed MSBs (some > 127, some < 128)
        std::vector<uint8_t> raw7 = { 0x80, 0x7F, 0xFF, 0x00, 0xAA, 0x55, 0xC3 };
        std::vector<uint8_t> packed;

        REQUIRE(SysExCodec::pack8to7(raw7.data(), raw7.size(), packed));
        // 7 bytes should produce 8 encoded bytes (1 MSB collector + 7 data)
        REQUIRE(packed.size() == 8);

        // Crucial MIDI invariant: every encoded byte must be <= 0x7F (MSB bit 7 clear)
        for (size_t i = 0; i < packed.size(); ++i)
        {
            CHECK((packed[i] & 0x80) == 0);
        }

        // Unpack and verify bit-for-bit identity
        std::vector<uint8_t> unpacked;
        REQUIRE(SysExCodec::unpack7to8(packed.data(), packed.size(), unpacked));
        REQUIRE(unpacked.size() == raw7.size());
        CHECK(unpacked == raw7);
    }

    SECTION("Arbitrary lengths and pseudo-random stress roundtrip")
    {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> byteDist(0, 255);

        for (size_t len : { 1, 2, 6, 7, 8, 13, 14, 15, 64, 128, 288 })
        {
            std::vector<uint8_t> original(len);
            for (auto& b : original)
                b = static_cast<uint8_t>(byteDist(rng));

            std::vector<uint8_t> packed;
            REQUIRE(SysExCodec::pack8to7(original.data(), original.size(), packed));

            // All packed bytes must be <= 0x7F
            for (auto b : packed)
                REQUIRE((b & 0x80) == 0);

            std::vector<uint8_t> unpacked;
            REQUIRE(SysExCodec::unpack7to8(packed.data(), packed.size(), unpacked));
            REQUIRE(unpacked == original);
        }
    }
}

TEST_CASE("NRPNParser - 14-Bit State Machine and Telemetry", "[midi][nrpn][telemetry]")
{
    NRPNParser parser;
    NRPNMessage msg;

    SECTION("Partial sequence does not trigger complete event")
    {
        // Only CC 99 sent
        CHECK_FALSE(parser.processCC(1, 99, 0x02, msg));
        // CC 98 sent
        CHECK_FALSE(parser.processCC(1, 98, 0x1A, msg));
        // Only with CC 6 or CC 38 does it complete
    }

    SECTION("Full 14-bit NRPN message assembly")
    {
        // NRPN Parameter: (0x02 << 7) | 0x1A = 282
        // Value: (0x40 << 7) | 0x20 = 8192 + 32 = 8224
        CHECK_FALSE(parser.processCC(2, 99, 0x02, msg));
        CHECK_FALSE(parser.processCC(2, 98, 0x1A, msg));
        CHECK(parser.processCC(2, 6, 0x40, msg)); // Completed 7-bit/initial MSB

        CHECK(msg.channel == 2);
        CHECK(msg.nrpnMSB == 0x02);
        CHECK(msg.nrpnLSB == 0x1A);
        CHECK(msg.dataMSB == 0x40);

        // Now send LSB (CC 38)
        CHECK(parser.processCC(2, 38, 0x20, msg));
        CHECK(msg.dataLSB == 0x20);
        CHECK(msg.getValue14Bit() == 8224);
    }

    SECTION("Channel change resets partial state")
    {
        CHECK_FALSE(parser.processCC(1, 99, 0x05, msg));
        CHECK_FALSE(parser.processCC(1, 98, 0x10, msg));

        // Switch to channel 3 mid-stream
        CHECK_FALSE(parser.processCC(3, 6, 0x50, msg)); // Should NOT trigger because 99/98 were on ch 1
    }

    SECTION("appendNRPNToBuffer generation")
    {
        juce::MidiBuffer buffer;
        // Generate NRPN 0x01, 0x0A with value 10000 (14-bit)
        NRPNParser::appendNRPNToBuffer(buffer, 1, 0x01, 0x0A, 10000, true);

        // Expect 4 events: CC 99, CC 98, CC 6, CC 38
        int eventCount = 0;
        for (const auto meta : buffer)
        {
            auto m = meta.getMessage();
            REQUIRE(m.isController());
            eventCount++;
        }
        CHECK(eventCount == 4);
    }
}

#include "../hardware/CasioNibbleCodec.h"
#include "../hardware/JunoTapeModem.h"

TEST_CASE("CasioNibbleCodec - 4-Bit Packing, Unpacking and Checksum", "[casio][sysex][nibble]")
{
    SECTION("Empty input safety")
    {
        std::vector<uint8_t> out;
        CHECK(CasioNibbleCodec::encode(nullptr, 0, out));
        CHECK(out.empty());
        CHECK(CasioNibbleCodec::decode(nullptr, 0, out));
        CHECK(out.empty());

        // Odd length must fail decode
        std::vector<uint8_t> odd = { 0x01, 0x02, 0x03 };
        CHECK_FALSE(CasioNibbleCodec::decode(odd.data(), odd.size(), out));
    }

    SECTION("HighFirst and LowFirst encoding accuracy")
    {
        std::vector<uint8_t> raw = { 0xA5, 0x3F, 0x00, 0xC8 };
        std::vector<uint8_t> highNibbles;
        std::vector<uint8_t> lowNibbles;

        REQUIRE(CasioNibbleCodec::encode(raw, highNibbles, NibbleOrder::HighFirst));
        REQUIRE(CasioNibbleCodec::encode(raw, lowNibbles, NibbleOrder::LowFirst));

        // HighFirst: 0xA5 -> 0x0A, 0x05
        REQUIRE(highNibbles.size() == 8);
        CHECK(highNibbles[0] == 0x0A);
        CHECK(highNibbles[1] == 0x05);
        CHECK(highNibbles[2] == 0x03);
        CHECK(highNibbles[3] == 0x0F);

        // LowFirst: 0xA5 -> 0x05, 0x0A
        REQUIRE(lowNibbles.size() == 8);
        CHECK(lowNibbles[0] == 0x05);
        CHECK(lowNibbles[1] == 0x0A);
        CHECK(lowNibbles[2] == 0x0F);
        CHECK(lowNibbles[3] == 0x03);

        // Decode roundtrip
        std::vector<uint8_t> decodedHigh;
        std::vector<uint8_t> decodedLow;
        REQUIRE(CasioNibbleCodec::decode(highNibbles, decodedHigh, NibbleOrder::HighFirst));
        REQUIRE(CasioNibbleCodec::decode(lowNibbles, decodedLow, NibbleOrder::LowFirst));

        CHECK(decodedHigh == raw);
        CHECK(decodedLow == raw);
    }

    SECTION("Casio 7-bit checksum verification")
    {
        std::vector<uint8_t> data = { 0x12, 0x34, 0x56, 0x78 };
        // Sum = 0x12 + 0x34 + 0x56 + 0x78 = 18 + 52 + 86 + 120 = 276
        // 276 & 0x7F = 276 & 127 = 20
        uint8_t expectedSum = static_cast<uint8_t>((0x12 + 0x34 + 0x56 + 0x78) & 0x7F);
        CHECK(CasioNibbleCodec::calculateChecksum(data.data(), data.size()) == expectedSum);
        CHECK(CasioNibbleCodec::verifyChecksum(data.data(), data.size(), expectedSum));
        CHECK_FALSE(CasioNibbleCodec::verifyChecksum(data.data(), data.size(), expectedSum + 1));
    }
}

TEST_CASE("JunoTapeModem - CPFSK 1.3/2.6 kHz Audio Modulation and Demodulation", "[juno][fsk][tape]")
{
    JunoTapeConfig config;
    config.pilotDurationSec = 0.05f; // Short pilot for fast test execution
    config.baudRate = 1300.0f;
    config.spaceFrequencyHz = 1300.0f;
    config.markFrequencyHz = 2600.0f;
    config.amplitude = 0.8f;

    JunoTapeModem modem(config);

    SECTION("Checksum calculation matches Roland tape standard")
    {
        std::vector<uint8_t> testData = { 0x10, 0x20, 0x30 };
        // Sum = 0x60 = 96. (-96) & 0x7F = 32
        uint8_t chk = JunoTapeModem::calculateChecksum(testData.data(), testData.size());
        int32_t sum = 0x10 + 0x20 + 0x30;
        uint8_t expected = static_cast<uint8_t>((-sum) & 0x7F);
        CHECK(chk == expected);
    }

    SECTION("Audio modulation and carrier tone detection")
    {
        std::vector<uint8_t> payload = { 0x01, 0x02, 0x03, 0x04 };
        const double sampleRate = 48000.0;
        auto audioBuffer = modem.encodeToAudio(payload, sampleRate);

        REQUIRE(audioBuffer.getNumChannels() == 1);
        REQUIRE(audioBuffer.getNumSamples() > 1000);

        // Pilot detection
        auto detection = modem.detectCarrier(audioBuffer, sampleRate, 6.0f);
        CHECK(detection.detected);
        CHECK(detection.pilotPowerDb > -30.0f);
        CHECK(detection.snrDb > 6.0f);
    }

    SECTION("Full Encode -> Audio Buffer -> Decode Roundtrip (48 kHz)")
    {
        // 18-byte standard Roland Juno-106 single patch memory payload
        std::vector<uint8_t> junoPatch = {
            0x24, 0x1A, 0x7F, 0x40, 0x55, 0x12, 0x00, 0x70, 0x38,
            0x19, 0x6E, 0x5A, 0x4F, 0x32, 0x11, 0x08, 0x04, 0x7F
        };

        const double sampleRate = 48000.0;
        auto audioBuffer = modem.encodeToAudio(junoPatch, sampleRate);
        REQUIRE(audioBuffer.getNumSamples() > 0);

        auto decoded = modem.decodeFromAudio(audioBuffer, sampleRate);
        REQUIRE(decoded.size() == junoPatch.size());
        CHECK(decoded == junoPatch);
    }

    SECTION("Encode -> Decode Roundtrip at 44.1 kHz")
    {
        std::vector<uint8_t> smallPayload = { 0xCA, 0xFE, 0xBA, 0xBE, 0x42 };
        const double sampleRate = 44100.0;
        auto audioBuffer = modem.encodeToAudio(smallPayload, sampleRate);

        auto decoded = modem.decodeFromAudio(audioBuffer, sampleRate);
        REQUIRE(decoded.size() == smallPayload.size());
        CHECK(decoded == smallPayload);
    }
}
