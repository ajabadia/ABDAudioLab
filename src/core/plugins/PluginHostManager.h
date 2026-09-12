#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <functional>
#include <vector>

namespace abdaudiolab::core
{

/**
 * @class PluginHostManager
 * @brief Manages scanning, instantiating, and hosting virtual audio plugins (VST3, AU, LV2, ARA).
 *        Thread-safe wrapper around juce::AudioPluginFormatManager and juce::KnownPluginList.
 */
class PluginHostManager
{
public:
    PluginHostManager();
    ~PluginHostManager() = default;

    /**
     * @brief Formats supported by this build.
     */
    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

    /**
     * @brief Known plugins catalog.
     */
    juce::KnownPluginList& getKnownPluginList() noexcept { return knownPlugins; }

    /**
     * @brief Loads cached known plugins from XML settings file.
     */
    void loadCache(const juce::File& cacheFile);

    /**
     * @brief Saves current known plugins to XML settings file.
     */
    void saveCache(const juce::File& cacheFile);

    /**
     * @brief Default file location for plugin cache in user application data.
     */
    static juce::File getDefaultCacheFile();

    /**
     * @brief Synchronous or asynchronous plugin scanner for a specific directory or default VST3 paths.
     */
    void scanPlugins(const juce::FileSearchPath& searchPath,
                     bool recursive = true,
                     std::function<void(const juce::String& currentPlugin, float progress0to1)> progressCallback = nullptr);

    /**
     * @brief Loads a plugin instance from a known plugin description asynchronously.
     */
    void instantiatePluginAsync(const juce::PluginDescription& desc,
                                double sampleRate,
                                int blockSize,
                                std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback);

    /**
     * @brief Loads a plugin directly from a file path (.vst3) by inspecting its description.
     */
    void loadPluginFromFileAsync(const juce::File& file,
                                 double sampleRate,
                                 int blockSize,
                                 std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback);

    /**
     * @brief Returns a list of all currently available plugin descriptions.
     */
    std::vector<juce::PluginDescription> getAvailablePlugins() const;

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHostManager)
};

} // namespace abdaudiolab::core
