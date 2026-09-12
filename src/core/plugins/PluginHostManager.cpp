#include "PluginHostManager.h"

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

void PluginHostManager::instantiatePluginAsync(const juce::PluginDescription& desc,
                                              double sampleRate,
                                              int blockSize,
                                              std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback)
{
    juce::Logger::writeToLog("[PluginHost] instantiatePluginAsync requested: '" + desc.name
        + "' [" + desc.pluginFormatName + "] UID: " + desc.fileOrIdentifier
        + ", SR: " + juce::String(sampleRate) + ", BS: " + juce::String(blockSize));

    formatManager.createPluginInstanceAsync(desc,
                                            sampleRate,
                                            blockSize,
                                            [cb = std::move(callback), pluginName = desc.name](std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                                               const juce::String& errorMsg) {
                                                if (errorMsg.isNotEmpty())
                                                {
                                                    juce::Logger::writeToLog("[PluginHost ERROR] createPluginInstanceAsync error for '"
                                                        + pluginName + "': " + errorMsg);
                                                }
                                                else if (instance == nullptr)
                                                {
                                                    juce::Logger::writeToLog("[PluginHost ERROR] createPluginInstanceAsync returned nullptr instance for '"
                                                        + pluginName + "' without explicit error message.");
                                                }
                                                else
                                                {
                                                    juce::Logger::writeToLog("[PluginHost SUCCESS] Plugin instantiated: '" + instance->getName()
                                                        + "' (Inputs: " + juce::String(instance->getTotalNumInputChannels())
                                                        + ", Outputs: " + juce::String(instance->getTotalNumOutputChannels())
                                                        + ", Params: " + juce::String(instance->getParameters().size())
                                                        + ", hasEditor: " + juce::String(instance->hasEditor() ? "YES" : "NO")
                                                        + ", acceptsMidi: " + juce::String(instance->acceptsMidi() ? "YES" : "NO") + ")");
                                                }

                                                if (cb != nullptr)
                                                    cb(std::move(instance), errorMsg);
                                            });
}

void PluginHostManager::loadPluginFromFileAsync(const juce::File& file,
                                                double sampleRate,
                                                int blockSize,
                                                std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback)
{
    juce::Logger::writeToLog("[PluginHost] loadPluginFromFileAsync: " + file.getFullPathName()
        + " (SR: " + juce::String(sampleRate) + ", BS: " + juce::String(blockSize) + ")");

    if (!file.exists())
    {
        juce::Logger::writeToLog("[PluginHost ERROR] File does not exist: " + file.getFullPathName());
        if (callback != nullptr)
            callback(nullptr, "File does not exist: " + file.getFullPathName());
        return;
    }

    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr) continue;

        const bool mightContain = format->fileMightContainThisPluginType(file.getFullPathName());
        juce::Logger::writeToLog("[PluginHost] Checking format '" + format->getName()
            + "' -> mightContain: " + juce::String(mightContain ? "YES" : "NO"));

        if (mightContain)
        {
            juce::OwnedArray<juce::PluginDescription> typesFound;
            juce::Logger::writeToLog("[PluginHost] Calling format->findAllTypesForFile synchronously for: " + file.getFileName());

            format->findAllTypesForFile(typesFound, file.getFullPathName());

            juce::Logger::writeToLog("[PluginHost] findAllTypesForFile finished. Types found: " + juce::String(typesFound.size()));

            if (typesFound.size() > 0)
            {
                auto* desc = typesFound[0];
                juce::Logger::writeToLog("[PluginHost] Selected type: '" + desc->name
                    + "' (" + desc->pluginFormatName + ") by " + desc->manufacturerName
                    + " [isInstrument: " + juce::String(desc->isInstrument ? "YES" : "NO") + "]");

                knownPlugins.addType(*desc);
                instantiatePluginAsync(*desc, sampleRate, blockSize, std::move(callback));
                return;
            }
            else
            {
                juce::Logger::writeToLog("[PluginHost WARNING] Format '" + format->getName()
                    + "' matched extension, but findAllTypesForFile found 0 descriptions.");
            }
        }
    }

    juce::String errMsg = "No compatible audio plugin format recognized for file: " + file.getFileName();
    juce::Logger::writeToLog("[PluginHost ERROR] " + errMsg);
    if (callback != nullptr)
        callback(nullptr, errMsg);
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

} // namespace abdaudiolab::core
