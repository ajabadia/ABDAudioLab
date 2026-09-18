#include "PluginHardwareContractAdapter.h"
#include "../../synth/Sha256.h"

namespace abdaudiolab::core
{

nlohmann::ordered_json Vst3SessionDescriptor::toJson() const
{
    nlohmann::ordered_json j;
    j["automationMode"] = automationMode;
    j["blockSize"] = blockSize;
    j["parameterListHash"] = parameterListHash;
    j["pluginIdentifier"] = pluginIdentifier;
    j["pluginVersion"] = pluginVersion;
    j["sampleRate"] = sampleRate;
    j["statePresetHash"] = statePresetHash;
    return j;
}

HardwareContract PluginHardwareContractAdapter::createContractFromPlugin(juce::AudioProcessor& plugin,
                                                                        const juce::PluginDescription& desc)
{
    HardwareContract contract;
    contract.schemaVersion = "2.0";
    contract.id = "plugin_" + juce::File::createLegalFileName(desc.fileOrIdentifier).toStdString();
    contract.displayName = desc.name.isNotEmpty() ? desc.name.toStdString() : "Plugin Virtual";
    contract.description = "Instancia de plugin virtual " + desc.pluginFormatName.toStdString() + " cargada en memoria.";
    contract.deviceType = "SOFTWARE_PLUGIN";
    contract.brand = desc.manufacturerName.isNotEmpty() ? desc.manufacturerName.toStdString() : "Generic";
    contract.brandLogo = "models/logos/generic_plugin.svg";
    contract.modelImage = desc.isInstrument ? "models/generic-instrument-plugin.png" : "models/generic-vst-plugin.png";
    contract.manufacturer = contract.brand;

    // Build main function
    HardwareFunction fn;
    fn.id = "fn_main_plugin_profile";
    fn.name = desc.name.toStdString() + " (Main Processing Block)";
    fn.blockType = desc.isInstrument ? "SynthOscillator" : "SpectrumFilter";
    fn.suggestedStimulus = desc.isInstrument ? "NOTE_ON_EXCITATION" : "LOG_SINE_SWEEP";
    fn.captureMode = "FIXED_TIME";
    fn.defaultBurstDurationSec = desc.isInstrument ? 2.5f : 1.5f;

    if (desc.isInstrument)
    {
        fn.excitationMode = ExcitationMode::MidiNotes;
        fn.measurementRecipe.recipeType = "INTERNAL_MIDI_EXCITATION";
        fn.measurementRecipe.excitationMode = ExcitationMode::MidiNotes;
        fn.measurementRecipe.description = desc.name.toStdString() + " Standard Profiling (Cutoff / Velocity Sweep)";

        NoteSequenceEvent defaultNote;
        defaultNote.noteNumber = 60; // C4
        defaultNote.velocity = 100;
        defaultNote.durationMs = 1500;
        fn.measurementRecipe.excitationNotes.push_back(defaultNote);

        fn.routingGuide.stimulusOutput = "Internal MIDI Injection (Direct Bus)";
        fn.routingGuide.responseInput = "Plugin Internal Audio Out (DAC Direct)";
        fn.routingGuide.notes = "Excitación autónoma por secuenciador MIDI interno sin latencia de hardware.";
    }
    else
    {
        fn.excitationMode = ExcitationMode::AudioSweep;
        fn.measurementRecipe.recipeType = "DIRECT_AUDIO_IN";
        fn.measurementRecipe.excitationMode = ExcitationMode::AudioSweep;
        fn.measurementRecipe.description = desc.name.toStdString() + " Audio Sweep Profile";

        fn.routingGuide.stimulusOutput = "Stimulus Generator (Internal Bus)";
        fn.routingGuide.responseInput = "Plugin Audio Output (Direct In)";
        fn.routingGuide.notes = "Lazo cerrado digital directo sin coloración de convertidores físicos.";
    }

    juce::Logger::writeToLog("[PluginContract] Creating HardwareContract for: '" + desc.name + "' from " + desc.pluginFormatName);

    // Introspect parameters
    const auto& params = plugin.getParameters();
    juce::Logger::writeToLog("[PluginContract] Introspecting " + juce::String(params.size()) + " plugin parameters...");
    int idx = 1;
    for (auto* param : params)
    {
        if (param == nullptr) continue;

        HardwareControl ctrl;
        ctrl.index = idx++;
        ctrl.name = param->getName(64).toStdString();
        ctrl.type = "Normalized";
        ctrl.controlMethod = "SOFTWARE_PLUGIN_PARAM";
        ctrl.minVal = 0.0f;
        ctrl.maxVal = 1.0f;
        ctrl.defaultVal = param->getDefaultValue();
        ctrl.unit = param->getLabel().toStdString();

        fn.controls.push_back(std::move(ctrl));
    }

    juce::Logger::writeToLog("[PluginContract] Introspected " + juce::String(fn.controls.size()) + " valid controls.");
    contract.functions.push_back(std::move(fn));
    return contract;
}

void PluginHardwareContractAdapter::setParameterNormalized(juce::AudioProcessor& plugin, int controlIndex, float normVal)
{
    const auto& params = plugin.getParameters();
    int zeroBased = controlIndex - 1;
    if (zeroBased >= 0 && zeroBased < params.size() && params[zeroBased] != nullptr)
    {
        params[zeroBased]->setValueNotifyingHost(std::clamp(normVal, 0.0f, 1.0f));
    }
}

float PluginHardwareContractAdapter::getParameterNormalized(const juce::AudioProcessor& plugin, int controlIndex)
{
    const auto& params = plugin.getParameters();
    int zeroBased = controlIndex - 1;
    if (zeroBased >= 0 && zeroBased < params.size() && params[zeroBased] != nullptr)
    {
        return params[zeroBased]->getValue();
    }
    return 0.0f;
}

void PluginHardwareContractAdapter::beginParameterEdit(juce::AudioProcessor& plugin, int controlIndex)
{
    const auto& params = plugin.getParameters();
    int zeroBased = controlIndex - 1;
    if (zeroBased >= 0 && zeroBased < params.size() && params[zeroBased] != nullptr)
    {
        params[zeroBased]->beginChangeGesture();
    }
}

void PluginHardwareContractAdapter::performParameterEdit(juce::AudioProcessor& plugin, int controlIndex, float normVal)
{
    setParameterNormalized(plugin, controlIndex, normVal);
}

void PluginHardwareContractAdapter::endParameterEdit(juce::AudioProcessor& plugin, int controlIndex)
{
    const auto& params = plugin.getParameters();
    int zeroBased = controlIndex - 1;
    if (zeroBased >= 0 && zeroBased < params.size() && params[zeroBased] != nullptr)
    {
        params[zeroBased]->endChangeGesture();
    }
}

std::string PluginHardwareContractAdapter::computeParameterListHash(const juce::AudioProcessor& plugin)
{
    std::string acc;
    const auto& params = plugin.getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        if (auto* p = params[i])
        {
            acc += std::to_string(i) + ":" + p->getName(64).toStdString() + ";";
        }
    }
    return synth::Sha256::computeHex(acc);
}

std::string PluginHardwareContractAdapter::computeStateHash(juce::AudioProcessor& plugin)
{
    juce::MemoryBlock block;
    plugin.getStateInformation(block);
    if (block.getSize() == 0)
        return synth::Sha256::computeHex("empty_state");
    return synth::Sha256::computeHex(std::string_view(static_cast<const char*>(block.getData()), block.getSize()));
}

Vst3SessionDescriptor PluginHardwareContractAdapter::createSessionDescriptor(
    juce::AudioProcessor& plugin,
    const juce::PluginDescription& desc,
    double sampleRate,
    int blockSize)
{
    Vst3SessionDescriptor descOut;
    descOut.pluginIdentifier = desc.fileOrIdentifier.toStdString();
    descOut.pluginVersion = desc.version.isNotEmpty() ? desc.version.toStdString() : "1.0.0";
    descOut.parameterListHash = computeParameterListHash(plugin);
    descOut.statePresetHash = computeStateHash(plugin);
    descOut.sampleRate = sampleRate;
    descOut.blockSize = blockSize;
    descOut.automationMode = "sample_accurate_gesture";
    return descOut;
}

} // namespace abdaudiolab::core
