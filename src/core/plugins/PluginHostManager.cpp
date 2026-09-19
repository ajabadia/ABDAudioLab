#include "PluginHostManager.h"
#include <algorithm>

namespace abdaudiolab::core
{

PluginHostManager::PluginHostManager()
{
    formatManager.addDefaultFormats();
    juce::Logger::writeToLog("[PluginHost] Initialized. Formats registered: " + juce::String(formatManager.getNumFormats()));
    for (auto* fmt : formatManager.getFormats())
    {
        if (fmt != nullptr)
            juce::Logger::writeToLog("[PluginHost]   Format: " + fmt->getName());
    }
}

PluginHostManager::~PluginHostManager()
{
    juce::Logger::writeToLog("[PluginHost] Destructor called. Unloading active plugin if hosted.");
    unloadPlugin();
}

juce::File PluginHostManager::getDefaultCacheFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDAudioLab")
        .getChildFile("PluginCache.xml");
}

void PluginHostManager::loadCache(const juce::File& cacheFile)
{
    juce::Logger::writeToLog("[PluginHost] Loading cache from: " + cacheFile.getFullPathName());
    if (!cacheFile.existsAsFile())
    {
        juce::Logger::writeToLog("[PluginHost] Cache file does not exist yet.");
        return;
    }

    if (auto xml = juce::parseXML(cacheFile))
    {
        knownPlugins.recreateFromXml(*xml);
        juce::Logger::writeToLog("[PluginHost] Cache loaded successfully. Known plugins: "
            + juce::String(knownPlugins.getNumTypes())
            + " (blacklisted: " + juce::String(knownPlugins.getBlacklistedFiles().size()) + ")");
    }
    else
    {
        juce::Logger::writeToLog("[PluginHost WARNING] Failed to parse cache XML from: " + cacheFile.getFullPathName());
    }
}

void PluginHostManager::saveCache(const juce::File& cacheFile)
{
    juce::Logger::writeToLog("[PluginHost] Saving cache (" + juce::String(knownPlugins.getNumTypes())
        + " plugins) to: " + cacheFile.getFullPathName());
    cacheFile.getParentDirectory().createDirectory();
    if (auto xml = knownPlugins.createXml())
    {
        if (xml->writeTo(cacheFile))
            juce::Logger::writeToLog("[PluginHost] Cache saved successfully.");
        else
            juce::Logger::writeToLog("[PluginHost ERROR] Failed to write cache XML to: " + cacheFile.getFullPathName());
    }
    else
    {
        juce::Logger::writeToLog("[PluginHost ERROR] Failed to create XML from KnownPluginList.");
    }
}

void PluginHostManager::scanPlugins(const juce::FileSearchPath& searchPath,
                                    bool recursive,
                                    std::function<void(const juce::String& currentPlugin, float progress0to1)> progressCallback)
{
    juce::Logger::writeToLog("[PluginHost] scanPlugins started. Recursive: " + juce::String(recursive ? "true" : "false")
        + ", SearchPaths: " + searchPath.toString());

    int scannedCount = 0;
    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr) continue;

        juce::Logger::writeToLog("[PluginHost] Scanning format: " + format->getName());

        juce::PluginDirectoryScanner scanner(knownPlugins,
                                             *format,
                                             searchPath,
                                             recursive,
                                             juce::File());

        juce::String pluginBeingScanned;
        while (true)
        {
            juce::Logger::writeToLog("[PluginHost] Scanner requesting next file for format: " + format->getName());
            const bool hasMore = scanner.scanNextFile(true, pluginBeingScanned);

            if (pluginBeingScanned.isNotEmpty())
            {
                juce::Logger::writeToLog("[PluginHost] Scanned plugin file: " + pluginBeingScanned
                    + " [Progress: " + juce::String(juce::roundToInt(scanner.getProgress() * 100.0f)) + "%]");
                scannedCount++;
            }

            if (progressCallback != nullptr && pluginBeingScanned.isNotEmpty())
                progressCallback(pluginBeingScanned, scanner.getProgress());

            if (!hasMore)
            {
                juce::Logger::writeToLog("[PluginHost] Finished scanning format: " + format->getName());
                break;
            }
        }
    }

    juce::Logger::writeToLog("[PluginHost] scanPlugins complete. Scanned entries: " + juce::String(scannedCount)
        + ", Total known plugins: " + juce::String(knownPlugins.getNumTypes())
        + ", Blacklisted: " + juce::String(knownPlugins.getBlacklistedFiles().size()));
}

