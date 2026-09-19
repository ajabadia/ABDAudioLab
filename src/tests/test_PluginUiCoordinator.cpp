#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <thread>
#include <chrono>

#include "../gui/controllers/PluginUiCoordinator.h"
#include "../gui/controllers/IPluginUiHost.h"
#include "../core/plugins/PluginHostManager.h"
#include "../gui/plugins/PluginWindowController.h"
#include "../audio/LabAudioEngine.h"
#include "../core/HardwareManager.h"
#include "../core/ProfilingSequencer.h"
#include "../core/SessionManager.h"
#include "../gui/SoundIdCurvePlotter.h"
#include "../hardware/MockHardwareController.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;

namespace {

class DummyEditor : public juce::AudioProcessorEditor
{
public:
    explicit DummyEditor(juce::AudioProcessor& proc)
        : juce::AudioProcessorEditor(proc)
    {
        setSize(300, 200);
    }
};

class DummyPlugin : public juce::AudioPluginInstance
{
public:
    explicit DummyPlugin(bool provideEditor = false, bool isInst = false, bool hasBuses = true)
        : juce::AudioPluginInstance(hasBuses
                                        ? BusesProperties()
                                              .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                                        : BusesProperties()),
          hasEditorFlag(provideEditor),
          isInstFlag(isInst),
          hasBusesFlag(hasBuses)
    {
    }

