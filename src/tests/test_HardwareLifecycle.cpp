#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/HardwareContractRegistry.h"
#include "core/ProfilingSequencer.h"
#include "hardware/MockHardwareController.h"
#include "audio/LabAudioEngine.h"

TEST_CASE("HardwareLifecycleContract JSON Parsing", "[core][hardware][lifecycle]")
{
    using namespace abdaudiolab;
    using namespace abdaudiolab::core;

    const char* testJson = R"json({
        "id": "sequential_take5_sim",
        "displayName": "Sequential Take 5 (Test Profile)",
        "deviceType": "AUTOMATED_MIDI_CC",
        "lifecycle": {
            "preCalibrationSetup": [
                {
                    "description": "Disable Reverb FX",
                    "method": "MIDI_CC",
                    "channel": 2,
                    "controlNumber": 91,
                    "normalizedValue": 0.0,
                    "settlingDelayMs": 10
                }
            ],
            "preSessionSetup": [
                {
                    "description": "Zero Vintage Parameter",
                    "method": "NRPN",
                    "channel": 1,
                    "controlNumber": 128,
                    "normalizedValue": 0.5,
                    "settlingDelayMs": 15
                },
                {
                    "description": "Send Master Reset SysEx",
                    "method": "SYSEX_RAW",
                    "channel": 1,
                    "sysexHexPayload": "F0 41 10 00 00 00 15 12 F7",
                    "settlingDelayMs": 5
                },
                {
                    "description": "Set resonance knob to center",
                    "method": "MANUAL_PROMPT"
                }
            ],
            "postSessionTeardown": [
                {
                    "description": "Restore FX level",
                    "method": "MIDI_CC",
                    "channel": 2,
                    "controlNumber": 91,
                    "normalizedValue": 0.35,
                    "settlingDelayMs": 10
                }
            ]
        }
    })json";

    juce::File tempFile = juce::File::createTempFile("test_contract.json");
    tempFile.replaceWithText(testJson);

    juce::File tempDir = tempFile.getParentDirectory();

    HardwareContractRegistry registry;
    bool loaded = registry.loadContractsFromDirectory(tempDir);
    REQUIRE(loaded);

    const auto* contract = registry.findContractById("sequential_take5_sim");
    REQUIRE(contract != nullptr);
    REQUIRE(contract->displayName == "Sequential Take 5 (Test Profile)");

    // Verify Lifecycle Declarations
    const auto& lc = contract->lifecycle;
    REQUIRE(lc.preCalibrationSetup.size() == 1);
    CHECK(lc.preCalibrationSetup[0].description == "Disable Reverb FX");
    CHECK(lc.preCalibrationSetup[0].method == HardwareMethod::MIDI_CC);
    CHECK(lc.preCalibrationSetup[0].channel == 2);
    CHECK(lc.preCalibrationSetup[0].controlNumber == 91);
    CHECK(lc.preCalibrationSetup[0].normalizedValue == 0.0f);
    CHECK(lc.preCalibrationSetup[0].settlingDelayMs == 10);

    REQUIRE(lc.preSessionSetup.size() == 3);
    CHECK(lc.preSessionSetup[0].description == "Zero Vintage Parameter");
    CHECK(lc.preSessionSetup[0].method == HardwareMethod::NRPN);
    CHECK(lc.preSessionSetup[0].controlNumber == 128);
    CHECK(lc.preSessionSetup[0].normalizedValue == 0.5f);

    CHECK(lc.preSessionSetup[1].method == HardwareMethod::SYSEX_RAW);
    CHECK(lc.preSessionSetup[1].sysexHexPayload == "F0 41 10 00 00 00 15 12 F7");

    CHECK(lc.preSessionSetup[2].method == HardwareMethod::MANUAL_PROMPT);

    REQUIRE(lc.postSessionTeardown.size() == 1);
    CHECK(lc.postSessionTeardown[0].controlNumber == 91);
    REQUIRE_THAT(lc.postSessionTeardown[0].normalizedValue, Catch::Matchers::WithinAbs(0.35f, 0.01f));

    tempFile.deleteFile();
}

