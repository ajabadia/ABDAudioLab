#include "PluginHardwareContractAdapter.h"

namespace abdaudiolab::core
{

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
        fn.routingGuide.stimulusOutput = "Internal MIDI Injection (Direct Bus)";
        fn.routingGuide.responseInput = "Plugin Internal Audio Out (DAC Direct)";
        fn.routingGuide.notes = "Excitación autónoma por secuenciador MIDI interno sin latencia de hardware.";
    }
    else
    {
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

} // namespace abdaudiolab::core