    const juce::String getName() const override { return "MockTestPlugin"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return isInstFlag; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override
    {
        return hasEditorFlag ? new DummyEditor(*this) : nullptr;
    }
    bool hasEditor() const override { return hasEditorFlag; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    void fillInPluginDescription(juce::PluginDescription& desc) const override
    {
        desc.name = getName();
        desc.pluginFormatName = "VST3";
        desc.fileOrIdentifier = "mock_vst3_uid";
        desc.isInstrument = isInstFlag;
    }

private:
    bool hasEditorFlag { false };
    bool isInstFlag { false };
    bool hasBusesFlag { true };
};

juce::PluginDescription createDummyDesc(const juce::String& name = "MockTestPlugin", bool isInst = false)
{
    juce::PluginDescription d;
    d.name = name;
    d.pluginFormatName = "VST3";
    d.fileOrIdentifier = name.toLowerCase() + "_uid";
    d.isInstrument = isInst;
    return d;
}

class MockPluginUiHost : public IPluginUiHost
{
public:
    int loadingStateCalls { 0 };
    int identityCalls { 0 };
    int errorCalls { 0 };
    int unloadedCalls { 0 };

    bool lastLoadingSuccess { false };
    juce::String lastLoadingMessage;
    PluginIdentityPresentation lastIdentity;
    juce::PluginDescription lastDescription;
    juce::String lastErrorTitle;
    juce::String lastErrorMessage;

    void updatePluginLoadingState(bool isSuccess, const juce::String& message) override
    {
        loadingStateCalls++;
        lastLoadingSuccess = isSuccess;
        lastLoadingMessage = message;
    }

    void updatePluginIdentity(const PluginIdentityPresentation& identity,
                              const juce::PluginDescription& description) override
    {
        identityCalls++;
        lastIdentity = identity;
        lastDescription = description;
    }

    void showPluginError(const juce::String& title, const juce::String& message) override
    {
        errorCalls++;
        lastErrorTitle = title;
        lastErrorMessage = message;
    }

    void notifyPluginUnloaded() override
    {
        unloadedCalls++;
    }

    void reset()
    {
        loadingStateCalls = 0;
        identityCalls = 0;
        errorCalls = 0;
        unloadedCalls = 0;
        lastIdentity = {};
        lastDescription = {};
    }
};

struct CoordinatorFixture
{
    juce::ScopedJuceInitialiser_GUI juceGui;

    core::PluginHostManager hostManager;
    PluginWindowController windowController;
    audio::LabAudioEngine audioEngine;
    core::HardwareManager hardwareManager;
    hardware::MockHardwareController mockHw;
    core::ProfilingSequencer sequencer { audioEngine, mockHw };
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter plotter;
    gui::SessionExecutionCoordinator coordinator { sequencer, sessionManager, plotter };
    MockPluginUiHost mockHost;

    std::unique_ptr<PluginUiCoordinator> uiCoordinator;

    CoordinatorFixture()
    {
        uiCoordinator = std::make_unique<PluginUiCoordinator>(
            hostManager,
            windowController,
            audioEngine,
            hardwareManager,
            coordinator,
            mockHost
        );
    }
};

} // namespace

TEST_CASE("PluginUiCoordinator: Lifecycle, Routing & Identity Contracts", "[PluginUiCoordinator]")
{
    CoordinatorFixture f;

    SECTION("1. Descarga cuando no hay plugin es no-op e idempotente")
    {
        REQUIRE_FALSE(f.uiCoordinator->hasActivePlugin());
        REQUIRE(f.uiCoordinator->getActivePluginInstance() == nullptr);

        f.uiCoordinator->unloadPlugin();

        const auto& counters = f.uiCoordinator->getTeardownCounters();
        REQUIRE(counters.closeEditorCalls == 0);
        REQUIRE(counters.audioDisconnectCalls == 0);
        REQUIRE(counters.sessionTargetClearCalls == 0);
        REQUIRE(counters.contractUnregisterCalls == 0);
        REQUIRE(counters.hostUnloadCalls == 0);
        REQUIRE(counters.notifyUnloadedCalls == 0);

        REQUIRE(f.mockHost.unloadedCalls == 0);
        REQUIRE_FALSE(f.uiCoordinator->hasActivePlugin());
    }

    SECTION("2. Carga directa y routing hacia audio, sesion y contrato de hardware")
    {
        auto dummy = std::make_unique<DummyPlugin>(true, false);
        auto desc = createDummyDesc("MockTestPlugin", false);

        // Adopt into backend hostManager: triggers onPluginLoaded automatically
        auto res = f.hostManager.adoptPluginInstance(std::move(dummy), desc, 48000.0, 256);
        REQUIRE(res.succeeded);
        REQUIRE(f.hostManager.hasActivePlugin());

        REQUIRE(f.uiCoordinator->hasActivePlugin());
        REQUIRE(f.uiCoordinator->getActivePluginInstance() != nullptr);
        REQUIRE(f.mockHost.identityCalls == 1);
        REQUIRE(f.mockHost.lastIdentity.pluginName == "MockTestPlugin");
        REQUIRE(f.mockHost.lastIdentity.formatName == "VST3");
        REQUIRE_FALSE(f.mockHost.lastIdentity.isInstrument);
        REQUIRE(f.mockHost.lastIdentity.category == "PLUGIN_VIRTUAL");

        // Contract registered in hardwareManager
        const auto& contracts = f.hardwareManager.getContractRegistry().getContracts();
        bool contractFound = false;
        for (const auto& c : contracts)
        {
            if (c.deviceType == "SOFTWARE_PLUGIN" && c.displayName == "MockTestPlugin")
                contractFound = true;
        }
        REQUIRE(contractFound);

        // Audio engine & session coordinator have active plugin
        REQUIRE(f.audioEngine.getActivePluginInstance() != nullptr);
        REQUIRE(f.sequencer.getHardwareDispatcher().getTargetPluginInstance() != nullptr);
    }

    SECTION("3. Descarga simetrica e idempotente con validacion estricta de contadores")
    {
        auto dummy = std::make_unique<DummyPlugin>(true, true);
        auto desc = createDummyDesc("MockTestPlugin", true);

        f.hostManager.adoptPluginInstance(std::move(dummy), desc, 48000.0, 256);

        REQUIRE(f.uiCoordinator->hasActivePlugin());

        // First unload: must clean everything with exact counters = 1
        f.uiCoordinator->unloadPlugin();

        const auto& c1 = f.uiCoordinator->getTeardownCounters();
        REQUIRE(c1.closeEditorCalls == 1);
        REQUIRE(c1.audioDisconnectCalls == 1);
        REQUIRE(c1.sessionTargetClearCalls == 1);
        REQUIRE(c1.contractUnregisterCalls == 1);
        REQUIRE(c1.hostUnloadCalls == 1);
        REQUIRE(c1.notifyUnloadedCalls == 1);

        REQUIRE_FALSE(f.uiCoordinator->hasActivePlugin());
        REQUIRE(f.uiCoordinator->getActivePluginInstance() == nullptr);
        REQUIRE(f.audioEngine.getActivePluginInstance() == nullptr);
        REQUIRE(f.sequencer.getHardwareDispatcher().getTargetPluginInstance() == nullptr);
        REQUIRE(f.mockHost.unloadedCalls == 1);

        // Dynamic contract must be cleanly unregistered
        const auto& contracts = f.hardwareManager.getContractRegistry().getContracts();
        for (const auto& c : contracts)
        {
            REQUIRE_FALSE(c.displayName == "MockTestPlugin");
        }

        // Second unload: must be completely idempotent and not increment ANY counter
        f.uiCoordinator->unloadPlugin();

        const auto& c2 = f.uiCoordinator->getTeardownCounters();
        REQUIRE(c2.closeEditorCalls == 1);
        REQUIRE(c2.audioDisconnectCalls == 1);
        REQUIRE(c2.sessionTargetClearCalls == 1);
        REQUIRE(c2.contractUnregisterCalls == 1);
        REQUIRE(c2.hostUnloadCalls == 1);
        REQUIRE(c2.notifyUnloadedCalls == 1);
        REQUIRE(f.mockHost.unloadedCalls == 1);
    }

    SECTION("4. Carga sucesiva reemplaza contrato anterior sin duplicados")
    {
        auto desc1 = createDummyDesc("FirstPlugin", false);
        auto dummy1 = std::make_unique<DummyPlugin>(false, false);
        f.hostManager.adoptPluginInstance(std::move(dummy1), desc1);

        REQUIRE(f.uiCoordinator->getActivePluginDescription().name == "FirstPlugin");

        auto desc2 = createDummyDesc("SecondPlugin", true);
        auto dummy2 = std::make_unique<DummyPlugin>(true, true);
        f.hostManager.adoptPluginInstance(std::move(dummy2), desc2);

        REQUIRE(f.uiCoordinator->getActivePluginDescription().name == "SecondPlugin");
        REQUIRE(f.mockHost.lastIdentity.isInstrument);

        // Only SecondPlugin contract must exist
        int pluginContracts = 0;
        for (const auto& c : f.hardwareManager.getContractRegistry().getContracts())
        {
            if (c.deviceType == "SOFTWARE_PLUGIN")
            {
                pluginContracts++;
                REQUIRE(c.displayName == "SecondPlugin");
            }
        }
        REQUIRE(pluginContracts == 1);
    }

    SECTION("5. Editor: apertura, cierre, plugin valido sin editor e idempotencia")
    {
        // Sin plugin: no-op
        f.uiCoordinator->showEditor();
        REQUIRE_FALSE(f.windowController.isWindowOpen());

        // Plugin valido sin editor
        auto noEditorDummy = std::make_unique<DummyPlugin>(false, false);
        auto descNoEd = createDummyDesc("NoEditorPlugin", false);
        f.hostManager.adoptPluginInstance(std::move(noEditorDummy), descNoEd);

        f.uiCoordinator->showEditor();
        REQUIRE_FALSE(f.windowController.isWindowOpen());
        f.uiCoordinator->closeEditor();
        REQUIRE_FALSE(f.windowController.isWindowOpen());

        // Plugin valido con editor
        auto editorDummy = std::make_unique<DummyPlugin>(true, false);
        auto descEd = createDummyDesc("WithEditorPlugin", false);
        f.hostManager.adoptPluginInstance(std::move(editorDummy), descEd);

        f.uiCoordinator->showEditor();
        REQUIRE(f.windowController.isWindowOpen());

        // Llamada duplicada a showEditor: idempotente
        f.uiCoordinator->showEditor();
        REQUIRE(f.windowController.isWindowOpen());

        // Descarga con editor abierto: debe cerrar el editor
        f.uiCoordinator->unloadPlugin();
        REQUIRE_FALSE(f.windowController.isWindowOpen());
        REQUIRE(f.uiCoordinator->getActivePluginInstance() == nullptr);
    }

    SECTION("6. Plugin valido sin buses de audio (canal 0/0)")
    {
        auto noBusesDummy = std::make_unique<DummyPlugin>(true, true, false);
        auto descNoBuses = createDummyDesc("NoBusesSynth", true);

        auto res = f.hostManager.adoptPluginInstance(std::move(noBusesDummy), descNoBuses, 48000.0, 256);
        REQUIRE(res.succeeded);
        REQUIRE(f.uiCoordinator->hasActivePlugin());
        REQUIRE(f.uiCoordinator->getActivePluginInstance() != nullptr);
        REQUIRE(f.mockHost.lastIdentity.pluginName == "NoBusesSynth");

        f.uiCoordinator->unloadPlugin();
        REQUIRE_FALSE(f.uiCoordinator->hasActivePlugin());
    }

    SECTION("7. Fallo de carga asincrona notifica un unico error a la UI")
    {
        juce::PluginDescription invalidDesc;
        invalidDesc.name = "NonExistentPlugin";
        invalidDesc.fileOrIdentifier = "invalid_uid_path";

        std::atomic<bool> loadCompleted { false };
        f.uiCoordinator->loadPlugin(invalidDesc, [&](bool) {
            loadCompleted.store(true);
        });

        int elapsed = 0;
        while (!loadCompleted.load() && elapsed < 2000)
        {
            f.uiCoordinator->flushAsyncUpdates();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            elapsed += 5;
        }
        f.uiCoordinator->flushAsyncUpdates();

        REQUIRE(loadCompleted.load());
        REQUIRE_FALSE(f.uiCoordinator->hasActivePlugin());
        REQUIRE(f.mockHost.errorCalls == 1);
        REQUIRE(f.mockHost.lastErrorTitle == "Error al cargar plugin");
    }

    SECTION("8. Callback de unloading repetido no duplica limpieza")
    {
        auto dummy = std::make_unique<DummyPlugin>(true, false);
        auto desc = createDummyDesc("BackendUnloadPlugin", false);

        f.hostManager.adoptPluginInstance(std::move(dummy), desc);

        REQUIRE(f.uiCoordinator->hasActivePlugin());

        // Simular que el backend descargo directamente
        f.hostManager.unloadPlugin();

        REQUIRE_FALSE(f.uiCoordinator->hasActivePlugin());
        REQUIRE(f.mockHost.unloadedCalls == 1);

        const auto& c1 = f.uiCoordinator->getTeardownCounters();
        REQUIRE(c1.closeEditorCalls == 1);
        REQUIRE(c1.audioDisconnectCalls == 1);
        REQUIRE(c1.sessionTargetClearCalls == 1);
        REQUIRE(c1.contractUnregisterCalls == 1);
        REQUIRE(c1.hostUnloadCalls == 1);
        REQUIRE(c1.notifyUnloadedCalls == 1);

        // Repetir llamada en backend
        f.hostManager.unloadPlugin();
        const auto& c2 = f.uiCoordinator->getTeardownCounters();
        REQUIRE(c2.hostUnloadCalls == 1);
        REQUIRE(c2.notifyUnloadedCalls == 1);
    }

    SECTION("9. Identidad UI no se actualiza ante callback obsoleto")
    {
        juce::PluginDescription descOld = createDummyDesc("OldStalePlugin", false);
        juce::PluginDescription descNew = createDummyDesc("NewValidPlugin", true);

        // Disparamos carga antigua
        f.uiCoordinator->loadPlugin(descOld);

        // Inmediatamente disparamos una nueva carga
        f.uiCoordinator->loadPlugin(descNew);

        // Adoptamos la nueva directamente para simular que termino la nueva
        auto dummyNew = std::make_unique<DummyPlugin>(true, true);
        f.hostManager.adoptPluginInstance(std::move(dummyNew), descNew);

        // Vaciamos actualizaciones pendientes (la operacion antigua fue descartada por opId)
        f.uiCoordinator->flushAsyncUpdates();

        REQUIRE(f.uiCoordinator->getActivePluginDescription().name == "NewValidPlugin");
        REQUIRE(f.mockHost.lastIdentity.pluginName == "NewValidPlugin");
        REQUIRE(f.mockHost.lastIdentity.isInstrument);
    }

    SECTION("10. Puntero observador queda nulo tras unload")
    {
        auto dummy = std::make_unique<DummyPlugin>(false, false);
        auto desc = createDummyDesc("ObserverNullCheck", false);
        f.hostManager.adoptPluginInstance(std::move(dummy), desc);

        REQUIRE(f.uiCoordinator->getActivePluginInstance() != nullptr);
        REQUIRE(f.uiCoordinator->hasActivePlugin());

        f.uiCoordinator->unloadPlugin();

        REQUIRE(f.uiCoordinator->getActivePluginInstance() == nullptr);
        REQUIRE_FALSE(f.uiCoordinator->hasActivePlugin());
    }

    SECTION("11. Control de direct monitoring por ventana y delegacion explicita")
    {
        REQUIRE_FALSE(f.audioEngine.isPluginMonitoringEnabled());

        // Simulamos apertura de ventana de plugin via callback
        if (f.windowController.onWindowStateChanged)
            f.windowController.onWindowStateChanged(true);
        REQUIRE(f.audioEngine.isPluginMonitoringEnabled());

        // Simulamos cierre de ventana de plugin via callback
        if (f.windowController.onWindowStateChanged)
            f.windowController.onWindowStateChanged(false);
        REQUIRE_FALSE(f.audioEngine.isPluginMonitoringEnabled());

        // Delegacion explicita (ej. virtual keyboard)
        f.uiCoordinator->setMonitoringEnabled(true);
        REQUIRE(f.audioEngine.isPluginMonitoringEnabled());

        f.uiCoordinator->setMonitoringEnabled(false);
        REQUIRE_FALSE(f.audioEngine.isPluginMonitoringEnabled());
    }
}
