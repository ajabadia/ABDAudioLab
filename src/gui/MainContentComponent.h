/**
 * @file MainContentComponent.h
 * @brief Main application content component: hardware selection, session plan, measurement
 *        sequencing, live visualization, and session management UI orchestration.
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "BuildVersion.h"
#include "audio/LabAudioEngine.h"
#include "core/HardwareContractRegistry.h"
#include "core/ProfilingSession.h"
#include "core/ProfilingSequencer.h"
#include "core/SessionSerializer.h"
#include "core/SessionManager.h"
#include "core/HardwareManager.h"

#include "gui/SoundIdTheme.h"
#include "gui/SoundIdCurvePlotter.h"
#include "gui/SoundIdMeterStrip.h"
#include "gui/SoundIdSuiteList.h"
#include "gui/SlideInDrawer.h"
#include "gui/InfoDrawer.h"
#include "gui/AboutModalDialog.h"
#include "gui/LoopbackCalibrationModal.h"
#include "gui/HardwareSelectorPill.h"
#include "gui/AudioMidiStatusPill.h"
#include "gui/SoundIdSplashScreen.h"
#include "gui/MeasurementHealthPanel.h"
#include "gui/OperatorStepModalDialog.h"
#include "gui/ConfirmationModalDialog.h"
#include "gui/ScopeWebFloatingWindow.h"
#include "export/CertificationReportExporter.h"
#include "export/NamDatasetExporter.h"
#include "config/AutoUpdaterConfig.h"
#include <AutoUpdater/AutoUpdater.h>

namespace abdaudiolab
{

inline std::string mapBadgeToBlockType(const juce::String& badgeText)
{
    if (badgeText == "FLT") return "SpectrumFilter";
    if (badgeText == "ENV") return "TimeDynamic";
    if (badgeText == "SAT") return "WaveShaper";
    if (badgeText == "MOD") return "CyclicModulator";
    if (badgeText == "WNH") return "WienerHammerstein";
    if (badgeText == "NAM") return "NeuralCalibration";
    return "AmplitudeGain";
}

inline hardware::AiraModel mapHardwareIdToAiraModel(const juce::String& hwId)
{
    if (hwId == "roland_aira_bitrazer") return hardware::AiraModel::Bitrazer;
    if (hwId == "roland_aira_demora")   return hardware::AiraModel::Demora;
    if (hwId == "roland_aira_torcido")  return hardware::AiraModel::Torcido;
    if (hwId == "roland_aira_scooper")  return hardware::AiraModel::Scooper;
    return hardware::AiraModel::GenericModular;
}

inline void applyBadgeForStimulus(gui::QueueItem& item, audio::StimulusType type)
{
    if (type == audio::StimulusType::SyncPulses3)
    {
        item.badgeText = "ENV";
        item.badgeColor = juce::Colour(0xff8b5cf6);
    }
    else if (type == audio::StimulusType::AmplitudeRamp)
    {
        item.badgeText = "SAT";
        item.badgeColor = juce::Colour(0xfff59e0b);
    }
    else if (type == audio::StimulusType::SineWave1kHz)
    {
        item.badgeText = "MOD";
        item.badgeColor = juce::Colour(0xff0284c7);
    }
    else
    {
        item.badgeText = "FLT";
        item.badgeColor = juce::Colour(0xff10b981);
    }
}

class MonochromeInfoButton : public juce::Button
{
public:
    MonochromeInfoButton() : juce::Button("InfoButton")
    {
        setTooltip("System telemetry & active routing information");
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        if (shouldDrawButtonAsDown)
        {
            g.setColour(gui::SoundIdTheme::bgCardHover);
            g.fillEllipse(bounds);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(gui::SoundIdTheme::bgCard);
            g.fillEllipse(bounds);
        }

        // Circular outline
        g.setColour(gui::SoundIdTheme::borderCard);
        g.drawEllipse(bounds.reduced(2.0f), 1.2f);

        // "i" text
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.setColour(gui::SoundIdTheme::textPrimary);
        g.drawText("i", bounds, juce::Justification::centred, false);
    }
};

class ThemeToggleButton : public juce::Button
{
public:
    ThemeToggleButton() : juce::Button("ThemeToggle")
    {
        setTooltip("Switch Interface Theme (Light / Dark)");
    }

    void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(isDown ? gui::SoundIdTheme::bgCardHover.darker(0.08f)
                           : (isHighlighted ? gui::SoundIdTheme::bgCardHover : gui::SoundIdTheme::bgCard));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(gui::SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        auto c = bounds.getCentre();
        g.setColour(gui::SoundIdTheme::textPrimary);

        if (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark)
        {
            // Monochrome vector crescent moon
            juce::Path crescent;
            float r = 6.2f;
            crescent.startNewSubPath(c.x + r * 0.25f, c.y - r);
            crescent.cubicTo(c.x + r * 1.15f, c.y - r * 0.35f, c.x + r * 1.15f, c.y + r * 0.35f, c.x + r * 0.25f, c.y + r);
            crescent.cubicTo(c.x + r * 0.7f, c.y + r * 0.38f, c.x + r * 0.7f, c.y - r * 0.38f, c.x + r * 0.25f, c.y - r);
            crescent.closeSubPath();
            g.fillPath(crescent);
        }
        else
        {
            // Monochrome vector sun (circle + 8 rays)
            float r = 3.6f;
            g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);
            for (int i = 0; i < 8; ++i)
            {
                float angle = static_cast<float>(i) * juce::MathConstants<float>::pi * 0.25f;
                float x1 = c.x + 5.2f * std::cos(angle);
                float y1 = c.y + 5.2f * std::sin(angle);
                float x2 = c.x + 7.6f * std::cos(angle);
                float y2 = c.y + 7.6f * std::sin(angle);
                g.drawLine(x1, y1, x2, y2, 1.2f);
            }
        }
    }
};

class CenterSplitterBar : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    std::function<void(int deltaY)> onDragged;
    std::function<void()> onResetToDefault;

    CenterSplitterBar()
    {
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setTooltip("Drag up/down to resize Graph and Queue \u2022 Double-click to reset split");
    }

    void mouseEnter(const juce::MouseEvent&) override { isHovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { isHovered = false; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStartPos = e.getEventRelativeTo(getParentComponent()).getPosition();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto currentPos = e.getEventRelativeTo(getParentComponent()).getPosition();
        int deltaY = currentPos.y - dragStartPos.y;
        dragStartPos = currentPos;
        if (onDragged) onDragged(deltaY);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (onResetToDefault) onResetToDefault();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        float cy = b.getCentreY();

        // Divider line
        g.setColour(isHovered ? gui::SoundIdTheme::accentGreen.withAlpha(0.7f) : gui::SoundIdTheme::borderSubtle);
        g.drawLine(b.getX(), cy, b.getRight(), cy, 1.0f);

        // Centered grip handle pill
        float gripW = 44.0f;
        float gripH = 4.0f;
        auto gripRect = juce::Rectangle<float>(b.getCentreX() - gripW * 0.5f, cy - gripH * 0.5f, gripW, gripH);
        g.setColour(isHovered ? gui::SoundIdTheme::accentGreen : gui::SoundIdTheme::textSecondary.withAlpha(0.5f));
        g.fillRoundedRectangle(gripRect, 2.0f);
    }

private:
    bool isHovered { false };
    juce::Point<int> dragStartPos;
};

class MainContentComponent : public juce::Component,
                             public juce::Timer,
                             public juce::KeyListener,
                             public juce::ChangeListener
{
public:
    MainContentComponent()
        : mockController(),
          sequencer(audioEngine, mockController)
    {
        setLookAndFeel(&soundIdTheme);
        juce::LookAndFeel::setDefaultLookAndFeel(&soundIdTheme);

        // 1. Initialize Audio Engine & Restore State
        juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ABDAudioLab");
        settingsFile = appData.getChildFile("AudioSettings.xml");
        audioEngine.initializeAudioDevices(settingsFile);
        audioEngine.setMockHardware(&mockController);

        // Load Contract Specifications Dynamically from contracts/hardware/ via relative traversal
        std::vector<juce::File> roots = {
            juce::File::getCurrentWorkingDirectory(),
            juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory()
        };

        for (auto root : roots)
        {
            for (int i = 0; i < 6; ++i)
            {
                auto direct = root.getChildFile("contracts").getChildFile("hardware");
                if (direct.isDirectory() && contractRegistry.loadContractsFromDirectory(direct))
                    break;

                auto shared = root.getChildFile("ABDSharedAssets").getChildFile("contracts");
                if (shared.isDirectory() && contractRegistry.loadContractsFromDirectory(shared))
                    break;

                auto siblingShared = root.getParentDirectory().getChildFile("ABDSharedAssets").getChildFile("contracts");
                if (siblingShared.isDirectory() && contractRegistry.loadContractsFromDirectory(siblingShared))
                    break;

                root = root.getParentDirectory();
            }
            if (contractRegistry.hasContracts())
                break;
        }

        if (!contractRegistry.hasContracts())
        {
            juce::Logger::writeToLog("[HardwareContractRegistry ERROR] " + juce::String(contractRegistry.getLastError()));
        }

        // Default export directory
        exportDirectory = juce::File::getCurrentWorkingDirectory().getChildFile("exported_luts");
        exportDirectory.createDirectory();

        // 2. Setup Manual Controller Callback (Only active in Manual Analogue mode)
        manualController.setPromptCallback([this](const juce::String& paramName, float norm, int raw) {
            juce::MessageManager::callAsync([this, paramName, norm, raw]() {
                manualPromptLabel.setText("MANUAL ACTION: Adjust [" + paramName + "] to " + 
                                          juce::String(norm, 2) + " (Raw: " + juce::String(raw) + ") and press SPACEBAR", 
                                          juce::dontSendNotification);
                confirmManualButton.setEnabled(true);
                confirmManualButton.setVisible(true);
                manualPromptLabel.setVisible(true);
                resized();
            });
        });

        // 3. UI Header
        titleLabel.setText("ABDAudioLab", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::textPrimary);
        addAndMakeVisible(titleLabel);

        // Populate Hardware Selector from Contract Registry
        std::vector<gui::HardwareItem> hwItems;
        for (const auto& c : contractRegistry.getContracts())
        {
            gui::HardwareItem item;
            item.id = juce::String(c.id);
            item.displayName = juce::String(c.displayName);
            item.description = juce::String(c.description);
            item.category = juce::String(c.deviceType);
            item.brand = juce::String(c.brand);
            item.brandLogo = juce::String(c.brandLogo);
            item.modelImage = juce::String(c.modelImage);

            for (const auto& f : c.functions)
            {
                gui::FunctionItem fItem;
                fItem.id = juce::String(f.id);
                fItem.name = juce::String(f.name);
                fItem.blockType = juce::String(f.blockType);
                fItem.stimulusOutput = juce::String(f.routingGuide.stimulusOutput);
                fItem.responseInput = juce::String(f.routingGuide.responseInput);
                fItem.notes = juce::String(f.routingGuide.notes);
                fItem.captureMode = juce::String(f.captureMode);
                fItem.defaultBurstDurationSec = f.defaultBurstDurationSec;
                for (const auto& ctrl : f.controls)
                {
                    gui::ControlItem cItem;
                    cItem.name = juce::String(ctrl.name);
                    cItem.type = juce::String(ctrl.type);
                    fItem.controls.push_back(cItem);
                }
                item.functions.push_back(fItem);
            }
            hwItems.push_back(item);
        }
        drawer.setHardwareList(hwItems);
        drawer.setContracts(contractRegistry.getContracts());
        drawer.setHardwareLocked(true);

        // Header Action Buttons
        btnFileMenu.setButtonText(juce::String::fromUTF8(u8"File \u25BE"));
        btnFileMenu.setTooltip("Session Management & File Operations (New, Open, Save, Export, Exit)");
        btnFileMenu.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
        btnFileMenu.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
        btnFileMenu.onClick = [this] { showFilePopupMenu(); };
        addAndMakeVisible(btnFileMenu);

        btnScope.setButtonText("Scope");
        btnScope.setTooltip("ABDScope Visualizer - Open floating multi-lane oscilloscope, real-time FFT spectrum, waterfall, freeze and snapshot inspector.");
        btnScope.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
        btnScope.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
        btnScope.onClick = [this] { toggleScopeWebWindow(); };
        addAndMakeVisible(btnScope);

        // Audio & MIDI Status Pill (Gear button + 4 status LEDs)
        audioMidiStatusPill.onConfigureClicked = [this] { openAudioMidiSettings(); };
        audioMidiStatusPill.updateStatus(audioEngine);
        addAndMakeVisible(audioMidiStatusPill);

        btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"● CALIBRATION: -3.0 dBFS"));
        btnCalibratePill.setTooltip("Sound Card Line Loopback Calibration - Calibrate DAC->ADC loopback latency and flat frequency compensation.");
        btnCalibratePill.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
        btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
        btnCalibratePill.onClick = [this] { loopbackModal.showDialog(this); };
        addAndMakeVisible(btnCalibratePill);

        btnHardwareSelector.clearHardware();
        btnHardwareSelector.setTooltip("Target Hardware & Submodule - Select device under test (Roland, Moog, Minilogue, etc.) and active circuit/submodule.");
        btnHardwareSelector.onClick = [this] { drawer.openHardwareDrawer(); };
        addAndMakeVisible(btnHardwareSelector);

        btnThemeToggle.onClick = [this] {
            auto newMode = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Light)
                               ? gui::AppTheme::ThemeMode::Dark
                               : gui::AppTheme::ThemeMode::Light;
            gui::SoundIdTheme::applyThemeMode(newMode, &soundIdTheme);
            sendLookAndFeelChange();

            btnCalibratePill.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
            btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);

            btnFileMenu.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
            btnFileMenu.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);

            btnScope.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
            btnScope.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);

            btnThemeToggle.repaint();
            btnHardwareSelector.repaint();
            audioMidiStatusPill.repaint();
            btnInfo.repaint();
            healthPanel.repaint();
            meterStrip.repaint();
            curvePlotter.updateTheme();
            suiteList.updateTheme();

            if (scopeWebWindow != nullptr)
            {
                scopeWebWindow->updateTheme();
            }
            drawer.updateTheme();

            repaint();
        };
        addAndMakeVisible(btnThemeToggle);

        // Info Button (Opens Setup & Telemetry Drawer)
        btnInfo.setTooltip("System Telemetry & Info - View real-time DSP load, sample rate, buffer size, latency and application build stats.");
        btnInfo.onClick = [this] { showInfoDrawer(); };
        addAndMakeVisible(btnInfo);

        // 4. Center Curve Plotter & Real-Time Visualization
        addAndMakeVisible(healthPanel);
        addAndMakeVisible(curvePlotter);

        curvePlotter.onToggleCollapse = [this] {
            if (centerSplitMode == CenterSplitMode::GraphMaximized)
                centerSplitMode = CenterSplitMode::Balanced;
            else if (centerSplitMode == CenterSplitMode::QueueMaximized)
                centerSplitMode = CenterSplitMode::Balanced;
            else
                centerSplitMode = CenterSplitMode::GraphMaximized;
            updateSplitLayout();
        };

        suiteList.onToggleCollapse = [this] {
            if (centerSplitMode == CenterSplitMode::QueueMaximized)
                centerSplitMode = CenterSplitMode::Balanced;
            else if (centerSplitMode == CenterSplitMode::GraphMaximized)
                centerSplitMode = CenterSplitMode::Balanced;
            else
                centerSplitMode = CenterSplitMode::QueueMaximized;
            updateSplitLayout();
        };

        centerSplitterBar.onDragged = [this](int deltaY) {
            if (centerSplitMode == CenterSplitMode::Balanced)
            {
                auto bounds = getLocalBounds().reduced(20);
                int minGraphAreaH = 180 + 32 + 12; // 180px curvePlotter + 32px healthPanel + 12px splitter
                int maxBottomH = bounds.getHeight() - minGraphAreaH;
                balancedBottomH = juce::jlimit(50.0f, static_cast<float>(std::max(50, maxBottomH)), balancedBottomH - static_cast<float>(deltaY));
                targetBottomH = balancedBottomH;
                currentBottomH = balancedBottomH;
                resized();
            }
        };
        centerSplitterBar.onResetToDefault = [this] {
            if (centerSplitMode == CenterSplitMode::Balanced)
            {
                balancedBottomH = 220.0f;
                targetBottomH = balancedBottomH;
            }
        };
        addChildComponent(centerSplitterBar);

        // 5. Bottom Test Queue (Session Test Plan & CRUD)
        suiteList.onAddStandardClicked = [this] {
            juce::String selectedHwId = drawer.getSelectedHardwareId();
            juce::String selectedFuncId = drawer.getSelectedFunctionId();
            const auto* contract = contractRegistry.findContractById(selectedHwId.toStdString());
            if (contract == nullptr || contract->functions.empty()) return;

            const auto* targetFunc = &contract->functions[0];
            for (const auto& func : contract->functions)
            {
                if (func.id == selectedFuncId.toStdString())
                {
                    targetFunc = &func;
                    break;
                }
            }

            const auto& f = *targetFunc;
            gui::TestConfiguration stdConf;
            stdConf.testName = juce::String(contract->displayName) + " (" + juce::String(f.name) + ")";
            if (f.blockType == "TimeDynamic") stdConf.stimulusType = audio::StimulusType::SyncPulses3;
            else if (f.blockType == "WaveShaper") stdConf.stimulusType = audio::StimulusType::AmplitudeRamp;
            else if (f.blockType == "CyclicModulator") stdConf.stimulusType = audio::StimulusType::SineWave1kHz;
            else stdConf.stimulusType = audio::StimulusType::LogFarinaSweep;

            stdConf.burstDurationSec = f.defaultBurstDurationSec > 0.05f ? f.defaultBurstDurationSec : 1.0f;
            stdConf.captureMode = f.captureMode;

            stdConf.controls.clear();
            for (size_t k = 0; k < f.controls.size(); ++k)
            {
                gui::ControlStepConfig cs;
                cs.name = f.controls[k].name;
                cs.type = f.controls[k].type;
                cs.steps = (k == 0) ? 8 : ((k == 1) ? 4 : 1);
                stdConf.controls.push_back(cs);
            }

            drawer.openTestEditorDrawer(stdConf, -1);
        };

        suiteList.onAddCustomClicked = [this] {
            gui::TestConfiguration customConf;
            customConf.testName = "Custom Profile";
            customConf.stimulusType = audio::StimulusType::LogFarinaSweep;
            customConf.burstDurationSec = 1.0f;
            customConf.captureMode = "FIXED_TIME";
            
            juce::String selectedHwId = drawer.getSelectedHardwareId();
            juce::String selectedFuncId = drawer.getSelectedFunctionId();
            const auto* contract = contractRegistry.findContractById(selectedHwId.toStdString());
            if (contract != nullptr && !contract->functions.empty())
            {
                const auto* targetFunc = &contract->functions[0];
                for (const auto& func : contract->functions)
                {
                    if (func.id == selectedFuncId.toStdString())
                    {
                        targetFunc = &func;
                        break;
                    }
                }
                const auto& f = *targetFunc;
                for (const auto& c : f.controls)
                {
                    gui::ControlStepConfig cs;
                    cs.name = c.name;
                    cs.type = c.type;
                    cs.steps = 4;
                    customConf.controls.push_back(cs);
                }
            }
            drawer.openTestEditorDrawer(customConf, -1);
        };

        suiteList.onEditTestClicked = [this](int index, const gui::QueueItem& item) {
            if (item.status == gui::QueueItemStatus::Completed || item.status == gui::QueueItemStatus::Incomplete)
            {
                confirmationModal.show(
                    this,
                    "Edit Completed Test",
                    "Modifying the parameters of '" + item.title + "' will invalidate its recorded measurements.\n\nDo you want to proceed and re-queue this test?",
                    "Edit & Invalidate",
                    "",
                    "Cancel",
                    [this, index, item](gui::ConfirmationModalDialog::Result result) {
                        if (result == gui::ConfirmationModalDialog::Result::Primary)
                        {
                            gui::TestConfiguration conf;
                            conf.testName = item.title;
                            conf.stimulusType = item.stimulusType;
                            conf.burstDurationSec = item.burstDurationSec;
                            conf.captureMode = item.captureMode;
                            conf.controls = item.controls;
                            drawer.openTestEditorDrawer(conf, index);
                        }
                    }
                );
            }
            else
            {
                gui::TestConfiguration conf;
                conf.testName = item.title;
                conf.stimulusType = item.stimulusType;
                conf.burstDurationSec = item.burstDurationSec;
                conf.captureMode = item.captureMode;
                conf.controls = item.controls;
                drawer.openTestEditorDrawer(conf, index);
            }
        };

        suiteList.onRequestDeleteTest = [this](int index, const gui::QueueItem& item) {
            promptDeleteTest(index, item);
        };

        drawer.onTestConfigConfirmed = [this](const gui::TestConfiguration& conf, int editingIndex) {
            gui::QueueItem item;
            item.title = conf.testName;
            item.stimulusType = conf.stimulusType;
            item.burstDurationSec = conf.burstDurationSec;
            item.captureMode = conf.captureMode;
            item.controls = conf.controls;
            item.totalPoints = conf.getTotalMeasurementPoints();

            applyBadgeForStimulus(item, conf.stimulusType);

            juce::String formulaStr;
            for (const auto& c : item.controls) {
                if (c.steps > 1) {
                    if (formulaStr.isNotEmpty()) formulaStr += " x ";
                    formulaStr += juce::String(c.steps);
                }
            }
            item.description = juce::String::fromUTF8(u8"Sweep \u2022 ") + formulaStr + " = " + juce::String(item.totalPoints) + " points";
            item.status = gui::QueueItemStatus::Queued;

            if (editingIndex >= 0 && editingIndex < suiteList.getQueueSize())
            {
                item.id = suiteList.getQueue()[static_cast<size_t>(editingIndex)].id;
                item.hwId = suiteList.getQueue()[static_cast<size_t>(editingIndex)].hwId;
                item.funcId = suiteList.getQueue()[static_cast<size_t>(editingIndex)].funcId;
                suiteList.updateTestInQueue(editingIndex, item);
            }
            else
            {
                item.id = "test_" + juce::String(juce::Random::getSystemRandom().nextInt(100000));
                suiteList.addTestToQueue(item);
            }
        };

        suiteList.onRestartTestClicked = [this](int index) {
            suiteList.updateItemStatus(index, gui::QueueItemStatus::Queued, 0);
            startProfilingSession(false);
        };

        suiteList.onContinueTestClicked = [this](int index) {
            juce::ignoreUnused(index);
            startProfilingSession(true);
        };

        suiteList.onSelectPointClicked = [this](int queueIdx, int pointIdx) {
            curvePlotter.setHighlightedPointIndex(pointIdx);

            if (queueIdx >= 0 && queueIdx < static_cast<int>(suiteList.getQueue().size()))
            {
                const auto& item = suiteList.getQueue()[static_cast<size_t>(queueIdx)];
                float stepPct = (item.totalPoints > 1) ? (static_cast<float>(pointIdx) / static_cast<float>(item.totalPoints - 1) * 100.0f) : 0.0f;
                
                juce::String statusStr = (item.status == gui::QueueItemStatus::Completed) ? "Measured" : "Queued";
                juce::String prompt = "Point #" + juce::String(pointIdx + 1) + " / " + juce::String(item.totalPoints) 
                                    + " (" + juce::String(stepPct, 1) + "% Pos) — " + item.title;

                std::vector<core::ParameterStep> pSteps;
                if (static_cast<size_t>(pointIdx) < sessionPoints.size())
                {
                    const auto& pt = sessionPoints[static_cast<size_t>(pointIdx)];
                    prompt += " | Gain: " + juce::String(pt.secondaryValue.mean, 2) + " dB, THD: " + juce::String(pt.thdPercent, 2) + "%, SNR: " + juce::String(pt.snrDb, 1) + " dB";
                    pSteps = pt.controlSteps;

                    if (!pt.irSamples.empty())
                    {
                        double sRate = audioEngine.getCurrentSampleRate();
                        juce::String label = "Point #" + juce::String(pointIdx + 1) + " (" + juce::String(stepPct, 1) + "%)";
                        curvePlotter.getSpectrumAnalyzer().setCapturedSignal(pt.irSamples, sRate, label);
                    }
                }

                if (pSteps.empty())
                {
                    float norm = (item.totalPoints > 1) ? (static_cast<float>(pointIdx) / static_cast<float>(item.totalPoints - 1)) : 0.0f;
                    core::ParameterStep ps;
                    ps.paramName = !item.controls.empty() ? item.controls[0].name.toStdString() : "Parameter 1";
                    ps.normalizedValue = norm;
                    ps.controlType = "Knob";
                    pSteps.push_back(ps);
                }

                suiteList.setVisible(false);
                operatorStepModal.showInspector(item.title, pointIdx + 1, item.totalPoints, pSteps, prompt);
                resized();
            }
        };

        suiteList.onClearPointClicked = [this](int queueIdx, int pointIdx) {
            handleClearPoint(queueIdx, pointIdx);
        };

        suiteList.onDeletePointClicked = [this](int queueIdx, int pointIdx) {
            handleClearPoint(queueIdx, pointIdx);
        };

        suiteList.onToggleSessionRunClicked = [this](bool start) {
            if (start)
            {
                if (!btnHardwareSelector.hasHardwareSelected())
                {
                    suiteList.setSessionRunning(false);
                    drawer.openHardwareDrawer();
                    manualPromptLabel.setText("Please select a Target Hardware device before starting the session.", juce::dontSendNotification);
                    manualPromptLabel.setVisible(true);
                    hidePromptAfterDelay(5000);
                    return;
                }
                startProfilingSession(false);
            }
            else stopProfilingSession();
        };

        suiteList.onDuplicateWarning = [this](const juce::String& msg) {
            manualPromptLabel.setText(msg, juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            hidePromptAfterDelay(4000);
        };
        addAndMakeVisible(suiteList);

        // 6. Right Master Level & Meter Strip
        meterStrip.onProfilingToggled = [this](bool start) {
            if (start)
            {
                if (!btnHardwareSelector.hasHardwareSelected())
                {
                    suiteList.setSessionRunning(false);
                    meterStrip.setProfilingActive(false);
                    drawer.openHardwareDrawer();
                    manualPromptLabel.setText("Please select a Target Hardware device before starting the session.", juce::dontSendNotification);
                    manualPromptLabel.setVisible(true);
                    hidePromptAfterDelay(5000);
                    return;
                }
                startProfilingSession();
            }
            else stopProfilingSession();
        };
        meterStrip.onAutoTrimClicked = [this] {
            audioEngine.performAutoGainTrim();
        };
        addAndMakeVisible(meterStrip);

        // 6. Manual Adjustment Prompt & Operator Correction Controls (Hidden by default)
        manualPromptLabel.setText("", juce::dontSendNotification);
        manualPromptLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        manualPromptLabel.setColour(juce::Label::textColourId, gui::SoundIdTheme::accentAmber);
        manualPromptLabel.setColour(juce::Label::backgroundColourId, gui::SoundIdTheme::bgCard);
        manualPromptLabel.setVisible(false);
        addChildComponent(manualPromptLabel);

        btnStepBack.setButtonText("<- STEP BACK");
        btnStepBack.setTooltip("Return to previous measurement step to redo or adjust physical knob");
        btnStepBack.onClick = [this] { sequencer.stepBack(); };
        btnStepBack.setEnabled(false);
        btnStepBack.setVisible(false);
        addChildComponent(btnStepBack);

        btnRepeatStep.setButtonText("REPEAT STEP");
        btnRepeatStep.setTooltip("Re-measure current knob position in case of audio glitch or misadjustment");
        btnRepeatStep.onClick = [this] { sequencer.repeatCurrentStep(); };
        btnRepeatStep.setEnabled(false);
        btnRepeatStep.setVisible(false);
        addChildComponent(btnRepeatStep);

        confirmManualButton.setButtonText("Confirm Step (Space)");
        confirmManualButton.setTooltip("Confirm Step - Signal the sequencer that hardware control is positioned and proceed with stimulus");
        confirmManualButton.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::pillBlackBg);
        confirmManualButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        confirmManualButton.setEnabled(false);
        confirmManualButton.setVisible(false);
        confirmManualButton.onClick = [this] { confirmManualStep(); };
        addChildComponent(confirmManualButton);

        // 7. Slide-In Drawer & Modals (Overlays on top)
        drawer.onHardwareSelected = [this](const juce::String& hwId, const juce::String& funcId) {
            onHardwareSelected(hwId, funcId);
        };
        drawer.onChangeExportFolderClicked = [this] {
            chooseExportFolder();
        };
        drawer.onOpenAudioSettingsClicked = [this] {
            openAudioMidiSettings();
        };
        drawer.onAboutClicked = [this] {
            showAboutDialog();
        };
        drawer.onNewSessionClicked = [this] {
            promptNewSession();
        };
        drawer.onOpenSessionClicked = [this] {
            handleOpenSession();
        };
        drawer.onSaveSessionClicked = [this] {
            handleSaveSession();
        };
        drawer.onSaveSessionAsClicked = [this] {
            handleSaveSessionAs();
        };
        drawer.onRevealExportFolderClicked = [this] {
            if (!exportDirectory.exists())
                exportDirectory.createDirectory();
            exportDirectory.revealToUser();
        };
        drawer.onExportReportClicked = [this] {
            exportCertificationReport();
        };
        drawer.onExitAppClicked = [this] {
            confirmAndExit();
        };
        drawer.onCheckUpdatesClicked = [this] {
            checkForAppUpdates(true);
        };
        addChildComponent(drawer);

        initializeAutoUpdater();

        loopbackModal.onCalibrationApplied = [this](const math::LoopbackCalibrationData& cal) {
            float gainDb = 20.0f * std::log10(std::max(cal.recommendedTrimGain, 1e-4f));
            juce::String sign = (gainDb >= 0.0f) ? "+" : "";
            juce::String msg = "Loopback Calibration complete! Auto-trim applied: " + 
                               sign + juce::String(gainDb, 1) + " dB (Target: -3.0 dBfs)";
            manualPromptLabel.setText(msg, juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            hidePromptAfterDelay(4000);
        };
        addChildComponent(loopbackModal);
        addChildComponent(aboutModal);
        addChildComponent(operatorStepModal);
        addChildComponent(confirmationModal);

        operatorStepModal.onAccept = [this] { confirmManualStep(); };
        operatorStepModal.onRepeat = [this] { sequencer.repeatCurrentStep(); };
        operatorStepModal.onStepBack = [this] { sequencer.stepBack(); };
        operatorStepModal.onCancel = [this] { stopProfilingSession(); };
        operatorStepModal.onCollapseToggled = [this](bool isCollapsed) {
            juce::ignoreUnused(isCollapsed);
            resized();
        };
        operatorStepModal.onCloseInspector = [this] {
            suiteList.setVisible(true);
            resized();
        };

        sequencer.setOperatorStepCallback([this](const core::TestCase& tc, int stepIndex, int totalSteps) {
            juce::MessageManager::callAsync([this, tc, stepIndex, totalSteps] {
                const auto* contract = contractRegistry.findContractById(drawer.getSelectedHardwareId().toStdString());
                bool isAuto = (contract != nullptr && (contract->deviceType == "AUTOMATED_SYSEX" || contract->deviceType == "AUTOMATED_MIDI_CC"));
                operatorStepModal.setAutomatedMode(isAuto);
                operatorStepModal.setStepInfo(juce::String(tc.testId), stepIndex, totalSteps, tc.parameterSteps);
                suiteList.setVisible(false);
                operatorStepModal.setVisible(true);
            });
        });

        // 8. Progress and Sequencer Callbacks
        sequencer.setProgressCallback([this](float progress, const juce::String& task, core::SequencerState state) {
            juce::MessageManager::callAsync([this, progress, task, state] {
                juce::ignoreUnused(progress);
                if (state == core::SequencerState::WaitingForOperator)
                {
                    operatorStepModal.setMeasuringState(false);
                    manualPromptLabel.setText(task, juce::dontSendNotification);
                    manualPromptLabel.setVisible(true);
                    confirmManualButton.setVisible(true);
                    confirmManualButton.setEnabled(true);
                    btnRepeatStep.setVisible(true);
                    btnRepeatStep.setEnabled(true);
                    btnStepBack.setVisible(totalPointsMeasured > 1);
                    btnStepBack.setEnabled(totalPointsMeasured > 1);
                }
                else if (state == core::SequencerState::CaptureAndAnalyze || state == core::SequencerState::InjectStimulus)
                {
                    operatorStepModal.setMeasuringState(true);
                    confirmManualButton.setEnabled(false);
                    btnRepeatStep.setEnabled(false);
                    btnStepBack.setEnabled(false);
                    manualPromptLabel.setText(task, juce::dontSendNotification);
                    manualPromptLabel.setVisible(true);
                    if (suiteList.getQueueSize() > 0)
                        suiteList.updateItemStatus(0, gui::QueueItemStatus::Running, totalPointsMeasured);
                }
                else if (state == core::SequencerState::Finished)
                {
                    operatorStepModal.dismiss();
                    suiteList.setVisible(true);
                    manualPromptLabel.setVisible(false);
                    confirmManualButton.setVisible(false);
                    btnRepeatStep.setVisible(false);
                    btnStepBack.setVisible(false);
                    meterStrip.setProfilingActive(false);
                    suiteList.setSessionRunning(false);
                    curvePlotter.setMeasuringState(false);
                    for (int i = 0; i < suiteList.getQueueSize(); ++i)
                    {
                        if (!suiteList.getQueue()[static_cast<size_t>(i)].isSkipped)
                            suiteList.updateItemStatus(i, gui::QueueItemStatus::Completed);
                    }
                }
                else if (state == core::SequencerState::ErrorState)
                {
                    manualPromptLabel.setText("Session Halted: Insufficient Audio Signal. Connect patch cable (DAC Out 1 -> ADC In 1) & retry.", juce::dontSendNotification);
                    manualPromptLabel.setVisible(true);
                    meterStrip.setProfilingActive(false);
                    suiteList.setSessionRunning(false);
                    curvePlotter.setMeasuringState(false);
                    confirmManualButton.setVisible(false);
                    btnRepeatStep.setVisible(false);
                    btnStepBack.setVisible(false);
                    if (suiteList.getQueueSize() > 0)
                        suiteList.updateItemStatus(0, gui::QueueItemStatus::Invalidated);
                }
            });
        });

        sequencer.setTestIndexCallback([this](int queueIndex, int currentPoint, int totalPoints) {
            juce::MessageManager::callAsync([this, queueIndex, currentPoint, totalPoints] {
                for (int i = 0; i < queueIndex; ++i)
                {
                    if (!suiteList.getQueue()[static_cast<size_t>(i)].isSkipped &&
                        suiteList.getQueue()[static_cast<size_t>(i)].status != gui::QueueItemStatus::Completed)
                    {
                        suiteList.updateItemStatus(i, gui::QueueItemStatus::Completed);
                    }
                }
                if (queueIndex >= 0 && queueIndex < suiteList.getQueueSize())
                {
                    suiteList.updateItemStatus(queueIndex, gui::QueueItemStatus::Running, currentPoint);
                    float progress = totalPoints > 1 ? (static_cast<float>(currentPoint) / static_cast<float>(totalPoints)) : 0.5f;
                    curvePlotter.setMeasuringState(true, progress);
                }
            });
        });

        sequencer.setPointMeasuredCallback([this](const exporting::MeasuredPoint& pt) {
            curvePlotter.addMeasuredPoint(pt);
            sessionPoints.push_back(pt);
            totalPointsMeasured++;

            float snrDb = pt.muSigmaValue.stdDev > 0.0001f ? (20.0f * std::log10(std::max(1e-4f, pt.muSigmaValue.mean) / pt.muSigmaValue.stdDev)) : 32.0f;
            float noiseDb = (pt.secondaryValue.mean < 0.0f) ? pt.secondaryValue.mean : -92.0f;
            healthPanel.setMeasurementHealth(snrDb, noiseDb, static_cast<int>(sessionPoints.size()), suiteList.getTotalPointCount());
            healthPanel.setLatestTestId(pt.testId);

            isSessionDirty = true;

            // Trigger non-blocking auto-save update
            sessionSerializer.triggerIncrementalAutoSave(buildCurrentSessionManifest(), sessionPoints);
        });

        addKeyListener(this);
        setWantsKeyboardFocus(true);
        audioEngine.getDeviceManager().addChangeListener(this);
        startTimerHz(60);
        setSize(1040, 720);
    }

    ~MainContentComponent() override
    {
        audioEngine.getDeviceManager().removeChangeListener(this);

        if (scopeWebWindow != nullptr)
        {
            scopeWebWindow->setVisible(false);
            scopeWebWindow = nullptr;
        }

        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        setLookAndFeel(nullptr);
        removeKeyListener(this);
        stopTimer();
        sequencer.stopSession();
        audioEngine.saveAudioSettings(settingsFile);
    }

    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        if (source == &audioEngine.getDeviceManager())
        {
            audioMidiStatusPill.updateStatus(audioEngine);
        }
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        bool isCmdOrCtrl = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
        bool isShift = key.getModifiers().isShiftDown();

        if (isCmdOrCtrl && isShift && key.getKeyCode() == 'S')
        {
            handleSaveSessionAs();
            return true;
        }
        else if (isCmdOrCtrl && key.getKeyCode() == 'S')
        {
            handleSaveSession();
            return true;
        }
        else if (isCmdOrCtrl && key.getKeyCode() == 'N')
        {
            promptNewSession();
            return true;
        }
        else if (isCmdOrCtrl && key.getKeyCode() == 'O')
        {
            handleOpenSession();
            return true;
        }
        else if (key.isKeyCode(juce::KeyPress::spaceKey))
        {
            if (confirmManualButton.isEnabled() && confirmManualButton.isVisible())
            {
                confirmManualStep();
                return true;
            }
        }
        return false;
    }

    enum class CenterSplitMode
    {
        QueueMaximized,  // Graph minimized to header (~32px), Queue takes the rest
        Balanced,        // Standard split (Queue ~220px resizable, Graph takes the rest)
        GraphMaximized   // Queue minimized to header (~36px), Graph takes the rest
    };
    CenterSplitMode centerSplitMode { CenterSplitMode::Balanced };

    float currentBottomH { 220.0f };
    float targetBottomH { 220.0f };
    float balancedBottomH { 220.0f };

    void updateSplitLayout()
    {
        switch (centerSplitMode)
        {
            case CenterSplitMode::Balanced:
                curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (pointing down to expand downwards)
                suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (pointing up to expand upwards)
                curvePlotter.setCollapsed(false);
                suiteList.setCollapsed(false);
                targetBottomH = balancedBottomH;
                break;

            case CenterSplitMode::GraphMaximized:
                curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (restore back up to balanced)
                suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25b2")); // ▲ (restore back up to balanced)
                targetBottomH = 36.0f;
                break;

            case CenterSplitMode::QueueMaximized:
                curvePlotter.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (restore back down to balanced)
                suiteList.setChevronGlyph(juce::String::fromUTF8(u8"\u25bc")); // ▼ (restore back down to balanced)
                {
                    auto totalArea = getLocalBounds().reduced(20);
                    // Reserve at least 180px graph + 32px health + 12px splitter + 48px top header
                    int minTopAndGraphH = 48 + 180 + 32 + 12;
                    int maxH = totalArea.getHeight() - minTopAndGraphH;
                    targetBottomH = static_cast<float>(std::max(140, maxH));
                }
                break;
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(gui::SoundIdTheme::bgLight);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);

        // 1. Top Header Area (File and Scope on the left, hardware controls on the right)
        auto topArea = bounds.removeFromTop(36);
        titleLabel.setVisible(false); // Removed ABDAudioLab from top bar per user request

        btnFileMenu.setBounds(topArea.removeFromLeft(68).withHeight(32));
        topArea.removeFromLeft(8);
        btnScope.setBounds(topArea.removeFromLeft(74).withHeight(32));
        topArea.removeFromLeft(8);

        // Audio & MIDI Status Pill with gear and 4 LEDs
        audioMidiStatusPill.setBounds(topArea.removeFromLeft(240).withHeight(32));

        btnInfo.setBounds(topArea.removeFromRight(32).withSizeKeepingCentre(28, 28));
        topArea.removeFromRight(8);

        btnThemeToggle.setBounds(topArea.removeFromRight(36).withHeight(32));
        topArea.removeFromRight(8);

        btnHardwareSelector.setBounds(topArea.removeFromRight(290).withHeight(32));
        topArea.removeFromRight(8);

        btnCalibratePill.setBounds(topArea.removeFromRight(185).withHeight(32));

        bounds.removeFromTop(12);

        // 2. Right Meter Strip
        auto rightArea = bounds.removeFromRight(120);
        meterStrip.setBounds(rightArea);
        bounds.removeFromRight(12);

        // 3. Manual Prompt Banner & Error Correction Controls
        if (confirmManualButton.isVisible())
        {
            auto manualRow = bounds.removeFromBottom(36);
            confirmManualButton.setBounds(manualRow.removeFromRight(150));
            manualRow.removeFromRight(8);
            btnRepeatStep.setBounds(manualRow.removeFromRight(100));
            manualRow.removeFromRight(8);
            btnStepBack.setBounds(manualRow.removeFromRight(100));
            manualRow.removeFromRight(8);
            manualPromptLabel.setBounds(manualRow);
            bounds.removeFromBottom(8);
        }

        // 4. Bottom Test Suite List & Controls Dock
        int bottomH = static_cast<int>(std::round(currentBottomH));
        if (operatorStepModal.isVisible() && operatorStepModal.isCollapsed)
        {
            bottomH = 34;
        }
        else
        {
            int minGraphAreaH = 180 + 32 + 12; // 180px curvePlotter + 32px healthPanel + 12px splitter
            int maxBottomH = bounds.getHeight() - minGraphAreaH;
            bottomH = juce::jlimit(36, std::max(36, maxBottomH), bottomH);
        }

        auto bottomArea = bounds.removeFromBottom(bottomH);
        suiteList.setBounds(bottomArea);
        operatorStepModal.setBounds(bottomArea);

        // Resizable Splitter Bar
        int splitterH = 8;
        if (centerSplitMode == CenterSplitMode::Balanced &&
            (!operatorStepModal.isVisible() || !operatorStepModal.isCollapsed) &&
            std::abs(targetBottomH - currentBottomH) < 2.0f)
        {
            centerSplitterBar.setVisible(true);
            centerSplitterBar.setBounds(bounds.removeFromBottom(splitterH));
            bounds.removeFromBottom(4);
        }
        else
        {
            centerSplitterBar.setVisible(false);
            bounds.removeFromBottom(8);
        }

        // 5. Center Curve Plotter & Health Panel
        auto centerArea = bounds;
        healthPanel.setBounds(centerArea.removeFromTop(26));
        centerArea.removeFromTop(6);
        curvePlotter.setBounds(centerArea);

        // 6. Slide-in Drawer & Modals fill full window bounds
        drawer.setBounds(getLocalBounds());
        aboutModal.setBounds(getLocalBounds());
        confirmationModal.setBounds(getLocalBounds());
    }

    void timerCallback() override
    {
        meterStrip.setLevels(audioEngine.getInputPeakL(), audioEngine.getInputPeakR(), audioEngine.getInputRmsL(),
                             audioEngine.getOutputPeakL(), audioEngine.getOutputPeakR(), audioEngine.getOutputRmsL());

        if (audioEngine.isSpectrumReady())
        {
            std::array<float, audio::LabAudioEngine::kSpectrumBins> fftData;
            audioEngine.getSpectrumMagnitudes(fftData);
            curvePlotter.getSpectrumAnalyzer().pushSpectrumData(fftData, audioEngine.getCurrentSampleRate());
        }

        if ((statusUpdateCounter++ % 15) == 0)
        {
            if (loopbackModal.getCalibrationData().isCalibrated)
            {
                if (std::abs(loopbackModal.getCalibrationData().sampleRate - audioEngine.getCurrentSampleRate()) < 1.0)
                {
                    btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"● CAL: VALID (-3.0 dBFS)"));
                    btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::accentGreen);
                }
                else
                {
                    bool blink = ((statusUpdateCounter / 15) % 2) == 0;
                    btnCalibratePill.setButtonText(juce::String::fromUTF8(u8"⚠ CAL: RECALIBRATE (SR CHANGED)"));
                    btnCalibratePill.setColour(juce::TextButton::textColourOffId, blink ? gui::SoundIdTheme::accentAmber : gui::SoundIdTheme::textMuted);
                }
            }
        }

        // Slower, smooth and relaxed chevron/split animation
        if (std::abs(targetBottomH - currentBottomH) > 0.5f)
        {
            float diff = targetBottomH - currentBottomH;
            float step = diff * 0.07f;
            if (std::abs(step) < 0.35f)
                step = (diff > 0.0f ? 0.35f : -0.35f);

            if (std::abs(diff) <= std::abs(step))
                currentBottomH = targetBottomH;
            else
                currentBottomH += step;

            resized();
        }
        else if (currentBottomH != targetBottomH)
        {
            currentBottomH = targetBottomH;
            if (centerSplitMode == CenterSplitMode::GraphMaximized)
                suiteList.setCollapsed(true);
            else if (centerSplitMode == CenterSplitMode::QueueMaximized)
                curvePlotter.setCollapsed(true);
            resized();
        }

        if (++statusUpdateCounter % 60 == 0)
        {
            audioMidiStatusPill.updateStatus(audioEngine);
        }
    }

private:
    void showFilePopupMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(1, "New Session\tCtrl+N");
        menu.addItem(2, "Open Session...\tCtrl+O");
        menu.addItem(3, "Save Session\tCtrl+S");
        menu.addItem(4, "Save Session As...\tCtrl+Shift+S");
        menu.addSeparator();
        menu.addItem(5, "Export Certification Report (PDF/HTML)...");
        menu.addItem(6, "Open Export Folder");
        menu.addSeparator();
        menu.addItem(7, "Exit ABDAudioLab");

        juce::Component::SafePointer<MainContentComponent> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnFileMenu),
                           [safeThis](int result) {
            if (safeThis == nullptr || result == 0) return;
            switch (result)
            {
                case 1: safeThis->promptNewSession(); break;
                case 2: safeThis->handleOpenSession(); break;
                case 3: safeThis->handleSaveSession(); break;
                case 4: safeThis->handleSaveSessionAs(); break;
                case 5: safeThis->exportCertificationReport(); break;
                case 6:
                    if (!safeThis->exportDirectory.exists())
                        safeThis->exportDirectory.createDirectory();
                    safeThis->exportDirectory.revealToUser();
                    break;
                case 7: safeThis->confirmAndExit(); break;
                default: break;
            }
        });
    }

    void toggleScopeWebWindow()
    {
        if (scopeWebWindow == nullptr)
        {
            scopeWebWindow = std::make_unique<gui::ScopeWebFloatingWindow>(
                audioEngine,
                [this] {
                    audioEngine.getScopeCollector().deactivateAll();
                }
            );
        }

        if (scopeWebWindow->isVisible())
        {
            scopeWebWindow->toFront(true);
        }
        else
        {
            scopeWebWindow->setVisible(true);
            scopeWebWindow->toFront(true);
            scopeWebWindow->onWindowShown();
        }
    }



    void showInfoDrawer()
    {
        gui::TelemetryInfo info;
        auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
        if (device != nullptr)
        {
            info.audioDeviceName = device->getName();
            info.sampleRate = device->getCurrentSampleRate();
            info.bufferSize = device->getCurrentBufferSizeSamples();
            info.latencyMs = (info.sampleRate > 0) ? (static_cast<double>(info.bufferSize) * 1000.0 / info.sampleRate) : 0.0;
        }
        else
        {
            info.audioDeviceName = "Windows Audio (Default)";
            info.sampleRate = 96000.0;
            info.bufferSize = 256;
            info.latencyMs = 2.67;
        }

        auto midiInputs = juce::MidiInput::getAvailableDevices();
        if (!midiInputs.isEmpty())
            info.midiInputName = midiInputs[0].name;

        auto midiOutputs = juce::MidiOutput::getAvailableDevices();
        if (!midiOutputs.isEmpty())
            info.midiOutputName = midiOutputs[0].name;

        info.exportDirectoryPath = exportDirectory.getFullPathName();
        info.autoTrimGainDb = (audioEngine.getInputAutoTrim() > 1e-4f) ? (20.0f * std::log10(audioEngine.getInputAutoTrim())) : 0.0f;
        info.totalMeasuredPoints = totalPointsMeasured;
        info.appVersion = version::kAppVersion;
        info.buildNumber = version::kBuildNumber;

        drawer.openSetupDrawer(info);
    }

    void chooseExportFolder()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Output Directory for C++ Look-Up Tables & Reports",
            exportDirectory,
            "*"
        );

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                auto chosen = fc.getResult();
                if (chosen.isDirectory())
                {
                    exportDirectory = chosen;
                    drawer.openFileDrawer(exportDirectory.getFullPathName());
                    manualPromptLabel.setText("Target export folder updated to: " + exportDirectory.getFullPathName(), juce::dontSendNotification);
                    manualPromptLabel.setVisible(true);
                    hidePromptAfterDelay(5000);
                }
            });
    }

    void onHardwareSelected(const juce::String& hwId, const juce::String& funcId)
    {
        juce::ignoreUnused(funcId);
        const auto* contract = contractRegistry.findContractById(hwId.toStdString());
        if (contract == nullptr) return;

        gui::HardwareConnectionStatus connStatus = gui::HardwareConnectionStatus::NotApplicable;

        if (contract->deviceType == "MOCK_DSP")
        {
            audioEngine.setMockHardware(&mockController);
            connStatus = gui::HardwareConnectionStatus::NotApplicable;
        }
        else if (contract->deviceType == "AUTOMATED_SYSEX")
        {
            audioEngine.setMockHardware(nullptr);
            hardware::AiraModel model = mapHardwareIdToAiraModel(contract->id);
            airaController = std::make_unique<hardware::AiraSysExController>(model);
            bool connected = airaController->connect();
            connStatus = connected ? gui::HardwareConnectionStatus::Connected : gui::HardwareConnectionStatus::Disconnected;
        }
        else if (contract->deviceType == "AUTOMATED_MIDI_CC")
        {
            audioEngine.setMockHardware(nullptr);
            if (midiCcController == nullptr)
                midiCcController = std::make_unique<hardware::MidiCcController>();

            juce::String keyword = juce::String(contract->displayName);
            if (keyword.containsIgnoreCase("DeepMind")) keyword = "DeepMind";
            else if (keyword.containsIgnoreCase("MS2000")) keyword = "MS2000";
            else if (keyword.containsIgnoreCase("CZ-101") || keyword.containsIgnoreCase("CZ101")) keyword = "CZ";
            else if (keyword.containsIgnoreCase("PRO-800") || keyword.containsIgnoreCase("PRO800")) keyword = "PRO-800";
            else if (keyword.containsIgnoreCase("Bass Station") || keyword.containsIgnoreCase("BassStation")) keyword = "Bass Station";

            midiCcController->setTargetDeviceIdentifier(keyword);
            bool connected = midiCcController->connect();
            connStatus = connected ? gui::HardwareConnectionStatus::Connected : gui::HardwareConnectionStatus::Disconnected;
        }
        else if (contract->deviceType == "MANUAL_EURORACK" || contract->deviceType == "ANALOGUE_PEDAL")
        {
            audioEngine.setMockHardware(nullptr);
            manualController.connect();
            connStatus = gui::HardwareConnectionStatus::NotApplicable;
        }

        btnHardwareSelector.setHardwareInfo(
            juce::String(contract->displayName),
            drawer.getActiveFunctionDisplayName(),
            drawer.getActiveModelRasterImage(),
            connStatus
        );
    }

    void hidePromptAfterDelay(int delayMs = 4000)
    {
        juce::Component::SafePointer<MainContentComponent> safeThis(this);
        juce::Timer::callAfterDelay(delayMs, [safeThis] {
            if (safeThis != nullptr && safeThis->sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
                safeThis->manualPromptLabel.setVisible(false);
        });
    }

    void handleClearPoint(int queueIdx, int pointIdx)
    {
        curvePlotter.removePoint(pointIdx);
        if (pointIdx >= 0 && pointIdx < static_cast<int>(sessionPoints.size()))
            sessionPoints.erase(sessionPoints.begin() + pointIdx);

        suiteList.setPointStatus(queueIdx, pointIdx, gui::PointStatus::Annulled);
        manualPromptLabel.setText("Point #" + juce::String(pointIdx + 1) + " marked as ANNULLED.", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(3500);
    }

    void startProfilingSession(bool resumeFromExisting = false)
    {
        if (sequencer.isRunningSession())
            return;

        if (suiteList.getQueueSize() <= 0)
        {
            manualPromptLabel.setText("Please add at least one test to the Session Plan before starting.", juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            meterStrip.setProfilingActive(false);
            return;
        }

        if (!resumeFromExisting)
        {
            suiteList.resetAllStatuses();
            suiteList.updateItemStatus(0, gui::QueueItemStatus::Running, 0);
            curvePlotter.clear();
            sessionPoints.clear();
            totalPointsMeasured = 0;
        }

        suiteList.setSessionRunning(true);

        juce::String selectedHwId = drawer.getSelectedHardwareId();
        juce::String selectedFuncId = drawer.getSelectedFunctionId();
        const auto* contract = contractRegistry.findContractById(selectedHwId.toStdString());

        hardware::IHardwareController* activeHw = &mockController;
        std::string modeStr = "MOCK_DSP";
        std::string hwName = "MOCK_VA_SYNTH";

        if (contract != nullptr)
        {
            modeStr = contract->deviceType;
            hwName = contract->id;

            if (contract->deviceType == "AUTOMATED_SYSEX")
            {
                hardware::AiraModel model = mapHardwareIdToAiraModel(contract->id);
                airaController = std::make_unique<hardware::AiraSysExController>(model);
                airaController->connect();
                activeHw = airaController.get();
            }
            else if (contract->deviceType == "AUTOMATED_MIDI_CC")
            {
                if (midiCcController == nullptr)
                    midiCcController = std::make_unique<hardware::MidiCcController>();
                midiCcController->connect();
                activeHw = midiCcController.get();
            }
            else if (contract->deviceType == "MANUAL_EURORACK" || contract->deviceType == "ANALOGUE_PEDAL")
            {
                manualController.connect();
                activeHw = &manualController;
            }
        }

        if (contract != nullptr && contract->deviceType == "MOCK_DSP")
        {
            audioEngine.setMockHardware(&mockController);
        }
        else
        {
            audioEngine.setMockHardware(nullptr);
        }

        sequencer.setHardwareController(activeHw);

        core::ProfilingSession currentProfilingSession = buildProfilingSessionFromQueue(hwName, modeStr);
        juce::String baseName = juce::String(hwName) + "_" + selectedFuncId;

        if (resumeFromExisting && !sessionPoints.empty())
        {
            auto allTestCases = currentProfilingSession.getTestCases();
            size_t alreadyMeasured = sessionPoints.size();
            if (alreadyMeasured < allTestCases.size())
            {
                std::vector<core::TestCase> remainingTestCases(allTestCases.begin() + alreadyMeasured, allTestCases.end());
                currentProfilingSession.setTestCases(remainingTestCases);
            }
        }

        meterStrip.setProfilingActive(true);
        sequencer.startSession(currentProfilingSession, exportDirectory, baseName);
    }

    void stopProfilingSession()
    {
        sequencer.stopSession();
        meterStrip.setProfilingActive(false);
        suiteList.setSessionRunning(false);
        confirmManualButton.setVisible(false);
        btnStepBack.setVisible(false);
        btnRepeatStep.setVisible(false);
        manualPromptLabel.setVisible(false);
        operatorStepModal.setVisible(false);
        suiteList.setVisible(true);

        // If stopped midway, mark the running item as Incomplete with measured points
        for (int i = 0; i < suiteList.getQueueSize(); ++i)
        {
            if (suiteList.getQueue()[static_cast<size_t>(i)].status == gui::QueueItemStatus::Running)
            {
                suiteList.updateItemStatus(i, gui::QueueItemStatus::Incomplete, totalPointsMeasured);
                break;
            }
        }

        resized();
    }

    void confirmManualStep()
    {
        sequencer.confirmOperatorStep();
        confirmManualButton.setEnabled(false);
    }

    void openAudioMidiSettings()
    {
        auto* selector = new juce::AudioDeviceSelectorComponent(
            audioEngine.getDeviceManager(),
            0, 2,
            0, 2,
            true,
            true,
            false,
            false
        );
        selector->setLookAndFeel(&soundIdTheme);
        selector->setSize(520, 520);

        juce::DialogWindow::LaunchOptions opt;
        opt.content.setOwned(selector);
        opt.dialogTitle = "Audio & MIDI Configuration";
        opt.dialogBackgroundColour = gui::SoundIdTheme::bgLight;
        opt.escapeKeyTriggersCloseButton = true;
        opt.useNativeTitleBar = true;
        opt.resizable = false;
        opt.launchAsync();
    }

    void showAboutDialog()
    {
        if (aboutSplashWindow == nullptr)
        {
            aboutSplashWindow = std::make_unique<gui::SoundIdSplashWindow>(true);
            aboutSplashWindow->setStatus("System Ready \u2022 All Research Contracts Operational", 1.0f);
            aboutSplashWindow->onCloseRequest = [this] {
                aboutSplashWindow.reset();
            };
        }
        else
        {
            aboutSplashWindow->toFront(true);
        }
    }

    core::ProfilingSession buildProfilingSessionFromQueue(const std::string& hwName, const std::string& modeStr)
    {
        core::ProfilingSession profSession;
        core::ProfilingMetadata meta;
        meta.hardwareName = hwName;
        meta.targetModule = drawer.getSelectedFunctionId().toStdString();
        meta.operatorMode = modeStr;
        meta.sampleRate = audioEngine.getSampleRate();
        meta.bitDepth = 24;
        meta.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
        profSession.setMetadata(meta);

        const auto& queue = suiteList.getQueue();
        for (int qIdx = 0; qIdx < static_cast<int>(queue.size()); ++qIdx)
        {
            const auto& item = queue[static_cast<size_t>(qIdx)];
            if (item.isSkipped) continue;

            if (item.stimulusType == audio::StimulusType::Silence)
            {
                core::TestCase tc;
                tc.queueItemIndex = qIdx;
                tc.pointIndexInTest = 1;
                tc.totalPointsInTest = 1;
                tc.testId = item.title.toStdString();
                tc.functionalBlockType = "NoiseFloor";
                tc.stimulusType = audio::StimulusType::Silence;
                tc.stimulusDurationSec = (item.burstDurationSec > 0.1f) ? item.burstDurationSec : 0.8;
                tc.numPasses = 1;
                tc.stabilizationWaitMs = 50.0;
                profSession.addTestCase(tc);
                continue;
            }

            size_t numControls = item.controls.size();
            if (numControls == 0)
            {
                core::TestCase tc;
                tc.queueItemIndex = qIdx;
                tc.pointIndexInTest = 1;
                tc.totalPointsInTest = 1;
                tc.testId = item.title.toStdString();
                tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
                tc.stimulusType = item.stimulusType;
                tc.stimulusDurationSec = item.burstDurationSec;
                tc.startFreqHz = 20.0f;
                tc.endFreqHz = 20000.0f;
                tc.numPasses = 1;
                tc.stabilizationWaitMs = 50.0;
                profSession.addTestCase(tc);
                continue;
            }

            std::vector<int> stepsPerControl(numControls);
            std::vector<std::string> controlNames(numControls);
            std::vector<std::string> controlTypes(numControls);
            std::vector<float> minNorms(numControls);
            std::vector<float> maxNorms(numControls);

            int totalTestPoints = 1;
            for (size_t k = 0; k < numControls; ++k)
            {
                const auto& c = item.controls[k];
                stepsPerControl[k] = std::max(1, c.steps);
                controlNames[k] = c.name.toStdString();
                controlTypes[k] = c.type.isEmpty() ? "Knob" : c.type.toStdString();
                minNorms[k] = std::clamp(c.minPct / 100.0f, 0.0f, 1.0f);
                maxNorms[k] = std::clamp(c.maxPct / 100.0f, minNorms[k], 1.0f);
                totalTestPoints *= stepsPerControl[k];
            }

            for (int p = 0; p < totalTestPoints; ++p)
            {
                int temp = p;
                std::vector<int> stepIndices(numControls);
                for (int k = static_cast<int>(numControls) - 1; k >= 0; --k)
                {
                    stepIndices[static_cast<size_t>(k)] = temp % stepsPerControl[static_cast<size_t>(k)];
                    temp /= stepsPerControl[static_cast<size_t>(k)];
                }

                core::TestCase tc;
                tc.queueItemIndex = qIdx;
                tc.pointIndexInTest = p + 1;
                tc.totalPointsInTest = totalTestPoints;
                tc.testId = item.title.toStdString();

                tc.functionalBlockType = mapBadgeToBlockType(item.badgeText);
                tc.stimulusType = item.stimulusType;
                tc.stimulusDurationSec = item.burstDurationSec;
                tc.startFreqHz = 20.0f;
                tc.endFreqHz = 20000.0f;
                tc.numPasses = 1;
                tc.stabilizationWaitMs = 50.0;

                for (size_t k = 0; k < numControls; ++k)
                {
                    int stepIdx = stepIndices[k];
                    int sCount = stepsPerControl[k];
                    float minN = minNorms[k];
                    float maxN = maxNorms[k];

                    float normVal = (sCount > 1)
                        ? (minN + (static_cast<float>(stepIdx) / static_cast<float>(sCount - 1)) * (maxN - minN))
                        : (minN + maxN) * 0.5f;

                    int rawVal = static_cast<int>(std::round(normVal * 127.0f));

                    core::ParameterStep ps;
                    ps.paramIndex = static_cast<int>(k) + 1;
                    ps.paramName = controlNames[k];
                    ps.controlType = controlTypes[k];
                    ps.minNormalized = minN;
                    ps.maxNormalized = maxN;
                    ps.normalizedValue = normVal;
                    ps.rawValue = rawVal;
                    ps.id = item.controls[k].id.isNotEmpty() ? item.controls[k].id.toStdString() : ("ctrl_" + std::to_string(k + 1));
                    ps.sortOrder = item.controls[k].sortOrder;
                    tc.parameterSteps.push_back(ps);
                }

                profSession.addTestCase(tc);
            }
        }
        return profSession;
    }

    core::SessionManifest buildCurrentSessionManifest()
    {
        core::SessionManifest sm;
        sm.appVersion = version::kAppVersion;
        sm.buildNumber = version::kBuildNumber;
        sm.formatVersion = "1.0";
        sm.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
        sm.hardwareId = drawer.getSelectedHardwareId().toStdString();
        sm.hardwareDisplayName = drawer.getActiveHardwareDisplayName().toStdString();
        sm.activeFunctionId = drawer.getSelectedFunctionId().toStdString();
        sm.activeFunctionName = drawer.getActiveFunctionDisplayName().toStdString();
        sm.sampleRate = audioEngine.getSampleRate();
        sm.lineCalibrationGainDb = -3.0f;
        sm.noiseFloorThresholdDb = -85.0f;
        sm.totalMeasuredPoints = totalPointsMeasured;

        for (const auto& item : suiteList.getQueue())
        {
            gui::TestConfiguration tc;
            tc.testName = item.title;
            tc.stimulusType = item.stimulusType;
            tc.burstDurationSec = item.burstDurationSec;
            tc.captureMode = item.captureMode;
            tc.controls = item.controls;
            sm.tests.push_back(tc);
        }
        return sm;
    }

    void applyLoadedSession(const core::SessionManifest& manifest, const std::vector<exporting::MeasuredPoint>& points)
    {
        totalPointsMeasured = 0;
        sessionPoints = points;
        curvePlotter.clear();
        for (const auto& pt : points)
        {
            curvePlotter.addMeasuredPoint(pt);
            totalPointsMeasured++;
        }

        // Apply Hardware & Function
        drawer.setSelectedHardwareId(juce::String(manifest.hardwareId));
        drawer.setHardwareLocked(true);

        const auto* contract = contractRegistry.findContractById(manifest.hardwareId);
        if (contract != nullptr)
        {
            onHardwareSelected(juce::String(manifest.hardwareId), juce::String(manifest.activeFunctionId));
        }

        // Apply Test Queue
        suiteList.clearQueue();
        for (const auto& tc : manifest.tests)
        {
            gui::QueueItem item;
            item.title = tc.testName;
            item.stimulusType = tc.stimulusType;
            item.burstDurationSec = tc.burstDurationSec;
            item.captureMode = tc.captureMode;
            item.controls = tc.controls;
            item.totalPoints = tc.getTotalMeasurementPoints();
            item.id = "test_" + juce::String(juce::Random::getSystemRandom().nextInt(100000));
            item.status = (!points.empty()) ? gui::QueueItemStatus::Completed : gui::QueueItemStatus::Queued;

            applyBadgeForStimulus(item, tc.stimulusType);

            suiteList.addTestToQueue(item);
        }

        manualPromptLabel.setText("Session loaded: " + juce::String(manifest.hardwareDisplayName) + " (" + juce::String(points.size()) + " points)", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
    }

    void handleSaveSession()
    {
        if (sessionSerializer.getActiveSessionFile().existsAsFile())
        {
            saveSessionToFile(sessionSerializer.getActiveSessionFile());
            return;
        }
        handleSaveSessionAs();
    }

    void handleSaveSessionAs()
    {
        juce::String defaultName = drawer.getActiveHardwareDisplayName().replaceCharacter(' ', '_') + ".abdlabtest";
        fileChooser = std::make_unique<juce::FileChooser>(
            "Save ABDAudioLab Session Package (.abdlabtest)...",
            exportDirectory.getChildFile(defaultName),
            "*.abdlabtest"
        );
        auto dialogFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
        fileChooser->launchAsync(dialogFlags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File())
            {
                if (file.getFileExtension() != ".abdlabtest")
                    file = file.withFileExtension(".abdlabtest");

                saveSessionToFile(file);
            }
        });
    }

    void saveSessionToFile(const juce::File& file)
    {
        core::SessionManifest manifest = buildCurrentSessionManifest();
        bool ok = sessionSerializer.saveSessionToPackage(file, manifest, sessionPoints);
        if (ok)
        {
            isSessionDirty = false;

            // Synchronize & export C++ LUT header & JSON report alongside save to keep dev assets in sync
            juce::String baseName = file.getFileNameWithoutExtension();
            juce::File headerFile = exportDirectory.getChildFile(baseName + "_LUT.h");
            juce::File jsonFile = exportDirectory.getChildFile(baseName + "_Report.json");

            core::ProfilingMetadata meta;
            meta.hardwareName = drawer.getActiveHardwareDisplayName().toStdString();
            meta.targetModule = drawer.getSelectedFunctionId().toStdString();
            meta.sampleRate = audioEngine.getCurrentSampleRate();
            meta.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();

            exporting::LutExporter::exportToCppHeader(headerFile.getFullPathName().toStdString(),
                                                      meta,
                                                      baseName.toStdString(),
                                                      sessionPoints);

            exporting::LutExporter::exportToJsonReport(jsonFile.getFullPathName().toStdString(),
                                                       meta,
                                                       sessionPoints);

            manualPromptLabel.setText("Session package saved successfully: " + file.getFileName(), juce::dontSendNotification);
            manualPromptLabel.setVisible(true);

            // Update file drawer preview
            drawer.openFileDrawer(exportDirectory.getFullPathName());
            hidePromptAfterDelay(4000);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Save Failed",
                "Could not write session package to:\n" + file.getFullPathName() + "\n\nPlease check disk space and folder permissions.",
                "OK"
            );
        }
    }

    void exportCertificationReport()
    {
        if (!exportDirectory.exists())
            exportDirectory.createDirectory();

        juce::String hwId = drawer.getSelectedHardwareId();
        juce::String funcId = drawer.getSelectedFunctionId();
        juce::String baseName = (hwId.isNotEmpty() ? hwId : "hardware").toLowerCase() + "_" + (funcId.isNotEmpty() ? funcId : "profile").toLowerCase();

        juce::File htmlFile = exportDirectory.getChildFile(baseName + "_Certification_Report.html");

        exporting::SessionManifestData manifest;
        manifest.hardwareId = hwId.toStdString();
        manifest.hardwareName = drawer.getActiveHardwareDisplayName().toStdString();
        manifest.functionId = funcId.toStdString();
        manifest.functionName = drawer.getActiveFunctionDisplayName().toStdString();
        manifest.deviceType = hwId.containsIgnoreCase("AIRA") ? "AUTOMATED_SYSEX" : "MANUAL_EURORACK";
        manifest.sampleRate = audioEngine.getCurrentSampleRate();

        bool success = exporting::CertificationReportExporter::exportReportToHtml(
            htmlFile.getFullPathName().toStdString(),
            manifest,
            sessionPoints
        );

        if (success)
        {
            manualPromptLabel.setText("Certification Report exported: " + htmlFile.getFileName(), juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            drawer.openFileDrawer(exportDirectory.getFullPathName());
            htmlFile.startAsProcess();
            hidePromptAfterDelay(5000);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Export Failed",
                "Could not generate HTML Certification Report at:\n" + htmlFile.getFullPathName(),
                "OK"
            );
        }
    }

    void promptNewSession()
    {
        if (isSessionDirty && !sessionPoints.empty())
        {
            confirmationModal.show(
                this,
                "Unsaved Changes",
                "The current session contains unsaved measurement points.\nDo you want to save before creating a new session?",
                "Save",
                "Don't Save",
                "Cancel",
                [this](gui::ConfirmationModalDialog::Result result) {
                    if (result == gui::ConfirmationModalDialog::Result::Primary)
                    {
                        handleSaveSession();
                        performNewSessionReset();
                    }
                    else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                    {
                        performNewSessionReset();
                    }
                }
            );
        }
        else
        {
            performNewSessionReset();
        }
    }

    void performNewSessionReset()
    {
        suiteList.clearQueue();
        curvePlotter.clear();
        sessionPoints.clear();
        totalPointsMeasured = 0;
        isSessionDirty = false;
        sessionSerializer.cleanupTempSession();
        drawer.setHardwareLocked(false);
        drawer.clearSelectedHardware();
        btnHardwareSelector.clearHardware();
        drawer.openHardwareDrawer();
        manualPromptLabel.setText("New session initialized. Select hardware and active submodule, then click Accept.", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(4000);
    }

    void handleOpenSession()
    {
        if (isSessionDirty && !sessionPoints.empty())
        {
            confirmationModal.show(
                this,
                "Unsaved Changes",
                "The current session contains unsaved measurement points.\nDo you want to save before opening another session?",
                "Save",
                "Don't Save",
                "Cancel",
                [this](gui::ConfirmationModalDialog::Result result) {
                    if (result == gui::ConfirmationModalDialog::Result::Primary)
                    {
                        handleSaveSession();
                        performOpenSessionFileChooser();
                    }
                    else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                    {
                        performOpenSessionFileChooser();
                    }
                }
            );
        }
        else
        {
            performOpenSessionFileChooser();
        }
    }

    void performOpenSessionFileChooser()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Open ABDAudioLab Session Package (.abdlabtest)...",
            exportDirectory,
            "*.abdlabtest;*.json"
        );
        auto dialogFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(dialogFlags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                core::SessionManifest manifest;
                std::vector<exporting::MeasuredPoint> points;
                juce::String err;
                if (sessionSerializer.loadSessionFromPackage(file, manifest, points, err))
                {
                    applyLoadedSession(manifest, points);
                    isSessionDirty = false;
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Failed to Open Session",
                        err,
                        "OK"
                    );
                }
            }
        });
    }

    void promptDeleteTest(int index, const gui::QueueItem& item)
    {
        if (item.status == gui::QueueItemStatus::Completed || item.status == gui::QueueItemStatus::Incomplete)
        {
            confirmationModal.show(
                this,
                "Delete Measured Test",
                "Test '" + item.title + "' contains recorded measurement data.\n\nWhat would you like to do?",
                "Discard & Delete",
                "Mark as Invalid (Keep)",
                "Cancel",
                [this, index](gui::ConfirmationModalDialog::Result result) {
                    if (result == gui::ConfirmationModalDialog::Result::Primary)
                    {
                        suiteList.removeTestDirectly(index);
                        isSessionDirty = true;
                    }
                    else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                    {
                        suiteList.invalidateTest(index);
                        isSessionDirty = true;
                    }
                }
            );
        }
        else
        {
            suiteList.removeTestDirectly(index);
            isSessionDirty = true;
        }
    }

    void confirmAndExit()
    {
        if (isSessionDirty && !sessionPoints.empty())
        {
            confirmationModal.show(
                this,
                "Exit ABDAudioLab",
                "You have unsaved measurement points in this session. Do you want to save before exiting?",
                "Save and Exit",
                "Exit Without Saving",
                "Cancel",
                [this](gui::ConfirmationModalDialog::Result result) {
                    if (result == gui::ConfirmationModalDialog::Result::Primary)
                    {
                        handleSaveSession();
                        juce::JUCEApplication::getInstance()->systemRequestedQuit();
                    }
                    else if (result == gui::ConfirmationModalDialog::Result::Secondary)
                    {
                        juce::JUCEApplication::getInstance()->systemRequestedQuit();
                    }
                }
            );
        }
        else
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    }

    void initializeAutoUpdater()
    {
        autoUpdater = std::make_unique<ABDShared::AutoUpdater>(config::getAutoUpdaterConfig());
        autoUpdater->setUpdateCallback([this](const ABDShared::AutoUpdater::UpdateInfo& info, bool isManualCheck) {
            juce::MessageManager::callAsync([this, info, isManualCheck]() {
                if (autoUpdater->isUpdateAvailable())
                {
                    juce::String msg = "A new version of ABDAudioLab is available!\n\n"
                                       "Latest Version: " + info.version + "\n"
                                       "Current Version: " + autoUpdater->getCurrentVersion() + "\n\n"
                                       "Would you like to open the GitHub releases download page?";

                    confirmationModal.show(
                        this,
                        "New Update Available",
                        msg,
                        "Download Update",
                        "",
                        "Later",
                        [info](gui::ConfirmationModalDialog::Result result) {
                            if (result == gui::ConfirmationModalDialog::Result::Primary)
                            {
                                if (info.downloadUrl.isNotEmpty())
                                    juce::URL(info.downloadUrl).launchInDefaultBrowser();
                                else
                                    juce::URL("https://github.com/ajabadia/ABDAudioLab/releases").launchInDefaultBrowser();
                            }
                        }
                    );
                }
                else if (isManualCheck)
                {
                    confirmationModal.show(
                        this,
                        "Up to Date",
                        "You are currently running the latest release of ABDAudioLab (v" + autoUpdater->getCurrentVersion() + ").",
                        "OK",
                        "",
                        "",
                        [](gui::ConfirmationModalDialog::Result) {}
                    );
                }
            });
        });

        // Run background check on launch
        autoUpdater->checkForUpdates(false);
    }

    void checkForAppUpdates(bool isManual)
    {
        if (autoUpdater != nullptr)
        {
            autoUpdater->checkForUpdates(isManual);
        }
    }

    // Engine & Controllers
    audio::LabAudioEngine audioEngine;
    core::HardwareContractRegistry contractRegistry;
    std::unique_ptr<ABDShared::AutoUpdater> autoUpdater;
    hardware::MockHardwareController mockController;
    std::unique_ptr<hardware::AiraSysExController> airaController;
    std::unique_ptr<hardware::MidiCcController> midiCcController;
    hardware::ManualAnalogueController manualController;
    core::ProfilingSequencer sequencer;
    core::SessionSerializer sessionSerializer;
    std::vector<exporting::MeasuredPoint> sessionPoints;

    gui::SoundIdTheme soundIdTheme;
    juce::TooltipWindow tooltipWindow { this, 400 };

    // Files & Directories
    juce::File settingsFile;
    juce::File exportDirectory;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int totalPointsMeasured { 0 };
    bool isSessionDirty { false };

    // UI Widgets & Visualizers
    juce::Label titleLabel;
    juce::TextButton btnFileMenu;
    juce::TextButton btnScope;
    gui::AudioMidiStatusPill audioMidiStatusPill;
    juce::TextButton btnCalibratePill;
    gui::HardwareSelectorPill btnHardwareSelector;
    ThemeToggleButton btnThemeToggle;
    MonochromeInfoButton btnInfo;
    int statusUpdateCounter { 0 };

    std::unique_ptr<gui::ScopeWebFloatingWindow> scopeWebWindow;
    std::unique_ptr<gui::SoundIdSplashWindow> aboutSplashWindow;

    gui::SoundIdCurvePlotter curvePlotter;
    CenterSplitterBar centerSplitterBar;
    gui::MeasurementHealthPanel healthPanel;
    gui::SoundIdMeterStrip meterStrip;
    gui::SoundIdSuiteList suiteList;
    gui::SlideInDrawer drawer;
    gui::AboutModalDialog aboutModal;
    gui::LoopbackCalibrationModal loopbackModal { audioEngine };
    gui::OperatorStepModalDialog operatorStepModal;
    gui::ConfirmationModalDialog confirmationModal;

    juce::Label manualPromptLabel;
    juce::TextButton btnStepBack;
    juce::TextButton btnRepeatStep;
    juce::TextButton confirmManualButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
} // namespace abdaudiolab

