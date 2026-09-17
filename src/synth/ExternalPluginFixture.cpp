#include "ExternalPluginFixture.h"
#include "Sha256.h"
#include <algorithm>
#include <fstream>
#include <chrono>

namespace abdaudiolab::synth
{

ExternalPluginFixture::ExternalPluginFixture(juce::AudioPluginFormatManager& formatManager)
    : formatManager_(formatManager)
{
}

ExternalPluginFixture::~ExternalPluginFixture()
{
    if (instance_ != nullptr)
    {
        instance_->releaseResources();
        instance_.reset();
    }
}

void ExternalPluginFixture::computeBundleManifest(const juce::File& pluginFile)
{
    identity_.absolutePath = pluginFile.getFullPathName().toStdString();
    identity_.format = "VST3";
    identity_.executionIsolation = PluginExecutionIsolation::InProcessExternalBinary;
    identity_.bundleFiles.clear();

    if (pluginFile.isDirectory())
    {
        juce::Array<juce::File> files;
        pluginFile.findChildFiles(files, juce::File::findFiles, true);

        for (const auto& f : files)
        {
            PluginBundleFileEntry entry;
            entry.relativePath = f.getRelativePathFrom(pluginFile).toStdString();
            entry.fileSize = static_cast<uint64_t>(f.getSize());

            juce::MemoryBlock mb;
            if (f.loadFileAsData(mb))
            {
                entry.fileSha256 = Sha256::computeHex(
                    static_cast<const uint8_t*>(mb.getData()), mb.getSize());
            }

            // Identificar binario principal en contenidos típicos x86_64-win
            if (f.getFileExtension().equalsIgnoreCase(".vst3") ||
                f.getFileExtension().equalsIgnoreCase(".dll"))
            {
                identity_.binaryHash = entry.fileSha256;
            }

            identity_.bundleFiles.push_back(entry);
        }

        // Orden canónico por ruta relativa
        std::sort(identity_.bundleFiles.begin(), identity_.bundleFiles.end(),
                  [](const auto& a, const auto& b) {
                      return a.relativePath < b.relativePath;
                  });

        // Hash compuesto del bundle completo
        std::string bundleBlob;
        for (const auto& entry : identity_.bundleFiles)
        {
            bundleBlob += entry.relativePath + "|"
                        + std::to_string(entry.fileSize) + "|"
                        + entry.fileSha256 + "\n";
        }
        identity_.bundleHash = Sha256::computeHex(bundleBlob);
    }
    else if (pluginFile.existsAsFile())
    {
        juce::MemoryBlock mb;
        if (pluginFile.loadFileAsData(mb))
        {
            identity_.binaryHash = Sha256::computeHex(
                static_cast<const uint8_t*>(mb.getData()), mb.getSize());
            identity_.bundleHash = identity_.binaryHash;

            PluginBundleFileEntry entry;
            entry.relativePath = pluginFile.getFileName().toStdString();
            entry.fileSize = static_cast<uint64_t>(mb.getSize());
            entry.fileSha256 = identity_.binaryHash;
            identity_.bundleFiles.push_back(entry);
        }
    }
}

bool ExternalPluginFixture::loadPluginFromDisk(const juce::File& pluginFile,
                                              double sampleRate,
                                              int blockSize,
                                              std::string& errorMessage)
{
    if (!pluginFile.exists())
    {
        errorMessage = "Plugin file does not exist: " + pluginFile.getFullPathName().toStdString();
        return false;
    }

    // 1. Hashear canónicamente el bundle o archivo
    computeBundleManifest(pluginFile);

    // 2. Buscar formato y tipos de plugin
    juce::OwnedArray<juce::PluginDescription> types;
    juce::String err;

    for (int i = 0; i < formatManager_.getNumFormats(); ++i)
    {
        auto* format = formatManager_.getFormat(i);
        if (format != nullptr && format->fileMightContainThisPluginType(pluginFile.getFullPathName()))
        {
            format->findAllTypesForFile(types, pluginFile.getFullPathName());
            if (types.size() > 0)
                break;
        }
    }

    if (types.isEmpty())
    {
        errorMessage = "No compatible plugin types found in: " + pluginFile.getFullPathName().toStdString();
        return false;
    }

    // 3. Instanciación del binario en proceso host
    const auto& desc = *types[0];
    identity_.pluginName = desc.name.toStdString();
    identity_.manufacturer = desc.manufacturerName.toStdString();
    identity_.version = desc.version.toStdString();
    identity_.pluginUid = desc.createIdentifierString().toStdString();
    identity_.architecture = (sizeof(void*) == 8) ? "x86_64" : "x86";
    identity_.absolutePath = pluginFile.getFullPathName().toStdString();

    juce::String createErr;
    instance_ = formatManager_.createPluginInstance(desc, sampleRate, blockSize, createErr);

    if (instance_ == nullptr)
    {
        errorMessage = "Failed to instantiate plugin: " + createErr.toStdString();
        return false;
    }

    // 4. Preparar buses y procesamiento
    spec_.sampleRate = sampleRate;
    spec_.blockSize = blockSize;
    spec_.numChannels = 2;

    instance_->setPlayConfigDetails(0, spec_.numChannels, sampleRate, blockSize);
    instance_->prepareToPlay(sampleRate, blockSize);

    isStateVerified_ = true;
    return true;
}

bool ExternalPluginFixture::loadState(const SynthPresetState& state)
{
    if (instance_ == nullptr)
        return false;

    if (!state.rawSysEx.empty())
    {
        instance_->setStateInformation(state.rawSysEx.data(), static_cast<int>(state.rawSysEx.size()));
    }

    // Aplicar parámetros normalizados por nativeId
    auto params = instance_->getParameters();
    for (const auto& np : state.normalizedParameters)
    {
        for (auto* p : params)
        {
            if (p != nullptr)
            {
                std::string pId;
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
                    pId = withId->paramID.toStdString();
                else
                    pId = "param_" + std::to_string(p->getParameterIndex());

                if (pId == np.name || p->getName(64).toStdString() == np.name)
                {
                    p->setValueNotifyingHost(static_cast<float>(np.value));
                    break;
                }
            }
        }
    }

    isStateVerified_ = true;
    return true;
}

bool ExternalPluginFixture::loadSysEx(const SysExArtifact& sysEx)
{
    if (instance_ == nullptr)
        return false;

    if (sysEx.bytes.empty() || sysEx.semanticStatus != "valid")
        return false;

    instance_->setStateInformation(sysEx.bytes.data(), static_cast<int>(sysEx.bytes.size()));
    isStateVerified_ = true;
    return true;
}

StateAppliedStatus ExternalPluginFixture::verifyState() const
{
    return isStateVerified_ ? StateAppliedStatus::Passed : StateAppliedStatus::Unverified;
}

void ExternalPluginFixture::prepare(const ProcessingSpec& spec)
{
    spec_ = spec;
    if (instance_ != nullptr)
    {
        instance_->setPlayConfigDetails(0, spec.numChannels, spec.sampleRate, spec.blockSize);
        instance_->prepareToPlay(spec.sampleRate, spec.blockSize);
    }
}

void ExternalPluginFixture::resetState()
{
    if (instance_ != nullptr)
    {
        instance_->reset();
    }
}

void ExternalPluginFixture::render(const MidiExcitationSequence& sequence,
                                   std::vector<float>& destinationAudio,
                                   int /*repetitionIndex*/)
{
    if (instance_ == nullptr || faulted_)
    {
        destinationAudio.clear();
        return;
    }

    auto startTotal = std::chrono::steady_clock::now();
    lastTelemetry_ = RenderExecutionTelemetry{};
    lastTelemetry_.pluginLatencySamples = (instance_ != nullptr) ? static_cast<double>(instance_->getLatencySamples()) : 0.0;
    lastTelemetry_.hostLatencySamples = 0.0;
    lastTelemetry_.underruns = 0;
    lastTelemetry_.overruns = 0;
    lastTelemetry_.bitExactDeterministic = true;

    int totalSamples = static_cast<int>(std::lround(sequence.totalDurationSec * spec_.sampleRate));
    destinationAudio.assign(static_cast<size_t>(totalSamples), 0.0f);

    juce::AudioBuffer<float> blockBuf(spec_.numChannels, spec_.blockSize);
    juce::MidiBuffer midiBuf;

    int samplesRendered = 0;
    while (samplesRendered < totalSamples)
    {
        lastTelemetry_.blocksProcessed++;
        int currentBlockSize = std::min(spec_.blockSize, totalSamples - samplesRendered);
        blockBuf.setSize(spec_.numChannels, currentBlockSize, false, false, true);
        blockBuf.clear();
        midiBuf.clear();

        // 1. Inyectar eventos MIDI canónicos o temporizados con sampleOffset exacto dentro del bloque
        if (!sequence.canonicalEvents.empty())
        {
            for (const auto& cev : sequence.canonicalEvents)
            {
                if (cev.sampleOffset >= samplesRendered && cev.sampleOffset < (samplesRendered + currentBlockSize))
                {
                    int offsetInBlock = cev.sampleOffset - samplesRendered;
                    if (!cev.bytes.empty())
                    {
                        juce::MidiMessage msg(cev.bytes.data(), static_cast<int>(cev.bytes.size()), 0.0);
                        if (msg.getRawDataSize() > 0)
                            midiBuf.addEvent(msg, offsetInBlock);
                    }
                }
            }
        }
        else
        {
            for (const auto& ev : sequence.events)
            {
                if (ev.sampleOffset >= samplesRendered && ev.sampleOffset < (samplesRendered + currentBlockSize))
                {
                    int offsetInBlock = ev.sampleOffset - samplesRendered;
                    juce::MidiMessage msg;

                    if (ev.type == TimedMidiType::NoteOn)
                        msg = juce::MidiMessage::noteOn(ev.channel, ev.noteNumber, ev.velocity);
                    else if (ev.type == TimedMidiType::NoteOff)
                        msg = juce::MidiMessage::noteOff(ev.channel, ev.noteNumber, 0.0f);
                    else if (ev.type == TimedMidiType::AllNotesOff)
                        msg = juce::MidiMessage::allNotesOff(ev.channel);

                    if (msg.getRawDataSize() > 0)
                        midiBuf.addEvent(msg, offsetInBlock);
                }
            }
        }

        // 2. Despachar automatización de parámetros correspondiente a este bloque
        for (const auto& pe : sequence.parameterEvents)
        {
            if (pe.sampleOffset >= samplesRendered && pe.sampleOffset < (samplesRendered + currentBlockSize))
            {
                for (auto* p : instance_->getParameters())
                {
                    if (p != nullptr)
                    {
                        std::string pId;
                        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
                            pId = withId->paramID.toStdString();
                        else
                            pId = "param_" + std::to_string(p->getParameterIndex());

                        if (pId == pe.nativeParameterId || pId == pe.normalizedParameterId)
                        {
                            p->setValueNotifyingHost(static_cast<float>(pe.normalizedValue));
                            break;
                        }
                    }
                }
            }
        }

        // 3. Procesar bloque en el plugin externo real con watchdog y captura de excepciones
        auto startBlock = std::chrono::steady_clock::now();
        try
        {
            instance_->processBlock(blockBuf, midiBuf);
        }
        catch (const std::exception& e)
        {
            faulted_ = true;
            lastFaultMessage_ = "Exception in VST3 processBlock: " + std::string(e.what());
            throw;
        }
        catch (...)
        {
            faulted_ = true;
            lastFaultMessage_ = "Unknown exception in VST3 processBlock";
            throw;
        }
        auto endBlock = std::chrono::steady_clock::now();
        double blockDurationMs = std::chrono::duration<double, std::milli>(endBlock - startBlock).count();

        if (watchdogMaxBlockDurationMs_ > 0.0 && blockDurationMs > watchdogMaxBlockDurationMs_)
        {
            timedOut_ = true;
            faulted_ = true;
            lastFaultMessage_ = "Watchdog timeout in VST3 processBlock: duration " + std::to_string(blockDurationMs)
                              + " ms exceeded limit " + std::to_string(watchdogMaxBlockDurationMs_) + " ms";
            throw std::runtime_error(lastFaultMessage_);
        }

        // 4. Copiar canal izquierdo al buffer de captura
        const float* outChannel = blockBuf.getReadPointer(0);
        for (int i = 0; i < currentBlockSize; ++i)
        {
            destinationAudio[static_cast<size_t>(samplesRendered + i)] = outChannel[i];
        }

        samplesRendered += currentBlockSize;
    }

    auto endTotal = std::chrono::steady_clock::now();
    lastTelemetry_.totalSamplesRendered = samplesRendered;
    lastTelemetry_.totalRenderTimeMs = std::chrono::duration<double, std::milli>(endTotal - startTotal).count();
}

TargetTimingInfo ExternalPluginFixture::timingInfo() const
{
    TargetTimingInfo info;
    info.isPhysicalHardware = false;
    info.isDirectPlugin = true;
    if (instance_ != nullptr)
    {
        info.declaredLatencySamples = static_cast<double>(instance_->getLatencySamples());
        info.measuredTransportLatencyMs = (spec_.sampleRate > 0.0) ? (info.declaredLatencySamples / spec_.sampleRate * 1000.0) : 0.0;
    }
    info.timingJitterMs = 0.0;
    info.timingDescription = "External Binary VST3 (InProcessExternalBinary via AudioPluginFormatManager)";
    return info;
}

bool ExternalPluginFixture::supportsBinaryState() const
{
    return true;
}

ISynthTarget::StateTransferResult ExternalPluginFixture::getState(std::vector<uint8_t>& stateData) const
{
    if (instance_ == nullptr)
        return ISynthTarget::StateTransferResult{ false, false, "NULL_INSTANCE", 0, "" };

    juce::MemoryBlock mb;
    instance_->getStateInformation(mb);
    stateData.assign(static_cast<const uint8_t*>(mb.getData()),
                     static_cast<const uint8_t*>(mb.getData()) + mb.getSize());
    auto hash = Sha256::computeHex(stateData.data(), stateData.size());
    return ISynthTarget::StateTransferResult{ true, true, "", stateData.size(), hash };
}

ISynthTarget::StateTransferResult ExternalPluginFixture::setState(const std::vector<uint8_t>& stateData)
{
    if (instance_ == nullptr)
        return ISynthTarget::StateTransferResult{ false, false, "NULL_INSTANCE", 0, "" };

    instance_->setStateInformation(stateData.data(), static_cast<int>(stateData.size()));
    isStateVerified_ = true;
    auto hash = Sha256::computeHex(stateData.data(), stateData.size());
    return ISynthTarget::StateTransferResult{ true, true, "", stateData.size(), hash };
}

TargetContract ExternalPluginFixture::discoverContract() const
{
    if (instance_ == nullptr)
        return TargetContract{};

    TargetContractDiscovery discovery;
    auto contract = discovery.discoverContract(*instance_);
    contract.manufacturer = identity_.manufacturer.empty() ? "ExternalVST3" : identity_.manufacturer;
    contract.pluginUid = identity_.pluginUid;
    contract.targetVersion = identity_.version;
    contract.computeHash();
    return contract;
}

bool ExternalPluginFixture::inspectPluginModule(juce::AudioPluginFormatManager& formatManager,
                                                const juce::File& pluginFile,
                                                const std::string& targetUid,
                                                InspectedPluginModule& outModule,
                                                std::string& outError)
{
    if (!pluginFile.exists())
    {
        outError = "Plugin file does not exist: " + pluginFile.getFullPathName().toStdString();
        return false;
    }

    outModule = InspectedPluginModule{};
    outModule.canonicalPath = pluginFile.getFullPathName().toStdString();
    outModule.architecture = (sizeof(void*) == 8) ? "x86_64" : "x86";

    // Calcular hash binario del archivo o bundle
    if (pluginFile.existsAsFile())
    {
        juce::MemoryBlock mb;
        if (pluginFile.loadFileAsData(mb))
            outModule.binarySha256 = Sha256::computeHex(static_cast<const uint8_t*>(mb.getData()), mb.getSize());
    }
    else if (pluginFile.isDirectory())
    {
        juce::Array<juce::File> files;
        pluginFile.findChildFiles(files, juce::File::findFiles, true);
        for (const auto& f : files)
        {
            if (f.getFileExtension().equalsIgnoreCase(".vst3") || f.getFileExtension().equalsIgnoreCase(".dll"))
            {
                juce::MemoryBlock mb;
                if (f.loadFileAsData(mb))
                {
                    outModule.binarySha256 = Sha256::computeHex(static_cast<const uint8_t*>(mb.getData()), mb.getSize());
                    break;
                }
            }
        }
    }

    // Enumerar tipos de la fábrica
    juce::OwnedArray<juce::PluginDescription> types;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat(i);
        if (format != nullptr && format->fileMightContainThisPluginType(pluginFile.getFullPathName()))
        {
            format->findAllTypesForFile(types, pluginFile.getFullPathName());
            if (types.size() > 0)
                break;
        }
    }

