#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "synth/Sha256.h"
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <mutex>

namespace abdaudiolab::core
{

/**
 * @struct PluginIdentity
 * @brief Domain identity and fixity contract for an audio plugin.
 */
struct PluginIdentity
{
    std::string pluginId;
    std::string vendor;
    std::string name;
    std::string version;
    std::string componentSha256;

    bool operator==(const PluginIdentity& other) const noexcept
    {
        return pluginId == other.pluginId &&
               vendor == other.vendor &&
               name == other.name &&
               version == other.version &&
               componentSha256 == other.componentSha256;
    }

    bool operator!=(const PluginIdentity& other) const noexcept
    {
        return !(*this == other);
    }
};

/**
 * @struct PluginLoadResult
 * @brief Structured outcome of a plugin loading operation.
 */
struct PluginLoadResult
{
    bool succeeded { false };
    PluginIdentity identity;
    std::string errorCode;
    std::string userMessage;
};

/**
 * @struct PluginEditorResult
 * @brief Outcome of inspecting plugin GUI editor capabilities.
 */
struct PluginEditorResult
{
    bool available { false };
    std::string viewId;
    std::string errorCode;
};

/**
 * @class PluginHostManager
 * @brief Manages scanning, instantiating, hosting, and lifecycle of virtual audio plugins (VST3, AU, LV2, ARA).
 *        Sole owner of the active plugin instance, enforcing strict deterministic lifecycle:
 *        Load:   discover -> instantiate component -> instantiate controller -> initialize -> bus arrangement -> prepare SR/BS -> activate -> connect audio/MIDI
 *        Unload: stop MIDI -> deactivate -> disconnect callbacks -> close editor -> release controller -> release component
 */
class PluginHostManager
{
public:
    PluginHostManager();
    ~PluginHostManager();

    /**
     * @brief Formats supported by this build.
     */
    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }
    const juce::AudioPluginFormatManager& getFormatManager() const noexcept { return formatManager; }

    /**
     * @brief Known plugins catalog.
     */
    juce::KnownPluginList& getKnownPluginList() noexcept { return knownPlugins; }
    const juce::KnownPluginList& getKnownPluginList() const noexcept { return knownPlugins; }

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
     * @brief Returns a list of all currently available plugin descriptions.
     */
    std::vector<juce::PluginDescription> getAvailablePlugins() const;

    /**
     * @brief Computes stable SHA-256 fixity hash for a plugin file or bundle.
     */
    static std::string calculateFileSha256(const juce::File& file);

    /**
     * @brief Computes stable SHA-256 fixity hash for a plugin description.
     */
    static std::string calculatePluginSha256(const juce::PluginDescription& desc);

    // =========================================================================
    // LIFECYCLE MANAGEMENT (SEAM 4)
    // =========================================================================

    /**
     * @brief Synchronously loads and activates a plugin from a known description.
     */
    PluginLoadResult loadPlugin(const juce::PluginDescription& desc,
                                double sampleRate = 44100.0,
                                int blockSize = 512);

    /**
     * @brief Synchronously loads and activates a plugin from a file path (.vst3).
     */
    PluginLoadResult loadPluginFromFile(const juce::File& file,
                                        double sampleRate = 44100.0,
                                        int blockSize = 512);

    /**
     * @brief Adopts an existing AudioPluginInstance (used by tests, mocks, or custom injectors).
     */
    PluginLoadResult adoptPluginInstance(std::unique_ptr<juce::AudioPluginInstance> instance,
                                         const juce::PluginDescription& desc,
                                         double sampleRate = 44100.0,
                                         int blockSize = 512);

    /**
     * @brief Asynchronously loads a plugin from description, reporting structured PluginLoadResult.
     */
    void loadPluginAsync(const juce::PluginDescription& desc,
                         double sampleRate,
                         int blockSize,
                         std::function<void(const PluginLoadResult&)> callback);

    /**
     * @brief Asynchronously loads a plugin from file, reporting structured PluginLoadResult.
     */
    void loadPluginFromFileAsync(const juce::File& file,
                                 double sampleRate,
                                 int blockSize,
                                 std::function<void(const PluginLoadResult&)> callback);

    /**
     * @brief Legacy asynchronous loader for backward compatibility.
     */
    void instantiatePluginAsync(const juce::PluginDescription& desc,
                                double sampleRate,
                                int blockSize,
                                std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback);

