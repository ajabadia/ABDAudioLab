#include "profiling/TargetProfileLegacyAdapter.h"
#include <algorithm>
#include <variant>

namespace abdaudiolab::profiling
{

core::HardwareContract TargetProfileLegacyAdapter::toLegacyHardwareContract(const TargetProfile& profile)
{
    core::HardwareContract legacy;
    legacy.schemaVersion = "2.0";
    legacy.id = profile.targetProfileId;
    legacy.displayName = profile.displayName;
    legacy.description = "Adapted from canonical TargetProfile " + profile.targetProfileId;
    legacy.manufacturer = profile.vendor;
    legacy.brand = profile.vendor;
    legacy.model = profile.displayName;

    // Aliases e identificadores aceptados
    if (!profile.identity.canonicalTargetId.empty())
        legacy.aliases.push_back(profile.identity.canonicalTargetId);

    if (profile.targetProfileId == "hw-behringer-pro800-canonical")
        legacy.aliases.push_back("behringer_pro800");
    else if (profile.targetProfileId == "hw-yamaha-dx7-canonical")
        legacy.aliases.push_back("yamaha_dx7");
    else if (profile.targetProfileId == "hw-boss-ds1-canonical")
        legacy.aliases.push_back("boss_ds1_distortion");

    for (const auto& a : profile.identity.acceptedUniqueIds)
    {
        if (std::find(legacy.aliases.begin(), legacy.aliases.end(), a) == legacy.aliases.end())
            legacy.aliases.push_back(a);
    }

    // Metadatos MIDI Identity para detección y hotplug
    legacy.midiIdentity.manufacturer = profile.vendor;
    legacy.midiIdentity.model = profile.displayName;
    for (const auto& a : legacy.aliases)
    {
        legacy.midiIdentity.portNameMatches.push_back(a);
    }
    if (profile.targetProfileId == "hw-behringer-pro800-canonical")
    {
        legacy.midiIdentity.manufacturerIdHex = "00 20 32";
        legacy.midiIdentity.modelIdHex = "2C";
        legacy.midiIdentity.sysexHeaderHex = "00 20 32 00 01 24";
        legacy.midiIdentity.model = "PRO-800";
    }
    else if (profile.targetProfileId == "hw-yamaha-dx7-canonical")
    {
        legacy.midiIdentity.manufacturerIdHex = "43";
        legacy.midiIdentity.modelIdHex = "09";
        legacy.midiIdentity.sysexHeaderHex = "43 00 09";
        legacy.midiIdentity.model = "DX7";
    }

    bool hasMidiCc = false;
    bool hasSysEx = false;
    bool hasManual = false;

    core::HardwareFunction mainFunc;
    mainFunc.id = "main_controls";
    mainFunc.name = profile.displayName + " Controls";
    mainFunc.blockType = "TimeDynamic";
    mainFunc.suggestedStimulus = "LOG_SINE_SWEEP";
    mainFunc.excitationMode = core::ExcitationMode::AudioSweep;

    int idx = 1;
    for (const auto& mapping : profile.parameters)
    {
        core::HardwareControl ctrl;
        ctrl.index = idx++;
        ctrl.name = mapping.displayName.empty() ? mapping.semanticId : mapping.displayName;
        ctrl.minVal = static_cast<float>(mapping.normalizedRange.first);
        ctrl.maxVal = static_cast<float>(mapping.normalizedRange.second);
        ctrl.defaultVal = (ctrl.minVal + ctrl.maxVal) * 0.5f;

        if (std::holds_alternative<MidiCcIdentifier>(mapping.technicalIdentifier))
        {
            hasMidiCc = true;
            const auto& ccId = std::get<MidiCcIdentifier>(mapping.technicalIdentifier);
            ctrl.controlMethod = "MIDI_CC";
            ctrl.type = "MidiCC";
            ctrl.ccNumber = ccId.controllerNumber;
        }
        else if (std::holds_alternative<MidiSysExIdentifier>(mapping.technicalIdentifier))
        {
            hasSysEx = true;
            const auto& sysExId = std::get<MidiSysExIdentifier>(mapping.technicalIdentifier);
            ctrl.controlMethod = "SYSEX";
            ctrl.type = "Knob";
            ctrl.sysexAddress = sysExId.messageTemplate;
        }
        else if (std::holds_alternative<ManualOperatorIdentifier>(mapping.technicalIdentifier))
        {
            hasManual = true;
            const auto& manualId = std::get<ManualOperatorIdentifier>(mapping.technicalIdentifier);
            ctrl.controlMethod = "MANUAL";
            ctrl.type = manualId.controlWidget.empty() ? "Knob" : manualId.controlWidget;
        }
        else if (std::holds_alternative<Vst3ParameterIdentifier>(mapping.technicalIdentifier))
        {
            const auto& vst3Id = std::get<Vst3ParameterIdentifier>(mapping.technicalIdentifier);
            ctrl.controlMethod = "MANUAL";
            ctrl.type = "Normalized";
            ctrl.ccNumber = vst3Id.parameterIndex;
        }
        else
        {
            ctrl.controlMethod = "MANUAL";
            ctrl.type = "Normalized";
        }

        mainFunc.controls.push_back(ctrl);
    }

    if (profile.targetKind == "SoftwarePlugin")
    {
        legacy.deviceType = "SOFTWARE_PLUGIN";
    }
    else if (hasSysEx)
    {
        legacy.deviceType = "AUTOMATED_SYSEX";
    }
    else if (hasMidiCc)
    {
        legacy.deviceType = "AUTOMATED_MIDI_CC";
    }
    else if (hasManual)
    {
        legacy.deviceType = "ANALOGUE_PEDAL";
    }
    else
    {
        legacy.deviceType = "MANUAL_EURORACK";
    }

    legacy.functions.push_back(mainFunc);
    return legacy;
}

} // namespace abdaudiolab::profiling