TEST_CASE("ProfilingSequencer executeLifecycleActions Dispatch", "[core][hardware][lifecycle]")
{
    using namespace abdaudiolab;
    using namespace abdaudiolab::core;

    audio::LabAudioEngine engine;
    hardware::MockHardwareController mockHw;
    ProfilingSequencer sequencer(engine, mockHw);

    mockHw.clearSentMessages();

    std::vector<HardwareSetupAction> actions;

    // 1. MIDI CC: Cutoff (CC 74) to 75% on channel 3
    HardwareSetupAction ccAction;
    ccAction.description = "Set Cutoff to 75%";
    ccAction.method = HardwareMethod::MIDI_CC;
    ccAction.channel = 3;
    ccAction.controlNumber = 74;
    ccAction.normalizedValue = 0.75f;
    ccAction.settlingDelayMs = 0;
    actions.push_back(ccAction);

    // 2. NRPN: Param 258 to 8192 (mid-range 50%) on channel 1
    // 258 = (2 << 7) | 2 -> MSB=2, LSB=2
    // 8192 = (64 << 7) | 0 -> MSB=64, LSB=0
    HardwareSetupAction nrpnAction;
    nrpnAction.description = "Set NRPN 258 to 50%";
    nrpnAction.method = HardwareMethod::NRPN;
    nrpnAction.channel = 1;
    nrpnAction.controlNumber = 258;
    nrpnAction.normalizedValue = 0.5f;
    nrpnAction.settlingDelayMs = 0;
    actions.push_back(nrpnAction);

    // 3. SYSEX RAW
    HardwareSetupAction sysexAction;
    sysexAction.description = "Test SysEx Ping";
    sysexAction.method = HardwareMethod::SYSEX_RAW;
    sysexAction.sysexHexPayload = "F0 00 20 32 01 02 F7";
    sysexAction.settlingDelayMs = 0;
    actions.push_back(sysexAction);

    // 4. MANUAL PROMPT
    bool promptReceived = false;
    juce::String receivedPrompt;
    sequencer.setManualPromptCallback([&](const juce::String& p) {
        promptReceived = true;
        receivedPrompt = p;
    });

    HardwareSetupAction promptAction;
    promptAction.description = "Please turn knob manually to 12 o'clock";
    promptAction.method = HardwareMethod::MANUAL_PROMPT;
    actions.push_back(promptAction);

    sequencer.executeLifecycleActions(actions);

    const auto& sent = mockHw.getSentMessages();
    // CC produces 1 message, NRPN produces 4 messages (CC 99, 98, 6, 38), SysEx produces 1 message = 6 total
    REQUIRE(sent.size() == 6);

    // Verify MIDI CC
    CHECK(sent[0].isController());
    CHECK(sent[0].getChannel() == 3);
    CHECK(sent[0].getControllerNumber() == 74);
    CHECK(sent[0].getControllerValue() == static_cast<int>(std::round(0.75f * 127.0f)));

    // Verify NRPN 258 (MSB=2, LSB=2) and Value 8192 (MSB=64, LSB=0)
    CHECK(sent[1].isControllerOfType(99));
    CHECK(sent[1].getControllerValue() == 2);
    CHECK(sent[2].isControllerOfType(98));
    CHECK(sent[2].getControllerValue() == 2);
    CHECK(sent[3].isControllerOfType(6));
    CHECK(sent[3].getControllerValue() == 64);
    CHECK(sent[4].isControllerOfType(38));
    CHECK(sent[4].getControllerValue() == 0);

    // Verify SysEx (Full 7-byte raw packet, 5-byte inner payload)
    CHECK(sent[5].isSysEx());
    CHECK(sent[5].getRawDataSize() == 7);
    CHECK(sent[5].getSysExDataSize() == 5);
    const uint8_t* raw = sent[5].getRawData();
    CHECK(raw[0] == 0xF0);
    CHECK(raw[1] == 0x00);
    CHECK(raw[2] == 0x20);
    CHECK(raw[3] == 0x32);
    CHECK(raw[4] == 0x01);
    CHECK(raw[5] == 0x02);
    CHECK(raw[6] == 0xF7);
}