std::vector<juce::PluginDescription> PluginHostManager::getAvailablePlugins() const
{
    std::vector<juce::PluginDescription> list;
    for (const auto& desc : knownPlugins.getTypes())
    {
        list.push_back(desc);
    }
    return list;
}

std::string PluginHostManager::calculateFileSha256(const juce::File& file)
{
    if (file.isDirectory())
    {
        juce::Array<juce::File> files;
        file.findChildFiles(files, juce::File::findFiles, true);
        std::vector<std::pair<std::string, std::string>> entries;

        for (const auto& f : files)
        {
            juce::MemoryBlock mb;
            if (f.loadFileAsData(mb))
            {
                std::string relPath = f.getRelativePathFrom(file).toStdString();
                std::string hash = synth::Sha256::computeHex(mb.getData(), mb.getSize());
                entries.emplace_back(relPath, hash);
            }
        }
        std::sort(entries.begin(), entries.end());
        std::string manifest;
        for (const auto& [path, h] : entries)
            manifest += path + ":" + h + "\n";
        return synth::Sha256::computeHex(manifest);
    }
    else if (file.existsAsFile())
    {
        juce::MemoryBlock mb;
        if (file.loadFileAsData(mb))
            return synth::Sha256::computeHex(mb.getData(), mb.getSize());
    }
    return {};
}

std::string PluginHostManager::calculatePluginSha256(const juce::PluginDescription& desc)
{
    juce::File f(desc.fileOrIdentifier);
    if (f.exists())
    {
        auto fileHash = calculateFileSha256(f);
        if (!fileHash.empty())
            return fileHash;
    }
    std::string fallbackBlob = desc.name.toStdString() + "|"
                             + desc.pluginFormatName.toStdString() + "|"
                             + desc.manufacturerName.toStdString() + "|"
                             + desc.fileOrIdentifier.toStdString();
    return synth::Sha256::computeHex(fallbackBlob);
}

PluginLoadResult PluginHostManager::activateInstanceInternal(std::unique_ptr<juce::AudioPluginInstance> instance,
                                                            const juce::PluginDescription& desc,
                                                            double sampleRate,
                                                            int blockSize)
{
    if (instance == nullptr)
    {
        juce::Logger::writeToLog("[PluginHost ERROR] activateInstanceInternal received nullptr instance.");
        return { false, {}, "NULL_INSTANCE", "Plugin instance pointer is null." };
    }

    // Safe unload of any previous plugin instance first
    unloadPlugin();

    std::lock_guard<std::mutex> lock(audioMutex);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentBlockSize = blockSize > 0 ? blockSize : 512;

    // Bus arrangement configuration
    int numIns = instance->getTotalNumInputChannels();
    int numOuts = instance->getTotalNumOutputChannels();
    if (numOuts == 0)
    {
        instance->setPlayConfigDetails(numIns, 2, currentSampleRate, currentBlockSize);
    }
    else
    {
        instance->setPlayConfigDetails(numIns, numOuts, currentSampleRate, currentBlockSize);
    }

    // Prepare plugin
    instance->prepareToPlay(currentSampleRate, currentBlockSize);
    isProcessingActive = true;

    activeInstance = std::move(instance);
    activeDescription = desc;

    activeIdentity.pluginId = desc.fileOrIdentifier.isNotEmpty() ? desc.fileOrIdentifier.toStdString() : desc.name.toStdString();
    activeIdentity.vendor = desc.manufacturerName.toStdString();
    activeIdentity.name = desc.name.toStdString();
    activeIdentity.version = desc.version.toStdString();
    activeIdentity.componentSha256 = calculatePluginSha256(desc);

    juce::Logger::writeToLog("[PluginHost SUCCESS] Plugin activated: '" + juce::String(activeIdentity.name)
        + "' by '" + juce::String(activeIdentity.vendor)
        + "' (SR: " + juce::String(currentSampleRate)
        + ", BS: " + juce::String(currentBlockSize)
        + ", SHA: " + juce::String(activeIdentity.componentSha256.substr(0, 12)) + "...)");

    if (onPluginLoaded)
        onPluginLoaded(activeIdentity);

    return { true, activeIdentity, "", "Plugin loaded successfully." };
}

