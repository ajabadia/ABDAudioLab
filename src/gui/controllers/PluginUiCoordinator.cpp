#include "PluginUiCoordinator.h"
#include "../../core/plugins/PluginHardwareContractAdapter.h"
#include "../session/UiStrings.h"

namespace abdaudiolab::gui {

PluginUiCoordinator::PluginUiCoordinator(core::PluginHostManager& hostManagerRef,
                                         PluginWindowController& windowControllerRef,
                                         audio::LabAudioEngine& audioEngineRef,
                                         core::HardwareManager& hardwareManagerRef,
                                         SessionExecutionCoordinator& sessionCoordinatorRef,
                                         IPluginUiHost& uiHostRef)
    : pluginHostManager(hostManagerRef),
      pluginWindowController(windowControllerRef),
      audioEngine(audioEngineRef),
      hardwareManager(hardwareManagerRef),
      sessionCoordinator(sessionCoordinatorRef),
      host(uiHostRef)
{
    // Single unloading hook registered on hostManager
    pluginHostManager.onPluginUnloading = [this] {
        if (!isDisconnecting)
        {
            teardownCounters.hostUnloadCalls++;
            disconnectActivePlugin(false);
        }
    };

    // Plugin Direct Monitoring Passthrough wiring (active while plugin GUI is open)
    pluginWindowController.onWindowStateChanged = [this](bool isOpen) {
        juce::Logger::writeToLog("[PluginUiCoordinator] Plugin window state changed: " + juce::String(isOpen ? "OPEN (monitoring ON)" : "CLOSED (monitoring OFF)"));
        audioEngine.setPluginMonitoringEnabled(isOpen);
    };

    // Automatically synchronize routing whenever a plugin is loaded/adopted in hostManager
    pluginHostManager.onPluginLoaded = [this](const core::PluginIdentity&) {
        if (pluginHostManager.hasActivePlugin())
        {
            double sr = 44100.0;
            int bs = 512;
            if (auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice())
            {
                sr = device->getCurrentSampleRate();
                bs = device->getCurrentBufferSizeSamples();
            }
            applyActivePluginRoutingAndUi(pluginHostManager.getActivePluginDescription(), sr, bs);
        }
    };
}

PluginUiCoordinator::~PluginUiCoordinator()
{
    pluginWindowController.onWindowStateChanged = nullptr;
    pluginHostManager.onPluginUnloading = nullptr;
    pluginHostManager.onPluginLoaded = nullptr;
    cancelPendingUpdate();
    disconnectActivePlugin(true);
}

void PluginUiCoordinator::setMonitoringEnabled(bool enabled)
{
    audioEngine.setPluginMonitoringEnabled(enabled);
}

bool PluginUiCoordinator::hasActivePlugin() const noexcept
{
    return pluginHostManager.hasActivePlugin();
}

juce::AudioPluginInstance* PluginUiCoordinator::getActivePluginInstance() const noexcept
{
    return pluginHostManager.hasActivePlugin() ? pluginHostManager.getActivePluginInstance() : nullptr;
}

const juce::PluginDescription& PluginUiCoordinator::getActivePluginDescription() const noexcept
{
    return pluginHostManager.getActivePluginDescription();
}

void PluginUiCoordinator::loadPlugin(const juce::PluginDescription& description, std::function<void(bool success)> onLoaded)
{
    uint64_t opId = currentOpId.fetch_add(1) + 1;

    double sr = 44100.0;
    int bs = 512;
    if (auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice())
    {
        sr = device->getCurrentSampleRate();
        bs = device->getCurrentBufferSizeSamples();
    }

    host.updatePluginLoadingState(false, "Cargando plugin: " + description.name);

    pluginHostManager.loadPluginAsync(description, sr, bs,
        [this, description, opId, sr, bs, onLoaded = std::move(onLoaded)](const core::PluginLoadResult& result) mutable {
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                pendingResults.push_back({ opId, result, description, sr, bs, std::move(onLoaded) });
            }
            triggerAsyncUpdate();
        });
}

void PluginUiCoordinator::loadPluginFromFile(const juce::File& file, std::function<void(bool success)> onLoaded)
{
    uint64_t opId = currentOpId.fetch_add(1) + 1;

    double sr = 44100.0;
    int bs = 512;
    if (auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice())
    {
        sr = device->getCurrentSampleRate();
        bs = device->getCurrentBufferSizeSamples();
    }

    host.updatePluginLoadingState(false, "Cargando plugin desde archivo: " + file.getFileName());

    pluginHostManager.loadPluginFromFileAsync(file, sr, bs,
        [this, file, opId, sr, bs, onLoaded = std::move(onLoaded)](const core::PluginLoadResult& result) mutable {
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                pendingResults.push_back({ opId, result, pluginHostManager.getActivePluginDescription(), sr, bs, std::move(onLoaded) });
            }
            triggerAsyncUpdate();
        });
}

void PluginUiCoordinator::flushAsyncUpdates()
{
    handleUpdateNowIfNeeded();
    handleAsyncUpdate();
}

