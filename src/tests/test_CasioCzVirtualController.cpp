#include <catch2/catch_test_macros.hpp>
#include "hardware/CasioCzVirtualController.h"

using namespace abdaudiolab::hardware;

TEST_CASE("CasioCzVirtualController - Initialization & Port Discovery Safety", "[CasioCzController]")
{
    CasioCzVirtualController controller;

    SECTION("Default connection parameters and state")
    {
        CHECK(controller.getHardwareName() == "Casio CZ (MAME Core / VES)");
        CHECK(controller.isAutomatic());
        CHECK(controller.getMidiChannel() == 1);
        CHECK(controller.getSettlingDelayMs() == 5);
    }

    SECTION("Port search with arbitrary identifier does not crash")
    {
        controller.setTargetDeviceIdentifier("NonExistent_Virtual_Port_9999");
        // En plataformas de test sin driver loopMIDI físico, connect busca o fallbackea de forma segura
        bool connected = controller.connect();
        // Si hay algún puerto MIDI en el sistema o no, connect no debe lanzar excepción
        CHECK((connected == controller.isConnected()));
        controller.disconnect();
        CHECK_FALSE(controller.isConnected());
    }

    SECTION("MIDI Channel configuration within legal bounds")
    {
        controller.setMidiChannel(10);
        CHECK(controller.getMidiChannel() == 10);

        controller.setMidiChannel(0);
        CHECK(controller.getMidiChannel() == 1);

        controller.setMidiChannel(17);
        CHECK(controller.getMidiChannel() == 16);
    }
}

TEST_CASE("CasioCzVirtualController - SysEx 4-Bit Nibble Packing", "[CasioCzController]")
{
    CasioCzVirtualController controller;
    controller.clearSentMessages();
    controller.setMidiChannel(1);
    controller.setSettlingDelayMs(0); // Ejecución rápida para tests unitarios

    SECTION("Casio CZ Standard Parameter Change Structure")
    {
        // Enviar parámetro de prueba: Opcode MSB=0x20 (DCW), LSB=0x00 (Rate 1), Value=127
        // 127 = 0x7F -> MSN = (127 >> 4) & 0x0F = 7 (0x07), LSN = 127 & 0x0F = 15 (0x0F)
        REQUIRE(controller.sendCzParameter(0x20, 0x00, 127));

        const auto& msgs = controller.getSentMessages();
        REQUIRE_FALSE(msgs.empty());

        const auto& lastMsg = msgs.back();
        CHECK(lastMsg.isSysEx());

        const uint8_t* rawData = lastMsg.getRawData();
        int size = lastMsg.getRawDataSize();
        REQUIRE(size == 9);

        CHECK(rawData[0] == 0xF0); // SysEx Start
        CHECK(rawData[1] == 0x44); // Casio ID
        CHECK(rawData[2] == 0x00); // Sub-status
        CHECK(rawData[3] == 0x00); // Channel 1 (0-indexed)
        CHECK(rawData[4] == 0x20); // Opcode MSB
        CHECK(rawData[5] == 0x00); // Opcode LSB
        CHECK(rawData[6] == 0x07); // MSN
        CHECK(rawData[7] == 0x0F); // LSN
        CHECK(rawData[8] == 0xF7); // SysEx End
    }

    SECTION("Channel encoding into SysEx byte 3")
    {
        controller.setMidiChannel(5);
        REQUIRE(controller.sendCzParameter(0x00, 0x01, 50));

        const auto& lastMsg = controller.getSentMessages().back();
        const uint8_t* rawData = lastMsg.getRawData();
        CHECK(rawData[3] == 0x04); // Canal 5 -> 0x04

        // Value 50 = 0x32 -> MSN = 3, LSN = 2
        CHECK(rawData[6] == 0x03);
        CHECK(rawData[7] == 0x02);
    }

    SECTION("Canonical Opcode Table Coverage: DCO1 Waveform & Octave")
    {
        controller.clearSentMessages();
        // Param 1 = DCO1 Waveform (maxRaw = 99)
        CHECK(controller.setParameter(1, 0.5f));
        REQUIRE_FALSE(controller.getSentMessages().empty());
        const uint8_t* rawDco = controller.getSentMessages().back().getRawData();
        CHECK(rawDco[4] == 0x00);
        CHECK(rawDco[5] == 0x01);

        // Param 2 = DCO1 Octave Select (maxRaw = 3)
        controller.clearSentMessages();
        CHECK(controller.setParameterRaw(2, 2));
        REQUIRE_FALSE(controller.getSentMessages().empty());
        const uint8_t* rawOct = controller.getSentMessages().back().getRawData();
        CHECK(rawOct[4] == 0x00);
        CHECK(rawOct[5] == 0x02);
        CHECK(rawOct[6] == 0x00);
        CHECK(rawOct[7] == 0x02);
    }
}