PluginLoadResult PluginHostManager::loadPlugin(const juce::PluginDescription& desc,
                                              double sampleRate,
                                              int blockSize)
{
    juce::Logger::writeToLog("[PluginHost] loadPlugin synchronously requested: '" + desc.name
        + "' [" + desc.pluginFormatName + "]");

    juce::String errorMsg;
    auto instance = formatManager.createPluginInstance(desc, sampleRate, blockSize, errorMsg);
    if (instance == nullptr || errorMsg.isNotEmpty())
    {
        juce::Logger::writeToLog("[PluginHost ERROR] createPluginInstance failed for '" + desc.name + "': " + errorMsg);
        return { false, {}, "LOAD_FAILED", errorMsg.toStdString() };
    }
    return activateInstanceInternal(std::move(instance), desc, sampleRate, blockSize);
}

PluginLoadResult PluginHostManager::loadPluginFromFile(const juce::File& file,
                                                      double sampleRate,
                                                      int blockSize)
{
    juce::Logger::writeToLog("[PluginHost] loadPluginFromFile synchronously requested: " + file.getFullPathName());

    if (!file.exists())
    {
        juce::Logger::writeToLog("[PluginHost ERROR] File does not exist: " + file.getFullPathName());
        return { false, {}, "FILE_NOT_FOUND", "Plugin file does not exist: " + file.getFullPathName().toStdString() };
    }

    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr) continue;
        if (format->fileMightContainThisPluginType(file.getFullPathName()))
        {
            juce::OwnedArray<juce::PluginDescription> typesFound;
            format->findAllTypesForFile(typesFound, file.getFullPathName());
            if (typesFound.size() > 0)
            {
                auto* desc = typesFound[0];
                knownPlugins.addType(*desc);
                return loadPlugin(*desc, sampleRate, blockSize);
            }
        }
    }
    return { false, {}, "INCOMPATIBLE_PLUGIN", "No compatible audio plugin format recognized for file: " + file.getFileName().toStdString() };
}

PluginLoadResult PluginHostManager::adoptPluginInstance(std::unique_ptr<juce::AudioPluginInstance> instance,
                                                       const juce::PluginDescription& desc,
                                                       double sampleRate,
                                                       int blockSize)
{
    juce::Logger::writeToLog("[PluginHost] adoptPluginInstance requested for: '" + desc.name + "'");
    return activateInstanceInternal(std::move(instance), desc, sampleRate, blockSize);
}

void PluginHostManager::loadPluginAsync(const juce::PluginDescription& desc,
                                       double sampleRate,
                                       int blockSize,
                                       std::function<void(const PluginLoadResult&)> callback)
{
    juce::Logger::writeToLog("[PluginHost] loadPluginAsync requested: '" + desc.name + "'");

    bool formatFound = false;
    for (auto* format : formatManager.getFormats())
    {
        if (format != nullptr && (format->getName() == desc.pluginFormatName
            || format->fileMightContainThisPluginType(desc.fileOrIdentifier)))
        {
            formatFound = true;
            break;
        }
    }

    if (!formatFound)
    {
        if (callback)
            callback({ false, {}, "INCOMPATIBLE_PLUGIN", "No compatible audio plugin format recognized for: " + desc.name.toStdString() });
        return;
    }

    formatManager.createPluginInstanceAsync(desc, sampleRate, blockSize,
        [this, desc, sampleRate, blockSize, cb = std::move(callback)](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
            PluginLoadResult res;
            if (error.isNotEmpty() || instance == nullptr)
            {
                res = { false, {}, "ASYNC_LOAD_FAILED", error.toStdString() };
            }
            else
            {
                res = activateInstanceInternal(std::move(instance), desc, sampleRate, blockSize);
            }
            if (cb != nullptr)
                cb(res);
        });
}