    /**
     * @brief Legacy asynchronous loader from file for backward compatibility.
     */
    void loadPluginFromFileAsync(const juce::File& file,
                                 double sampleRate,
                                 int blockSize,
                                 std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)> callback);

    /**
     * @brief Safe shutdown and release of the active plugin.
     *        Sequence: stop MIDI -> deactivate audio -> notify callbacks -> release controller & component.
     */
    void unloadPlugin();

    // =========================================================================
    // ACTIVE PLUGIN STATE & OBSERVER ACCESS
    // =========================================================================

    /**
     * @brief Returns true if an audio plugin is actively hosted.
     */
    [[nodiscard]] bool hasActivePlugin() const noexcept;

    /**
     * @brief Returns domain identity of the active plugin.
     */
    [[nodiscard]] const PluginIdentity& getActivePluginIdentity() const noexcept { return activeIdentity; }

    /**
     * @brief Returns JUCE description of the active plugin.
     */
    [[nodiscard]] const juce::PluginDescription& getActivePluginDescription() const noexcept { return activeDescription; }

    /**
     * @brief Observer pointer to the active plugin instance.
     *        LIFETIME WARNING: The returned pointer is owned exclusively by PluginHostManager
     *        and becomes invalid upon unloadPlugin() or new plugin load.
     */
    [[nodiscard]] juce::AudioPluginInstance* getActivePluginInstance() noexcept { return activeInstance.get(); }
    [[nodiscard]] const juce::AudioPluginInstance* getActivePluginInstance() const noexcept { return activeInstance.get(); }

    [[nodiscard]] double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    [[nodiscard]] int getCurrentBlockSize() const noexcept { return currentBlockSize; }

    // =========================================================================
    // AUDIO & MIDI PROCESSING
    // =========================================================================

    /**
     * @brief Prepares active plugin for playback with the specified sample rate and block size.
     */
    void prepare(double sampleRate, int blockSize);

    /**
     * @brief Processes an audio block and dispatches pending/incoming MIDI messages.
     */
    void processAudioBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    /**
     * @brief Enqueues a live MIDI message to be delivered during the next processBlock call.
     */
    void sendMidiMessage(const juce::MidiMessage& message);

    /**
     * @brief Sends All Notes Off and All Sound Off messages to stop active sound generation immediately.
     */
    void allNotesOff();

    // =========================================================================
    // STATE & PRESETS
    // =========================================================================

    /**
     * @brief Captures binary chunk state from the active plugin.
     */
    bool getStateInformation(juce::MemoryBlock& destData) const;

    /**
     * @brief Restores binary chunk state into the active plugin.
     */
    bool setStateInformation(const void* data, int sizeInBytes);

    // =========================================================================
    // PARAMETER INTROSPECTION
    // =========================================================================

    [[nodiscard]] int getParameterCount() const;
    [[nodiscard]] float getParameterNormalized(int index) const;
    void setParameterNormalized(int index, float normalizedValue);
    [[nodiscard]] std::string getParameterName(int index) const;

    // =========================================================================
    // EDITOR INTROSPECTION & FACTORY
    // =========================================================================

    /**
     * @brief Checks if the active plugin provides a custom graphical editor.
     */
    [[nodiscard]] PluginEditorResult inspectEditor() const;

    /**
     * @brief Creates a GUI editor instance if available. Callers take ownership.
     */
    std::unique_ptr<juce::AudioProcessorEditor> createEditorIfNeeded();

    // =========================================================================
    // HOOKS & NOTIFICATIONS
    // =========================================================================

    /**
     * @brief Invoked immediately before active plugin is deactivated and destroyed.
     *        Consumers (e.g. PluginWindowController) must close/disconnect here.
     */
    std::function<void()> onPluginUnloading;

    /**
     * @brief Invoked after a plugin has been successfully loaded and activated.
     */
    std::function<void(const PluginIdentity&)> onPluginLoaded;

private:
    PluginLoadResult activateInstanceInternal(std::unique_ptr<juce::AudioPluginInstance> instance,
                                              const juce::PluginDescription& desc,
                                              double sampleRate,
                                              int blockSize);

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    std::unique_ptr<juce::AudioPluginInstance> activeInstance;
    PluginIdentity activeIdentity;
    juce::PluginDescription activeDescription;

    double currentSampleRate { 44100.0 };
    int currentBlockSize { 512 };
    bool isProcessingActive { false };

    juce::MidiBuffer pendingMidiMessages;
    mutable std::mutex audioMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHostManager)
};

} // namespace abdaudiolab::core
