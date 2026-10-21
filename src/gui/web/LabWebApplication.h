/**
 * @file LabWebApplication.h
 * @brief Bootstrap y ciclo de vida de la aplicación moderna WebView2 (ABDAudioLab_Web).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../BuildVersion.h"
#include "../../audio/LabAudioEngine.h"
#include "../../hardware/AudioLabMidiBackend.h"
#include <HardwareMidiDetect/JuceHardwareMidiPicker.h>
#include "../NativeCalibrationPanel.h"
#include "../../core/plugins/PluginHostManager.h"
#include "../../core/LabResourcePaths.h"
#include <HardwareMidiDetect/HardwareContractRegistry.h>
#include "../plugins/PluginWindowController.h"
#include "../NativeTitleBarDwm.h"
#include "../AppTheme.h"
#include "../SoundIdTheme.h"
#include "../SoundIdSplashScreen.h"
#include "AudioLabWebComponent.h"

namespace abdaudiolab::gui::web
{

class LabWebMainWindow : public juce::DocumentWindow,
                         public juce::ChangeListener
{
public:
    explicit LabWebMainWindow(const juce::String& name,
                              std::function<void(const juce::String&, float)> progressCb = nullptr,
                              std::function<void()> onReadyCb = nullptr)
        : DocumentWindow(name,
                         juce::Colour(0xff070a0e), // --color-bg-base audiolab
                         DocumentWindow::allButtons),
          sessionController(std::make_unique<session::ProfilingSessionController>()),
          audioEngine(std::make_unique<audio::LabAudioEngine>()),
          pluginHostManager(std::make_unique<core::PluginHostManager>()),
          pluginWindowController(std::make_unique<gui::PluginWindowController>())
    {
        setUsingNativeTitleBar(true);

        const bool isDarkInit = (activeTheme_ != "audiolab-light" && activeTheme_ != "light");
        gui::SoundIdTheme::applyThemeMode(isDarkInit ? gui::AppTheme::ThemeMode::Dark : gui::AppTheme::ThemeMode::Light);

        if (progressCb) progressCb("Inicializando motor de audio...", 0.25f);

        juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ABDAudioLab");
        settingsFile = appData.getChildFile("AudioSettings.xml");
        audioEngine->initializeAudioDevices(settingsFile);
        audioEngine->getDeviceManager().addChangeListener(this);

        if (progressCb) progressCb("Cargando plugins y entorno de trabajo...", 0.50f);

        auto* web = new AudioLabWebComponent(sessionController.get());
        web->setAudioEngine(audioEngine.get());
        pluginHostManager->loadCache(core::PluginHostManager::getDefaultCacheFile());
        web->setPluginHosting(pluginHostManager.get(), pluginWindowController.get());
        calibrationPanel = std::make_unique<NativeCalibrationPanel>(*audioEngine);
        web->setCalibrationPanel(calibrationPanel.get());

        if (onReadyCb)
            web->onWebUiReady = std::move(onReadyCb);

        web->onAutoDetectHardwareRequested = [this] {
            openHardwarePickerModal();
        };
        web->onThemeChanged = [this](const juce::String& theme) {
            activeTheme_ = theme;
            const bool isDark = (theme != "audiolab-light" && theme != "light");
            gui::SoundIdTheme::applyThemeMode(isDark ? gui::AppTheme::ThemeMode::Dark : gui::AppTheme::ThemeMode::Light);
            gui::applyNativeTitleBarDarkMode(this, isDark);
            setBackgroundColour(isDark ? juce::Colour(0xff070a0e) : juce::Colour(0xfff8fafc));
            applyThemeToPicker(theme);
            if (pluginWindowController != nullptr)
                pluginWindowController->updateTheme();
            if (webComponent != nullptr)
                webComponent->updateScopeTheme();
        };
        webComponent = web;
        setContentOwned(webComponent, true);

        setResizable(true, true);
        setResizeLimits(1120, 700, 3840, 2160);

        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        int defaultWidth = 1540;
        int defaultHeight = 940;
        if (display != nullptr)
        {
            const auto userArea = display->userArea;
            defaultWidth = juce::jmin(defaultWidth, juce::jmax(1120, userArea.getWidth() - 60));
            defaultHeight = juce::jmin(defaultHeight, juce::jmax(700, userArea.getHeight() - 60));
        }

        centreWithSize(defaultWidth, defaultHeight);
        setVisible(true);

        const bool isDark = (activeTheme_ != "audiolab-light" && activeTheme_ != "light");
        gui::applyNativeTitleBarDarkMode(this, isDark);

        if (progressCb) progressCb("Iniciando interfaz WebView2...", 0.80f);
    }

    ~LabWebMainWindow() override
    {
        audioEngine->getDeviceManager().removeChangeListener(this);
        audioEngine->saveAudioSettings(settingsFile);
    }

    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        if (source == &audioEngine->getDeviceManager())
        {
            if (webComponent != nullptr)
            {
                webComponent->emitAudioDeviceState();
            }
            audioEngine->saveAudioSettings(settingsFile);
        }
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void applyThemeToPicker(const juce::String& theme)
    {
        const bool isLight = (theme == "audiolab-light" || theme == "light");
        const juce::Colour bg = isLight ? juce::Colour(0xfff8f9fa) : juce::Colour(0xff0b1017);

        if (pickerWindow != nullptr)
        {
            pickerWindow->setBackgroundColour(bg);
            if (auto* picker = dynamic_cast<abd::hwid::JuceHardwareMidiPicker*>(pickerWindow->getContentComponent()))
            {
                picker->setTheme(theme.toStdString());
            }
        }
    }

    void openHardwarePickerModal()
    {
        const bool isLight = (activeTheme_ == "audiolab-light" || activeTheme_ == "light");
        const juce::Colour bg = isLight ? juce::Colour(0xfff8f9fa) : juce::Colour(0xff0b1017);

        if (pickerWindow != nullptr)
        {
            pickerWindow->setBackgroundColour(bg);
            pickerWindow->setVisible(true);
            pickerWindow->toFront(true);
            if (auto* picker = dynamic_cast<abd::hwid::JuceHardwareMidiPicker*>(pickerWindow->getContentComponent()))
            {
                picker->setTheme(activeTheme_.toStdString());
                picker->startPick();
            }
            return;
        }

        if (midiBackend == nullptr)
            midiBackend = std::make_unique<hardware::AudioLabMidiBackend>();

        auto onPickCallback = [this](const abd::hwid::HardwarePickResult& res) {
            if (!res.cancelled && !res.hardwareId.empty())
            {
                juce::Logger::writeToLog("[LabWebMainWindow] Dispositivo detectado: " + juce::String(res.displayName));
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("hardwareId", juce::String(res.hardwareId));
                obj->setProperty("displayName", juce::String(res.displayName));
                obj->setProperty("brand", juce::String(res.manufacturer));
                obj->setProperty("model", juce::String(res.model));
                if (webComponent != nullptr)
                    webComponent->emitEventIfBrowserIsVisible("hardwareDetected", juce::var(obj.get()));
            }
        };

        abd::hwid::HardwareMidiDetector::DetectionConfig detectionConfig;
        detectionConfig.maxResults = 1;
        detectionConfig.autoSelectIfSingle = true;
        detectionConfig.includeHeuristic = true;
        detectionConfig.requireSysExVerified = false;

        abd::hwid::HardwareContractRegistry sharedRegistry;
        if (const auto sharedContracts = core::sharedContractsDir(); sharedContracts.isDirectory())
        {
            sharedRegistry.loadContractsFromDirectory(sharedContracts);
        }
        if (sharedRegistry.getContracts().empty())
        {
            if (const auto localHardware = core::contractsHardwareDir(); localHardware.isDirectory())
                sharedRegistry.loadContractsFromDirectory(localHardware);
        }
        std::vector<abd::hwid::HardwareContract> contracts = sharedRegistry.getContracts();

        auto* win = new juce::DocumentWindow("Hardware MIDI Auto-Detection (ABDShared)",
                                             bg,
                                             juce::DocumentWindow::closeButton);
        win->setUsingNativeTitleBar(false);
        win->setResizable(true, false);
        win->setResizeLimits(540, 480, 800, 700);

        auto* picker = new abd::hwid::JuceHardwareMidiPicker(
            *midiBackend,
            [this, win, onPickCallback](const abd::hwid::HardwarePickResult& res) {
                onPickCallback(res);
                win->setVisible(false);
            },
            contracts,
            detectionConfig,
            activeTheme_
        );

        win->setContentOwned(picker, true);
        win->centreWithSize(620, 540);
        win->setVisible(true);
        pickerWindow.reset(win);

        juce::MessageManager::callAsync([picker, this]() {
            if (picker != nullptr)
            {
                picker->setTheme(activeTheme_.toStdString());
                picker->startPick();
            }
        });
    }

private:
    juce::String activeTheme_ { "audiolab-dark" };
    std::unique_ptr<session::ProfilingSessionController> sessionController;
    std::unique_ptr<audio::LabAudioEngine> audioEngine;
    std::unique_ptr<NativeCalibrationPanel> calibrationPanel;
    std::unique_ptr<core::PluginHostManager> pluginHostManager;
    std::unique_ptr<gui::PluginWindowController> pluginWindowController;
    AudioLabWebComponent* webComponent { nullptr };
    juce::File settingsFile;
    std::unique_ptr<hardware::AudioLabMidiBackend> midiBackend;
    std::unique_ptr<juce::DocumentWindow> pickerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabWebMainWindow)
};

class LabWebApplication : public juce::JUCEApplication
{
public:
    LabWebApplication() = default;

    const juce::String getApplicationName() override    { return "ABDAudioLab_Web"; }
    const juce::String getApplicationVersion() override { return version::kAppVersion; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // 0. Activar registro en archivo en %APPDATA%/ABDAudioLab/ABDAudioLab_Web.log
        juce::File logDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ABDAudioLab");
        if (!logDir.exists()) logDir.createDirectory();
        fileLogger = std::make_unique<juce::FileLogger>(logDir.getChildFile("ABDAudioLab_Web.log"), "=== ABDAudioLab_Web Session Log ===");
        juce::Logger::setCurrentLogger(fileLogger.get());

        // 1. Mostrar Splash Screen flotante instantáneo (< 50ms)
        juce::Logger::writeToLog("[App] Creating SoundIdSplashWindow...");
        splashWindow = std::make_unique<gui::SoundIdSplashWindow>();
        juce::Logger::writeToLog("[App] SoundIdSplashWindow created. Reporting initial progress...");
        splashWindow->reportProgress("Iniciando ABDAudioLab...", 0.05f);

        // 2. Inicializar ventana principal con progreso real y fade-out al terminar
        juce::MessageManager::callAsync([this]() {
            auto progressCb = [this](const juce::String& msg, float prog) {
                if (splashWindow != nullptr)
                    splashWindow->reportProgress(msg, prog);
            };

            auto onReadyCb = [this]() {
                if (splashWindow != nullptr)
                {
                    splashWindow->reportProgress("Listo.", 1.0f);
                    juce::Component::SafePointer<gui::SoundIdSplashWindow> safeSplash(splashWindow.get());
                    juce::Timer::callAfterDelay(450, [safeSplash, this]() {
                        if (safeSplash != nullptr)
                        {
                            safeSplash->dismiss([this]() {
                                juce::MessageManager::callAsync([this]() {
                                    splashWindow.reset();
                                    if (mainWindow != nullptr)
                                        mainWindow->toFront(true);
                                });
                            });
                        }
                        else if (mainWindow != nullptr)
                        {
                            mainWindow->toFront(true);
                        }
                    });
                }
                else if (mainWindow != nullptr)
                {
                    mainWindow->toFront(true);
                }
            };

            mainWindow = std::make_unique<LabWebMainWindow>(getApplicationName(), progressCb, onReadyCb);

            // Timeout de seguridad: Si WebView2 tarda o falla el reporte, descartar el splash
            juce::Component::SafePointer<gui::SoundIdSplashWindow> safeSplash(splashWindow.get());
            juce::Timer::callAfterDelay(6000, [safeSplash, this]() {
                if (safeSplash != nullptr)
                {
                    safeSplash->dismiss([this]() {
                        splashWindow.reset();
                        if (mainWindow != nullptr)
                            mainWindow->toFront(true);
                    });
                }
            });
        });
    }

    void shutdown() override
    {
        mainWindow.reset();
        splashWindow.reset();
        juce::Logger::setCurrentLogger(nullptr);
        fileLogger.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<juce::FileLogger> fileLogger;
    std::unique_ptr<gui::SoundIdSplashWindow> splashWindow;
    std::unique_ptr<LabWebMainWindow> mainWindow;
};

} // namespace abdaudiolab::gui::web
