#include <catch2/catch_test_macros.hpp>
#include "hardware/transport/MidiTransportTypes.h"
#include "hardware/transport/IMidiTransport.h"
#include "tests/support/MockMidiTransport.h"

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::tests;

TEST_CASE("MidiTransport: Initial state is closed and empty", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    CHECK_FALSE(transport.isOpen());
    CHECK(transport.sentCc().empty());
    CHECK(transport.sentSysEx().empty());
    CHECK(transport.getActivePortSelection().stableDeviceId.empty());
}

TEST_CASE("MidiTransport: Open with valid selection succeeds", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "port_uid_123", "USB MIDI Out 1", "behringer_pro800" };

    auto res = transport.open(sel);
    REQUIRE(res.opened);
    REQUIRE(res.error == MidiTransportError::None);
    CHECK(transport.isOpen());
    CHECK(transport.getActivePortSelection() == sel);
}

TEST_CASE("MidiTransport: Open with empty device ID fails", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "", "Unnamed Port", "yamaha_dx7" };

    auto res = transport.open(sel);
    REQUIRE_FALSE(res.opened);
    CHECK(res.error == MidiTransportError::InvalidPortSelection);
    CHECK(res.diagnosticCode == "ERR_MIDI_INVALID_SELECTION");
    CHECK_FALSE(transport.isOpen());
}

TEST_CASE("MidiTransport: Injected open failure returns OpenFailed and leaves closed", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    transport.failNextOpen(MidiTransportError::OpenFailed, "ERR_MIDI_OUTPUT_OPEN_FAILED");

    MidiPortSelection sel{ "port_uid_123", "USB MIDI Out 1", "behringer_pro800" };
    auto res = transport.open(sel);
    REQUIRE_FALSE(res.opened);
    CHECK(res.error == MidiTransportError::OpenFailed);
    CHECK(res.diagnosticCode == "ERR_MIDI_OUTPUT_OPEN_FAILED");
    CHECK_FALSE(transport.isOpen());

    // El error inyectado es de un solo disparo: el siguiente open debe tener éxito
    auto retryRes = transport.open(sel);
    REQUIRE(retryRes.opened);
    CHECK(transport.isOpen());
}

TEST_CASE("MidiTransport: Send without opening returns NotOpen", "[hardware][transport][d2]")
{
    MockMidiTransport transport;

    MidiCcMessage ccMsg{ 1, 19, 64, "behringer_pro800", "filter_cutoff", 0 };
    auto ccRes = transport.sendCc(ccMsg);
    REQUIRE_FALSE(ccRes.acceptedForTransmission);
    CHECK(ccRes.error == MidiTransportError::NotOpen);
    CHECK(ccRes.diagnosticCode == "ERR_MIDI_NOT_OPEN");
    CHECK(transport.sentCc().empty());

    MidiSysExMessage syxMsg{ { 0xF0, 0x43, 0x10, 0x09, 0x10, 0x40, 0xF7 }, "yamaha_dx7", "op1_level", "yamaha-dx7", 0 };
    auto syxRes = transport.sendSysEx(syxMsg);
    REQUIRE_FALSE(syxRes.acceptedForTransmission);
    CHECK(syxRes.error == MidiTransportError::NotOpen);
    CHECK(transport.sentSysEx().empty());
}

TEST_CASE("MidiTransport: Send valid CC records message, sequence number and preserves order", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "port_uid_123", "USB MIDI Out 1", "behringer_pro800" };
    REQUIRE(transport.open(sel).opened);

    transport.setMockTimestampSamples(1024);

    MidiCcMessage msg1{ 1, 19, 32, "behringer_pro800", "filter_cutoff", 0 };
    MidiCcMessage msg2{ 1, 21, 64, "behringer_pro800", "filter_resonance", 0 };

    auto res1 = transport.sendCc(msg1);
    REQUIRE(res1.acceptedForTransmission);
    CHECK(res1.sequenceNumber == 1);
    CHECK(res1.logicalTimestampSamples == 1024);

    transport.setMockTimestampSamples(2048);
    auto res2 = transport.sendCc(msg2);
    REQUIRE(res2.acceptedForTransmission);
    CHECK(res2.sequenceNumber == 2);
    CHECK(res2.logicalTimestampSamples == 2048);

    REQUIRE(transport.sentCc().size() == 2);
    CHECK(transport.sentCc()[0].controllerNumber == 19);
    CHECK(transport.sentCc()[0].value == 32);
    CHECK(transport.sentCc()[0].sequenceNumber == 1);

    CHECK(transport.sentCc()[1].controllerNumber == 21);
    CHECK(transport.sentCc()[1].value == 64);
    CHECK(transport.sentCc()[1].sequenceNumber == 2);
}

