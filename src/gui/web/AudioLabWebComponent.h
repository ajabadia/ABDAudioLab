#pragma once

#include <WebView2Bridge/JuceWebView2Component.h>
#include <WebView2Bridge/WebView2ResourceProvider.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <string>

#include "../session/ProfilingSessionContracts.h"
#include "../session/ProfilingSessionController.h"

namespace abdaudiolab::core
{
class PluginHostManager;
}

namespace abdaudiolab::audio
{
class LabAudioEngine;
}

namespace abdaudiolab::gui
{
class PluginWindowController;
class NativeCalibrationPanel;
class ScopeWebFloatingWindow;
class SoundIdTheme;
class PluginScanDirectoriesModal;
class MeasurementFloatingWindow;
}

namespace abdaudiolab::gui::web
{

/**
 * @class AudioLabWebComponent
 * @brief Contenedor WebView2 de alto rendimiento para la interfaz web de ABDAudioLab.
 *
 * Características:
 * - Hereda de `abd::webview2::JuceWebView2Component`.
 * - Escucha `session::ProfilingSessionSnapshot` emitidos por `ProfilingSessionController`.
 * - Despacho de telemetría a 60 Hz con cero asignaciones de memoria en el hilo de audio en tiempo real.
 * - Protocolo IPC bidireccional estructurado (JS -> C++ via nativeEvent, C++ -> JS via sessionSnapshot/telemetryFrame).
 * - Integración con `juce::AudioDeviceSelectorComponent` para configuración estándar de Audio y MIDI.
 * - Hosting aislado de plugins VST3 con control de ventana GUI flotante nativa (Fase 3).
 * - Persistencia atómica de sesiones (.abdlabtest / JSON) con diálogos nativos (Fase 3).
 */
class AudioLabWebComponent : public abd::webview2::JuceWebView2Component,
                             public juce::Timer,
                             public session::IProfilingSessionEventListener,
                             public juce::ChangeListener
{
public:
    using ProgressCallback = std::function<void(const juce::String& message, float progress)>;

    explicit AudioLabWebComponent(session::ProfilingSessionController* controller = nullptr,
                                  const juce::String& devServerUrl = {},
                                  ProgressCallback progressCb = nullptr);
    ~AudioLabWebComponent() override;

    // juce::ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // session::IProfilingSessionEventListener
    void onSessionSnapshotUpdated(const session::ProfilingSessionSnapshot& snapshot) override;
    void onAlertRaised(const session::UiAlert& alert) override;
    void onWorkflowStageChanged(session::ProfilingWorkflowStage newStage) override;
    void onSessionStatusChanged(session::ProfilingSessionStatus newStatus) override;

    // Conexión con motor de audio para JUCE Settings y estado de hardware
    void setAudioEngine(audio::LabAudioEngine* engine);
    void setCalibrationPanel(gui::NativeCalibrationPanel* panel);
    void openAudioMidiSettings();
    void emitAudioDeviceState();

    // Hosting VST3 Aislado (Fase 3)
    void setPluginHosting(core::PluginHostManager* hostManager, gui::PluginWindowController* windowController);
    void openPluginGui();
    void closePluginGui();
    void promptLoadPluginFile();
    void loadPluginFile(const juce::File& file);
    void loadPluginDescription(const juce::PluginDescription& desc);
    void loadPluginByFileOrIdentifier(const juce::String& identifier, const juce::String& fallbackName = {});
    void emitAvailablePlugins();

    // Persistencia de Sesiones (Fase 3)
    void createNewSession();
    void promptSaveSession(bool saveAs = false);
    bool saveSessionToFile(const juce::File& file);
    void promptLoadSession();
    bool loadSessionFromFile(const juce::File& file);

    // Exportación Certificada e Integración de Sistema (Fase 4)
    void exportProductionPackage();
    void openCertificationReport();
    void openDataFolder(const juce::String& folderType);
    void exitApplication();
    void toggleScopeWindow();
    void updateScopeTheme();
    void promptSetWorkingDirectory();
    void openWorkingDirectory();
    void openPluginScanModal();
    void openMeasurementViewerWindow();
    void openMeasurementComparisonWindow();
    void emitStartupNotices();
    void emitWorkspaceRecipes();
    void saveRecipeToWorkspace(const juce::var& recipeVar);

    // Métodos para actualizar la telemetría desde el motor de audio C++ (Thread-Safe & Zero Alloc)
    void updateMeterLevels(float inDbfs, float outDbfs) noexcept;

    // Callbacks del Timer (hilo de mensaje/UI)
    void timerCallback() override;

    void handleWebUiEvent(const juce::var& message);

    // Callbacks para UI y tema
    std::function<void()> onAutoDetectHardwareRequested;
    std::function<void(const juce::String& newTheme)> onThemeChanged;
    std::function<void()> onWebUiReady;
    [[nodiscard]] juce::String getCurrentTheme() const noexcept { return juce::String(currentTheme); }

protected:
    void onPageLoaded() override;

private:
    session::ProfilingSessionController* controller_ { nullptr };
    audio::LabAudioEngine* audioEngine_ { nullptr };
    gui::NativeCalibrationPanel* calibrationPanel_ { nullptr };
    core::PluginHostManager* pluginHostManager_ { nullptr };
    gui::PluginWindowController* pluginWindowController_ { nullptr };
    std::unique_ptr<gui::ScopeWebFloatingWindow> scopeWebWindow_;
    std::unique_ptr<gui::SoundIdTheme> soundIdTheme_;
    std::unique_ptr<gui::PluginScanDirectoriesModal> pluginScanModal_;
    std::unique_ptr<gui::MeasurementFloatingWindow> measurementViewerWindow_;
    std::unique_ptr<gui::MeasurementFloatingWindow> measurementComparisonWindow_;
    juce::StringArray startupNotices_;

    session::ProfilingSessionSnapshot cachedSnapshot_;
    juce::File currentSessionFile_;
    std::unique_ptr<juce::FileChooser> activeFileChooser_;
    bool openGuiAfterLoad_ { false };

    juce::String devUrl_;
    std::atomic<float> latestInDbfs_ { -60.0f };
    std::atomic<float> latestOutDbfs_ { -60.0f };
    std::atomic<bool> hasTelemetryUpdate_ { false };

    // Sonda Preflight - Monitoreo acumulado durante la ráfaga
    std::atomic<bool> isPreflightProbing_ { false };
    std::atomic<float> preflightMaxPeak_ { 0.0f };
    std::atomic<float> preflightMaxRms_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioLabWebComponent)
};

} // namespace abdaudiolab::gui::web