void PluginUiCoordinator::handleAsyncUpdate()
{
    std::vector<PendingLoadResult> batch;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        batch.swap(pendingResults);
    }

    for (auto& item : batch)
    {
        if (item.opId != currentOpId.load())
        {
            // Stale operation discarded
            continue;
        }

        if (!item.result.succeeded || !pluginHostManager.hasActivePlugin())
        {
            juce::String errMsg = item.result.userMessage.empty()
                ? "Plugin incompatible o fallo de carga."
                : juce::String(item.result.userMessage);
            host.showPluginError("Error al cargar plugin", errMsg);
            host.updatePluginLoadingState(false, errMsg);
            if (item.onLoaded) item.onLoaded(false);
            continue;
        }

        applyActivePluginRoutingAndUi(item.description, item.sampleRate, item.blockSize);
        host.updatePluginLoadingState(true, "Plugin cargado correctamente");
        if (item.onLoaded) item.onLoaded(true);
    }
}

void PluginUiCoordinator::applyActivePluginRoutingAndUi(const juce::PluginDescription& desc, double sr, int bs)
{
    // If a contract was registered previously, unregister before replacing
    if (!registeredContractId.empty())
    {
        hardwareManager.getContractRegistry().unregisterContract(registeredContractId);
        registeredContractId.clear();
        teardownCounters.contractUnregisterCalls++;
    }

    activePluginInstance = pluginHostManager.getActivePluginInstance();
    activePluginDescription = desc;

    if (activePluginInstance == nullptr)
        return;

    // 1. Audio Engine & Monitoring
    audioEngine.setActivePluginInstance(activePluginInstance, sr, bs);
    audioEngine.setPluginMonitoringEnabled(true);

    // 2. Session Coordinator
    sessionCoordinator.setTargetPluginInstance(activePluginInstance);

    // 3. Dynamic Hardware Contract
    auto dynContract = core::PluginHardwareContractAdapter::createContractFromPlugin(*activePluginInstance, desc);
    hardwareManager.getContractRegistry().registerContract(dynContract);
    registeredContractId = dynContract.id;

    // 4. Transform domain identity to presentation
    PluginIdentityPresentation presentation;
    presentation.pluginName = desc.name;
    presentation.formatName = desc.pluginFormatName;
    presentation.isInstrument = desc.isInstrument;
    presentation.category = "PLUGIN_VIRTUAL";
    presentation.legalTargetId = "plugin_" + juce::File::createLegalFileName(desc.fileOrIdentifier);
    presentation.titleBadge = desc.name + (desc.isInstrument ? gui::strings::BADGE_INSTRUMENT : gui::strings::BADGE_EFFECT);
    presentation.busDescription = desc.pluginFormatName + " Virtual Bus";
    presentation.modelAssetPath = desc.isInstrument ? "models/generic-digital-keyboard.png" : "models/generic-audio-rack.png";

    host.updatePluginIdentity(presentation, desc);
}

void PluginUiCoordinator::showEditor()
{
    if (pluginHostManager.hasActivePlugin())
    {
        pluginWindowController.showPluginWindow(pluginHostManager);
    }
}

void PluginUiCoordinator::closeEditor()
{
    pluginWindowController.closePluginWindow();
}

void PluginUiCoordinator::unloadPlugin()
{
    if (isDisconnecting)
        return;

    // Idempotent guard: if nothing is active, return immediately
    if (!pluginHostManager.hasActivePlugin() && activePluginInstance == nullptr && registeredContractId.empty())
        return;

    disconnectActivePlugin(true);
}

void PluginUiCoordinator::disconnectActivePlugin(bool unloadBackend)
{
    isDisconnecting = true;

    // 1. Close Editor
    pluginWindowController.closePluginWindow();
    teardownCounters.closeEditorCalls++;

    // 2. Disconnect Audio
    audioEngine.setPluginMonitoringEnabled(false);
    audioEngine.setActivePluginInstance(nullptr);
    teardownCounters.audioDisconnectCalls++;

    // 3. Clear Session Target
    sessionCoordinator.setTargetPluginInstance(nullptr);
    teardownCounters.sessionTargetClearCalls++;

    // 4. Unregister Hardware Contract
    if (!registeredContractId.empty())
    {
        hardwareManager.getContractRegistry().unregisterContract(registeredContractId);
        registeredContractId.clear();
        teardownCounters.contractUnregisterCalls++;
    }

    // 5. Reset local observer
    activePluginInstance = nullptr;
    activePluginDescription = {};

    // 6. Unload in backend if still loaded and requested
    if (unloadBackend && pluginHostManager.hasActivePlugin())
    {
        teardownCounters.hostUnloadCalls++;
        pluginHostManager.unloadPlugin();
    }

    // 7. Notify UI
    host.notifyPluginUnloaded();
    teardownCounters.notifyUnloadedCalls++;

    isDisconnecting = false;
}

} // namespace abdaudiolab::gui