TEST_CASE("CasioCzVirtualController - Multi-Step Envelope Automation", "[CasioCzController]")
{
    CasioCzVirtualController controller;
    controller.clearSentMessages();
    controller.setSettlingDelayMs(0);

    SECTION("DCW Step 1 and Step 8 Opcode Computation")
    {
        // envType: 1=DCW, line: 1, step: 1, rate: 80, level: 90
        REQUIRE(controller.sendCzEnvelopeStep(1, 1, 1, 80, 90));
        // Genera 2 mensajes SysEx (uno para Rate y otro para Level)
        REQUIRE(controller.getSentMessages().size() == 2);

        // Mensaje de Rate (Step 1 -> LSB = 0)
        const uint8_t* rData = controller.getSentMessages()[0].getRawData();
        CHECK(rData[4] == 0x20); // DCW base MSB
        CHECK(rData[5] == 0x00); // Rate Step 1 LSB
        CHECK(rData[6] == ((80 >> 4) & 0x0F));
        CHECK(rData[7] == (80 & 0x0F));

        // Mensaje de Level (Step 1 -> LSB = 1)
        const uint8_t* lData = controller.getSentMessages()[1].getRawData();
        CHECK(lData[4] == 0x20);
        CHECK(lData[5] == 0x01); // Level Step 1 LSB
        CHECK(lData[6] == ((90 >> 4) & 0x0F));
        CHECK(lData[7] == (90 & 0x0F));
    }

    SECTION("Line 2 DCA Step 4 with Offset")
    {
        controller.clearSentMessages();
        // envType: 2=DCA (base 0x30), line: 2 (+0x40 -> 0x70), step: 4 (LSB: (4-1)*2 = 6)
        REQUIRE(controller.sendCzEnvelopeStep(2, 2, 4, 64, 48));
        REQUIRE(controller.getSentMessages().size() == 2);

        const uint8_t* rData = controller.getSentMessages()[0].getRawData();
        CHECK(rData[4] == 0x70); // 0x30 + 0x40
        CHECK(rData[5] == 0x06); // Step 4 Rate LSB
    }
}

TEST_CASE("CasioCzVirtualController - Dynamic JSON Opcode Mapping", "[CasioCzController]")
{
    CasioCzVirtualController controller;
    controller.clearSentMessages();
    controller.setSettlingDelayMs(0);

    SECTION("Load custom opcodes from JSON string")
    {
        juce::String customJson = R"({
            "opcodes": [
                { "index": 501, "msb": 64, "lsb": 10, "maxRaw": 127 },
                { "index": 502, "msb": 64, "lsb": 11, "maxRaw": 63 }
            ]
        })";

        REQUIRE(controller.loadParameterMappingJson(customJson));

        // Probar el nuevo parámetro 501
        CHECK(controller.setParameterRaw(501, 100));
        REQUIRE_FALSE(controller.getSentMessages().empty());
        const uint8_t* rawData = controller.getSentMessages().back().getRawData();
        CHECK(rawData[4] == 64);
        CHECK(rawData[5] == 10);
        CHECK(rawData[6] == ((100 >> 4) & 0x0F));
        CHECK(rawData[7] == (100 & 0x0F));
    }

    SECTION("Invalid JSON handling does not throw")
    {
        CHECK_FALSE(controller.loadParameterMappingJson("invalid { not json"));
    }
}
