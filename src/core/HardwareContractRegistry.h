#pragma once

#include <string>
#include <vector>
#include <memory>
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

namespace abdaudiolab::core
{

struct HardwareControl
{
    int index { 1 };
    std::string name;
    std::string type; // "Slider", "Knob", "Switch", "RotarySwitch", "Button", "MidiCC", "Normalized"
    std::string controlMethod { "MANUAL" }; // "MANUAL", "MIDI_CC", "NRPN", "SYSEX"
    int ccNumber { -1 };
    int nrpnNumber { -1 };
    std::string sysexAddress; // e.g. "10 00 00 01"
    float minVal { 0.0f };
    float maxVal { 1.0f };
    float defaultVal { 0.5f };
    std::string unit;
    std::vector<std::string> options; // For switches / selectors
};

struct HardwareRoutingGuide
{
    std::string stimulusOutput; // e.g. "Audio Out 1 (L) -> GATE IN (+5V pulse)"
    std::string responseInput;  // e.g. "ENV 1 OUT -> Audio In 1 (L)"
    std::string notes;          // e.g. "Manual gate button can also be used"
};

struct HardwareFunction
{
    std::string id;
    std::string name;
    std::string blockType; // "TimeDynamic", "SpectrumFilter", "WaveShaper", "CyclicModulator", "AmplitudeGain"
    std::string suggestedStimulus; // "GATE_PULSE", "LOG_SINE_SWEEP", "MULTILEVEL_RAMP", "SILENT_CAPTURE", "MULTI_CYCLE_MODULATION"
    std::string captureMode { "FIXED_TIME" }; // "FIXED_TIME", "ADAPTIVE_ENVELOPE", "INTEGRATED_TAIL"
    float defaultBurstDurationSec { 1.0f };
    float maxTimeoutSec { 60.0f };
    float silenceThresholdDb { -60.0f };
    HardwareRoutingGuide routingGuide;
    std::vector<HardwareControl> controls;
};

struct MidiIdentityContract
{
    std::string manufacturer;
    std::string manufacturerIdHex;         // e.g. "41", "42", "43", "44", "00 20 32"
    std::string model;
    std::string modelIdHex;                // e.g. "15", "58", "5A", "20", "2C", "09", "01"
    std::string familyIdHex;               // e.g. "00 00", "32 00"
    std::string sysexHeaderHex;            // e.g. "41 10 00 00 00 15" or "00 20 32 20"
    std::vector<std::string> portNameMatches; // Port substring keywords
};

struct HardwareContract
{
    std::string schemaVersion { "2.0" };
    std::string id;
    std::string displayName;
    std::string description;
    std::string deviceType; // "MANUAL_EURORACK", "ANALOGUE_PEDAL", "AUTOMATED_SYSEX", "AUTOMATED_MIDI_CC", "MOCK_DSP"
    std::string brand;
    std::string brandLogo;
    std::string modelImage;
    std::string manufacturer;
    std::string model;
    std::string modelIdHex;
    std::string autoDetectSysEx;
    std::string theme { "audiolab-light" };
    MidiIdentityContract midiIdentity;

    std::vector<HardwareFunction> functions;
};

class HardwareContractRegistry
{
public:
    HardwareContractRegistry() = default;
    ~HardwareContractRegistry() = default;

    bool loadContractsFromDirectory(const juce::File& contractsDir);

    [[nodiscard]] bool hasContracts() const noexcept { return !contracts.empty(); }
    [[nodiscard]] const std::vector<HardwareContract>& getContracts() const noexcept { return contracts; }
    [[nodiscard]] const HardwareContract* findContractById(const std::string& id) const noexcept;
    [[nodiscard]] const std::string& getLastError() const noexcept { return lastErrorMessage; }

private:
    std::vector<HardwareContract> contracts;
    std::string lastErrorMessage;
};

} // namespace abdaudiolab::core