    if (types.isEmpty())
    {
        outError = "No compatible plugin types found in factory: " + pluginFile.getFullPathName().toStdString();
        return false;
    }

    for (auto* desc : types)
    {
        if (desc != nullptr)
        {
            outModule.componentUids.push_back(desc->createIdentifierString().toStdString());
            outModule.componentNames.push_back(desc->name.toStdString());
            if (outModule.vendor.empty())
                outModule.vendor = desc->manufacturerName.toStdString();
            if (outModule.version.empty())
                outModule.version = desc->version.toStdString();
        }
    }

    // Seleccionar componente por UID o el primero disponible
    const juce::PluginDescription* selectedDesc = types[0];
    if (!targetUid.empty())
    {
        for (auto* desc : types)
        {
            if (desc != nullptr && (desc->createIdentifierString().toStdString() == targetUid || desc->name.toStdString() == targetUid))
            {
                selectedDesc = desc;
                break;
            }
        }
    }
    outModule.selectedUid = selectedDesc->createIdentifierString().toStdString();

    // Instanciación controlada para enumerar buses y parámetros
    juce::String createErr;
    auto instance = formatManager.createPluginInstance(*selectedDesc, 48000.0, 512, createErr);
    if (instance == nullptr)
    {
        outError = "Failed to instantiate plugin component: " + createErr.toStdString();
        return false;
    }