TEST_CASE("MidiTransport: Send invalid CC returns InvalidMessage", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "port_uid_123", "USB MIDI Out 1", "behringer_pro800" };
    REQUIRE(transport.open(sel).opened);

    // Canal 0 ilegal
    MidiCcMessage badChannel{ 0, 19, 64, "behringer_pro800", "cutoff", 0 };
    auto res1 = transport.sendCc(badChannel);
    REQUIRE_FALSE(res1.acceptedForTransmission);
    CHECK(res1.error == MidiTransportError::InvalidMessage);

    // Controller 128 ilegal
    MidiCcMessage badCtrl{ 1, 128, 64, "behringer_pro800", "cutoff", 0 };
    auto res2 = transport.sendCc(badCtrl);
    REQUIRE_FALSE(res2.acceptedForTransmission);
    CHECK(res2.error == MidiTransportError::InvalidMessage);

    // Valor 128 ilegal
    MidiCcMessage badVal{ 1, 19, 128, "behringer_pro800", "cutoff", 0 };
    auto res3 = transport.sendCc(badVal);
    REQUIRE_FALSE(res3.acceptedForTransmission);
    CHECK(res3.error == MidiTransportError::InvalidMessage);

    CHECK(transport.sentCc().empty());
}

TEST_CASE("MidiTransport: Send valid SysEx preserves exact bytes and metadata", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "dx7_port", "DX7 MIDI DIN", "yamaha_dx7" };
    REQUIRE(transport.open(sel).opened);

    std::vector<uint8_t> payload = { 0xF0, 0x43, 0x00, 0x09, 0x10, 0x7F, 0xF7 };
    MidiSysExMessage msg{ payload, "yamaha_dx7", "op1_level", "yamaha-dx7", 0 };

    auto res = transport.sendSysEx(msg);
    REQUIRE(res.acceptedForTransmission);
    CHECK(res.sequenceNumber == 1);

    REQUIRE(transport.sentSysEx().size() == 1);
    CHECK(transport.sentSysEx()[0].bytes == payload);
    CHECK(transport.sentSysEx()[0].targetProfileId == "yamaha_dx7");
    CHECK(transport.sentSysEx()[0].semanticId == "op1_level");
    CHECK(transport.sentSysEx()[0].sequenceNumber == 1);
}

TEST_CASE("MidiTransport: Send invalid SysEx frame returns InvalidMessage", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "dx7_port", "DX7 MIDI DIN", "yamaha_dx7" };
    REQUIRE(transport.open(sel).opened);

    // Falta F0
    MidiSysExMessage noF0{ { 0x43, 0x00, 0xF7 }, "dx7", "param", "none", 0 };
    auto res1 = transport.sendSysEx(noF0);
    REQUIRE_FALSE(res1.acceptedForTransmission);
    CHECK(res1.error == MidiTransportError::InvalidMessage);

    // Falta F7
    MidiSysExMessage noF7{ { 0xF0, 0x43, 0x00 }, "dx7", "param", "none", 0 };
    auto res2 = transport.sendSysEx(noF7);
    REQUIRE_FALSE(res2.acceptedForTransmission);
    CHECK(res2.error == MidiTransportError::InvalidMessage);

    // Byte ilegal en payload (0x80)
    MidiSysExMessage illegalByte{ { 0xF0, 0x43, 0x80, 0xF7 }, "dx7", "param", "none", 0 };
    auto res3 = transport.sendSysEx(illegalByte);
    REQUIRE_FALSE(res3.acceptedForTransmission);
    CHECK(res3.error == MidiTransportError::InvalidMessage);

    CHECK(transport.sentSysEx().empty());
}

TEST_CASE("MidiTransport: Send failure injection returns WriteFailed and does not record", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "port_uid_123", "USB MIDI Out 1", "behringer_pro800" };
    REQUIRE(transport.open(sel).opened);

    transport.failNextSend(MidiTransportError::WriteFailed, "ERR_MIDI_TRANSPORT_WRITE_FAILED");

    MidiCcMessage msg{ 1, 19, 50, "behringer_pro800", "cutoff", 0 };
    auto res = transport.sendCc(msg);
    REQUIRE_FALSE(res.acceptedForTransmission);
    CHECK(res.error == MidiTransportError::WriteFailed);
    CHECK(res.diagnosticCode == "ERR_MIDI_TRANSPORT_WRITE_FAILED");
    CHECK(transport.sentCc().empty());

    // El error es one-shot: el siguiente envío pasa
    auto res2 = transport.sendCc(msg);
    REQUIRE(res2.acceptedForTransmission);
    REQUIRE(transport.sentCc().size() == 1);
}

TEST_CASE("MidiTransport: Disconnect simulation returns Disconnected and close resets state", "[hardware][transport][d2]")
{
    MockMidiTransport transport;
    MidiPortSelection sel{ "port_uid_123", "USB MIDI Out 1", "behringer_pro800" };
    REQUIRE(transport.open(sel).opened);

    transport.simulateDisconnect();
    CHECK_FALSE(transport.isOpen());

    MidiCcMessage msg{ 1, 19, 50, "behringer_pro800", "cutoff", 0 };
    auto res = transport.sendCc(msg);
    REQUIRE_FALSE(res.acceptedForTransmission);
    CHECK(res.error == MidiTransportError::Disconnected);
    CHECK(res.diagnosticCode == "ERR_MIDI_DISCONNECTED");

    transport.close();
    CHECK_FALSE(transport.isOpen());
    CHECK(transport.getActivePortSelection().stableDeviceId.empty());
}
