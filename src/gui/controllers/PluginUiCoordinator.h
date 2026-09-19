#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <atomic>
#include <functional>
#include <string>

#include "IPluginUiHost.h"
#include "../../core/plugins/PluginHostManager.h"
#include "../plugins/PluginWindowController.h"
#include "../../audio/LabAudioEngine.h"
#include "../../core/HardwareManager.h"
#include "../SessionExecutionCoordinator.h"

namespace abdaudiolab::gui {

/**
 * @struct PluginOperationToken
 * @brief Identifies asynchronous plugin load operations to discard stale callbacks.
 */
struct PluginOperationToken
{
    std::uint64_t operationId { 0 };
};

/**
 * @class PluginUiCoordinator
 * @brief Autonomous non-owning UI coordinator for virtual audio plugins (VST3).
 *
 * Enforces:
 * - Single authority for active plugin lifecycle in PluginHostManager.
 * - Single idempotent unload and disconnect sequence.
 * - Operation token protection against stale async load callbacks.
 * - Dynamic hardware contract registration and symmetric deregistration.
 */
class PluginUiCoordinator : private juce::AsyncUpdater
{
public:
    PluginUiCoordinator(core::PluginHostManager& hostManagerRef,
                        PluginWindowController& windowControllerRef,
                        audio::LabAudioEngine& audioEngineRef,
                        core::HardwareManager& hardwareManagerRef,
                        SessionExecutionCoordinator& sessionCoordinatorRef,
                        IPluginUiHost& uiHostRef);
    ~PluginUiCoordinator();

    PluginUiCoordinator(const PluginUiCoordinator&) = delete;
    PluginUiCoordinator& operator=(const PluginUiCoordinator&) = delete;

    /**
     * @brief Asynchronously loads and activates a plugin from a KnownPluginDescription.
     */
    void loadPlugin(const juce::PluginDescription& description, std::function<void(bool success)> onLoaded = nullptr);

    /**
     * @brief Asynchronously loads and activates a plugin from a file (.vst3).
     */
    void loadPluginFromFile(const juce::File& file, std::function<void(bool success)> onLoaded = nullptr);

    /**
     * @brief Shows the plugin GUI editor window if the active plugin provides one.
     */
    void showEditor();

    /**
     * @brief Closes the plugin GUI editor window if open.
     */
    void closeEditor();

    /**
     * @brief Controls whether audio input/MIDI directly monitors through the active plugin.
     */
    void setMonitoringEnabled(bool enabled);

    /**
     * @brief Symmetrically and idempotently closes editor, disconnects audio/session/contract, and unloads plugin.
     */
    void unloadPlugin();

    /**
     * @brief Synchronously flushes any pending async updates on the current thread.
     */
    void flushAsyncUpdates();

    /**
     * @struct TeardownCounters
     * @brief Tracks teardown sub-operation counts to guarantee strict idempotency.
     */
    struct TeardownCounters
    {
        int closeEditorCalls { 0 };
        int audioDisconnectCalls { 0 };
        int sessionTargetClearCalls { 0 };
        int contractUnregisterCalls { 0 };
        int hostUnloadCalls { 0 };
        int notifyUnloadedCalls { 0 };
    };

    [[nodiscard]] const TeardownCounters& getTeardownCounters() const noexcept { return teardownCounters; }
    void resetTeardownCounters() noexcept { teardownCounters = {}; }

    /**
     * @brief Returns true if an audio plugin is actively hosted.
     */
    [[nodiscard]] bool hasActivePlugin() const noexcept;

    /**
     * @brief Returns non-owning observer pointer to the active AudioPluginInstance or nullptr.
     */
    [[nodiscard]] juce::AudioPluginInstance* getActivePluginInstance() const noexcept;

    /**
     * @brief Returns description of the active plugin.
     */
    [[nodiscard]] const juce::PluginDescription& getActivePluginDescription() const noexcept;

private:
    void handleAsyncUpdate() override;
    void applyActivePluginRoutingAndUi(const juce::PluginDescription& desc, double sr, int bs);
    void disconnectActivePlugin(bool unloadBackend = true);

    struct PendingLoadResult {
        std::uint64_t opId { 0 };
        core::PluginLoadResult result;
        juce::PluginDescription description;
        double sampleRate { 44100.0 };
        int blockSize { 512 };
        std::function<void(bool)> onLoaded;
    };

    core::PluginHostManager& pluginHostManager;
    PluginWindowController& pluginWindowController;
    audio::LabAudioEngine& audioEngine;
    core::HardwareManager& hardwareManager;
    SessionExecutionCoordinator& sessionCoordinator;
    IPluginUiHost& host;

    std::atomic<std::uint64_t> currentOpId { 0 };
    juce::AudioPluginInstance* activePluginInstance { nullptr };
    juce::PluginDescription activePluginDescription;
    std::string registeredContractId;
    bool isDisconnecting { false };
    TeardownCounters teardownCounters;

    std::mutex queueMutex;
    std::vector<PendingLoadResult> pendingResults;
};

} // namespace abdaudiolab::gui