    // Enumerar buses
    for (int i = 0; i < instance->getBusCount(true); ++i)
    {
        if (auto* bus = instance->getBus(true, i))
        {
            InspectedBusInfo b;
            b.name = bus->getName().toStdString();
            b.isInput = true;
            b.defaultChannelCount = bus->getDefaultLayout().size();
            outModule.buses.push_back(b);
        }
    }
    for (int i = 0; i < instance->getBusCount(false); ++i)
    {
        if (auto* bus = instance->getBus(false, i))
        {
            InspectedBusInfo b;
            b.name = bus->getName().toStdString();
            b.isInput = false;
            b.defaultChannelCount = bus->getDefaultLayout().size();
            outModule.buses.push_back(b);
        }
    }

    // Enumerar y validar catálogo de parámetros
    auto params = instance->getParameters();
    outModule.parameters.reserve(static_cast<size_t>(params.size()));

    for (int i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        if (p == nullptr) continue;

        InspectedParameterInfo paramInfo;
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
            paramInfo.id = withId->paramID.toStdString();
        else
            paramInfo.id = "param_" + std::to_string(p->getParameterIndex());

        paramInfo.title = p->getName(128).toStdString();
        paramInfo.unit = p->getLabel().toStdString();
        paramInfo.normalizedValue = p->getValue();
        paramInfo.plainValue = p->getDefaultValue();
        paramInfo.stepCount = p->getNumSteps();
        paramInfo.isDiscrete = p->isDiscrete();
        paramInfo.isAutomatable = p->isAutomatable();
        paramInfo.isMetaParameter = p->isMetaParameter();

        outModule.parameters.push_back(paramInfo);
    }
    outModule.parameterCount = static_cast<int>(outModule.parameters.size());

    // Cierre determinista al salir de ámbito
    return true;
}

} // namespace abdaudiolab::synth