void PluginHostManager::loadPluginFromFileAsync(const juce::File& file,
                                               double sampleRate,
                                               int blockSize,
                                               std::function<void(const PluginLoadResult&)> callback)
{
    juce::Logger::writeToLog("[PluginHost] loadPluginFromFileAsync (Seam 4) requested: " + file.getFullPathName());

    if (!file.exists())
    {
        if (callback)
            callback({ false, {}, "FILE_NOT_FOUND", "File does not exist: " + file.getFullPathName().toStdString() });
        return;
    }

    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr) continue;
        if (format->fileMightContainThisPluginType(file.getFullPathName()))
        {
            juce::OwnedArray<juce::PluginDescription> typesFound;
            format->findAllTypesForFile(typesFound, file.getFullPathName());
            if (typesFound.size() > 0)
            {
                auto* desc = typesFound[0];
                knownPlugins.addType(*desc);
                loadPluginAsync(*desc, sampleRate, blockSize, std::move(callback));
                return;
            }
        }
    }

    if (callback)
        callback({ false, {}, "INCOMPATIBLE_PLUGIN", "No compatible format recognized for: " + file.getFileName().toStdString() });
}

void PluginHostManager::instantiatePluginAsync(const juce::PluginDescription& desc,
                                              double sampleRate,
                                              int blockSize,
                                              std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback)
{
    formatManager.createPluginInstanceAsync(desc, sampleRate, blockSize, std::move(callback));
}

void PluginHostManager::loadPluginFromFileAsync(const juce::File& file,
                                                double sampleRate,
                                                int blockSize,
                                                std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback)
{
    if (!file.exists())
    {
        if (callback != nullptr)
            callback(nullptr, "File does not exist: " + file.getFullPathName());
        return;
    }

    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr) continue;
        if (format->fileMightContainThisPluginType(file.getFullPathName()))
        {
            juce::OwnedArray<juce::PluginDescription> typesFound;
            format->findAllTypesForFile(typesFound, file.getFullPathName());
            if (typesFound.size() > 0)
            {
                auto* desc = typesFound[0];
                knownPlugins.addType(*desc);
                instantiatePluginAsync(*desc, sampleRate, blockSize, std::move(callback));
                return;
            }
        }
    }

    juce::String errMsg = "No compatible audio plugin format recognized for file: " + file.getFileName();
    if (callback != nullptr)
        callback(nullptr, errMsg);
}

void PluginHostManager::unloadPlugin()
{
    if (activeInstance == nullptr)
        return;

    juce::Logger::writeToLog("[PluginHost] Unloading active plugin: " + juce::String(activeIdentity.name));

    // Order: stop MIDI -> deactivate -> disconnect callbacks -> close editor -> release controller -> release component

    // 1. Stop MIDI immediately
    allNotesOff();

    // 2. Disconnect callbacks & notify listeners (e.g. close UI window) before instance destruction
    if (onPluginUnloading)
    {
        try
        {
            onPluginUnloading();
        }
        catch (...)
        {
            juce::Logger::writeToLog("[PluginHost WARNING] Exception during onPluginUnloading callback.");
        }
    }

    // 3. Deactivate audio processing
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        isProcessingActive = false;
        if (activeInstance != nullptr)
            activeInstance->releaseResources();
    }

    // 4. Release active instance (releases controller and component)
    activeInstance.reset();
    activeIdentity = {};
    activeDescription = {};
    pendingMidiMessages.clear();

    juce::Logger::writeToLog("[PluginHost] Plugin unloaded successfully.");
}

