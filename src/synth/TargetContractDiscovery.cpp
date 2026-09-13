#include "TargetContractDiscovery.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace abdaudiolab::synth
{

static std::string toLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

ParameterRole TargetContractDiscovery::inferParameterRole(const std::string& nativeId, const std::string& name) const noexcept
{
    std::string idLower = toLower(nativeId);
    std::string nameLower = toLower(name);

    if (idLower.find("test_") != std::string::npos ||
        nameLower.find("test ") != std::string::npos ||
        idLower.find("infrastructure") != std::string::npos ||
        idLower.find("control_mode") != std::string::npos)
    {
        return ParameterRole::TestInfrastructure;
    }

    return ParameterRole::AudioControl;
}

ParameterCategory TargetContractDiscovery::inferParameterCategory(const std::string& name, ParameterRole role) const noexcept
{
    if (role == ParameterRole::TestInfrastructure)
    {
        return ParameterCategory::Custom;
    }

    std::string lower = toLower(name);

    if (lower.find("cutoff") != std::string::npos ||
        lower.find("filter") != std::string::npos ||
        lower.find("vcf") != std::string::npos ||
        lower.find("resonance") != std::string::npos ||
        lower.find("reso") != std::string::npos)
    {
        return ParameterCategory::Filter;
    }

    if (lower.find("attack") != std::string::npos ||
        lower.find("decay") != std::string::npos ||
        lower.find("sustain") != std::string::npos ||
        lower.find("release") != std::string::npos ||
        lower.find("env") != std::string::npos ||
        lower.find("adsr") != std::string::npos)
    {
        return ParameterCategory::Envelope;
    }

    if (lower.find("pitch") != std::string::npos ||
        lower.find("osc") != std::string::npos ||
        lower.find("wave") != std::string::npos ||
        lower.find("tune") != std::string::npos ||
        lower.find("octave") != std::string::npos ||
        lower.find("detune") != std::string::npos)
    {
        return ParameterCategory::Oscillator;
    }

    if (lower.find("gain") != std::string::npos ||
        lower.find("volume") != std::string::npos ||
        lower.find("level") != std::string::npos ||
        lower.find("master") != std::string::npos)
    {
        return ParameterCategory::Gain;
    }

    if (lower.find("lfo") != std::string::npos ||
        lower.find("mod") != std::string::npos ||
        lower.find("depth") != std::string::npos ||
        lower.find("rate") != std::string::npos)
    {
        return ParameterCategory::Modulation;
    }

    return ParameterCategory::Custom;
}

TargetContract TargetContractDiscovery::discoverContract(juce::AudioProcessor& processor) const
{
    TargetContract contract;
    contract.schemaVersion = "1.0.0";
    contract.name = processor.getName().toStdString();
    contract.manufacturer = "DiscoveredHostTarget";
    contract.targetVersion = "1.0.0";
    contract.format = "VST3";

    contract.acceptsMidi = processor.acceptsMidi();
    contract.numAudioInputs = processor.getTotalNumInputChannels();
    contract.numAudioOutputs = processor.getTotalNumOutputChannels();
    contract.declaredLatencySamples = static_cast<double>(processor.getLatencySamples());
    contract.declaredDeterministic = true;
    contract.supportsReset = true;

    auto params = processor.getParameters();
    contract.parameters.reserve(static_cast<size_t>(params.size()));

    for (int i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        if (p == nullptr)
            continue;

        TargetParameterDescriptor desc;

        // Extraer nativeId canónico
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
        {
            desc.nativeId = withId->paramID.toStdString();
        }
        else
        {
            desc.nativeId = "param_" + std::to_string(p->getParameterIndex());
        }

        desc.normalizedId = desc.nativeId;
        desc.nativeName = p->getName(128).toStdString();
        desc.defaultValue = static_cast<double>(p->getDefaultValue());
        desc.unit = p->getLabel().toStdString();
        desc.stepCount = p->getNumSteps();
        desc.isDiscrete = p->isDiscrete();
        desc.isAutomatable = p->isAutomatable();

        // 1. Separar parámetros musicales de infraestructura
        desc.role = inferParameterRole(desc.nativeId, desc.nativeName);

        // 2. Clasificar semántica provisional manteniendo origen de evidencia: "Descubrir no es comprender"
        desc.category = inferParameterCategory(desc.nativeName, desc.role);
        if (desc.category != ParameterCategory::Custom)
        {
            desc.semanticEvidence = SemanticEvidence::InferredFromName;
            desc.semanticStatus = SemanticStatus::Inferred;
        }
        else
        {
            desc.semanticEvidence = SemanticEvidence::Unknown;
            desc.semanticStatus = SemanticStatus::Unverified;
        }

        // 3. Suavizado metrológico: NO asumido en descubrimiento, se certifica experimentalmente
        desc.smoothing.declaredSmoothing = false;
        desc.smoothing.observedSmoothing = false;
        desc.smoothing.smoothingKnown = false;
        desc.smoothing.effectiveSmoothingTimeMs = 0.0;

        contract.parameters.push_back(desc);
    }

    contract.computeHash();
    return contract;
}

std::string TargetContractDiscovery::exportToJson(const TargetContract& contract) const
{
    std::ostringstream ss;
    ss << "{\n"
       << "  \"schemaVersion\": \"" << contract.schemaVersion << "\",\n"
       << "  \"name\": \"" << contract.name << "\",\n"
       << "  \"manufacturer\": \"" << contract.manufacturer << "\",\n"
       << "  \"targetVersion\": \"" << contract.targetVersion << "\",\n"
       << "  \"format\": \"" << contract.format << "\",\n"
       << "  \"acceptsMidi\": " << (contract.acceptsMidi ? "true" : "false") << ",\n"
       << "  \"numAudioInputs\": " << contract.numAudioInputs << ",\n"
       << "  \"numAudioOutputs\": " << contract.numAudioOutputs << ",\n"
       << "  \"declaredLatencySamples\": " << contract.declaredLatencySamples << ",\n"
       << "  \"parameterContractHash\": \"" << contract.parameterContractHash << "\",\n"
       << "  \"parameters\": [\n";

    for (size_t i = 0; i < contract.parameters.size(); ++i)
    {
        const auto& p = contract.parameters[i];
        ss << "    {\n"
           << "      \"nativeId\": \"" << p.nativeId << "\",\n"
           << "      \"nativeName\": \"" << p.nativeName << "\",\n"
           << "      \"defaultValue\": " << p.defaultValue << ",\n"
           << "      \"unit\": \"" << p.unit << "\",\n"
           << "      \"stepCount\": " << p.stepCount << ",\n"
           << "      \"isDiscrete\": " << (p.isDiscrete ? "true" : "false") << ",\n"
           << "      \"isAutomatable\": " << (p.isAutomatable ? "true" : "false") << ",\n"
           << "      \"role\": " << static_cast<int>(p.role) << ",\n"
           << "      \"category\": " << static_cast<int>(p.category) << ",\n"
           << "      \"semanticStatus\": " << static_cast<int>(p.semanticStatus) << ",\n"
           << "      \"semanticEvidence\": " << static_cast<int>(p.semanticEvidence) << "\n"
           << "    }" << (i + 1 < contract.parameters.size() ? "," : "") << "\n";
    }

    ss << "  ]\n"
       << "}\n";

    return ss.str();
}

} // namespace abdaudiolab::synth