bool PluginHostManager::hasActivePlugin() const noexcept
{
    return activeInstance != nullptr;
}

void PluginHostManager::prepare(double sampleRate, int blockSize)
{
    std::lock_guard<std::mutex> lock(audioMutex);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : currentSampleRate;
    currentBlockSize = blockSize > 0 ? blockSize : currentBlockSize;

    if (activeInstance != nullptr)
    {
        activeInstance->releaseResources();
        activeInstance->prepareToPlay(currentSampleRate, currentBlockSize);
        isProcessingActive = true;
    }
}

void PluginHostManager::processAudioBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    std::unique_lock<std::mutex> lock(audioMutex, std::try_to_lock);
    if (!lock.owns_lock() || activeInstance == nullptr || !isProcessingActive)
    {
        return;
    }

    if (!pendingMidiMessages.isEmpty())
    {
        midiMessages.addEvents(pendingMidiMessages, 0, buffer.getNumSamples(), 0);
        pendingMidiMessages.clear();
    }

    activeInstance->processBlock(buffer, midiMessages);
}

void PluginHostManager::sendMidiMessage(const juce::MidiMessage& message)
{
    std::lock_guard<std::mutex> lock(audioMutex);
    pendingMidiMessages.addEvent(message, 0);
}

void PluginHostManager::allNotesOff()
{
    std::lock_guard<std::mutex> lock(audioMutex);
    for (int ch = 1; ch <= 16; ++ch)
    {
        pendingMidiMessages.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        pendingMidiMessages.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
        pendingMidiMessages.addEvent(juce::MidiMessage::controllerEvent(ch, 120, 0), 0);
        pendingMidiMessages.addEvent(juce::MidiMessage::controllerEvent(ch, 123, 0), 0);
    }
}

bool PluginHostManager::getStateInformation(juce::MemoryBlock& destData) const
{
    if (activeInstance == nullptr)
        return false;
    activeInstance->getStateInformation(destData);
    return destData.getSize() > 0;
}

bool PluginHostManager::setStateInformation(const void* data, int sizeInBytes)
{
    if (activeInstance == nullptr || data == nullptr || sizeInBytes <= 0)
        return false;
    activeInstance->setStateInformation(data, sizeInBytes);
    return true;
}

int PluginHostManager::getParameterCount() const
{
    return activeInstance != nullptr ? static_cast<int>(activeInstance->getParameters().size()) : 0;
}

float PluginHostManager::getParameterNormalized(int index) const
{
    if (activeInstance == nullptr) return 0.0f;
    const auto& params = activeInstance->getParameters();
    if (index >= 0 && index < params.size() && params[index] != nullptr)
        return params[index]->getValue();
    return 0.0f;
}

void PluginHostManager::setParameterNormalized(int index, float normalizedValue)
{
    if (activeInstance == nullptr) return;
    const auto& params = activeInstance->getParameters();
    if (index >= 0 && index < params.size() && params[index] != nullptr)
        params[index]->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
}

std::string PluginHostManager::getParameterName(int index) const
{
    if (activeInstance == nullptr) return {};
    const auto& params = activeInstance->getParameters();
    if (index >= 0 && index < params.size() && params[index] != nullptr)
        return params[index]->getName(64).toStdString();
    return {};
}

PluginEditorResult PluginHostManager::inspectEditor() const
{
    if (activeInstance == nullptr)
        return { false, "", "NO_ACTIVE_PLUGIN" };
    if (!activeInstance->hasEditor())
        return { false, "", "NO_EDITOR_SUPPORTED" };
    return { true, activeIdentity.name + "_Editor", "" };
}

std::unique_ptr<juce::AudioProcessorEditor> PluginHostManager::createEditorIfNeeded()
{
    if (activeInstance == nullptr || !activeInstance->hasEditor())
        return nullptr;
    auto* ed = activeInstance->createEditorIfNeeded();
    return std::unique_ptr<juce::AudioProcessorEditor>(ed);
}

} // namespace abdaudiolab::core

