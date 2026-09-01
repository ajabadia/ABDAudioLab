#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "BuildVersion.h"
#include "audio/LabAudioEngine.h"
#include "core/HardwareContractRegistry.h"
#include "hardware/AiraSysExController.h"
#include "hardware/MockHardwareController.h"
#include "hardware/MidiCcController.h"
#include "hardware/ManualAnalogueController.h"
#include "core/ProfilingSession.h"
#include "core/ProfilingSequencer.h"

#include "gui/SoundIdTheme.h"
#include "gui/SoundIdCurvePlotter.h"
#include "gui/SoundIdMeterStrip.h"
#include "gui/SoundIdSuiteList.h"
#include "gui/SlideInDrawer.h"
#include "gui/InfoDrawer.h"
#include "gui/AboutModalDialog.h"
#include "gui/LoopbackCalibrationModal.h"
#include "gui/HardwareSelectorPill.h"
#include "gui/SoundIdSplashScreen.h"
#include "core/SessionSerializer.h"

namespace abdaudiolab
{

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

class MainContentComponent : public juce::Component,
                             public juce::Timer,
                             public juce::KeyListener
{
public:
    MainContentComponent()
        : mockController(),
          sequencer(audioEngine, mockController)
    {
        setLookAndFeel(&soundIdTheme);

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
        drawer.setHardwareLocked(true);

        // Header Action Buttons
        btnFileMenu.setButtonText("File v");
        btnFileMenu.setTooltip("Session management: New, Open, Save, Save As, Target Folder, and Exit");
        btnFileMenu.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
        btnFileMenu.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
        btnFileMenu.onClick = [this] { drawer.openFileDrawer(exportDirectory.getFullPathName()); };
        addAndMakeVisible(btnFileMenu);

        btnCalibratePill.setButtonText("Line Calibration: -3.0 dBFS");
        btnCalibratePill.setTooltip("Run loopback line calibration wizard to characterize DAC->ADC latency and frequency response");
        btnCalibratePill.setColour(juce::TextButton::buttonColourId, gui::SoundIdTheme::bgCard);
        btnCalibratePill.setColour(juce::TextButton::textColourOffId, gui::SoundIdTheme::textPrimary);
        btnCalibratePill.onClick = [this] { loopbackModal.showDialog(this); };
        addAndMakeVisible(btnCalibratePill);

        juce::String initialHwName = hwItems.empty() ? "Mock VA DSP (Self-Test)" : hwItems[0].displayName;
        juce::String initialFuncName = (!hwItems.empty() && !hwItems[0].functions.empty()) ? hwItems[0].functions[0].name : "Standard";
        btnHardwareSelector.setHardwareInfo(initialHwName, initialFuncName, drawer.getActiveModelRasterImage(), gui::HardwareConnectionStatus::NotApplicable);
        btnHardwareSelector.onClick = [this] { drawer.openHardwareDrawer(); };
        addAndMakeVisible(btnHardwareSelector);

        // Info Button (Opens Setup & Telemetry Drawer)
        btnInfo.onClick = [this] { showInfoDrawer(); };
        addAndMakeVisible(btnInfo);

        // 4. Center Curve Plotter & Real-Time Visualization
        addAndMakeVisible(curvePlotter);

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
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Edit Completed Test",
                    "Modifying the parameters of '" + item.title + "' will invalidate its recorded measurements.\n\n"
                    "Do you want to proceed and re-queue this test?",
                    "Edit & Invalidate",
                    "Cancel",
                    this,
                    juce::ModalCallbackFunction::create([this, index, item](int result) {
                        if (result == 1) // Edit & Invalidate
                        {
                            gui::TestConfiguration conf;
                            conf.testName = item.title;
                            conf.stimulusType = item.stimulusType;
                            conf.burstDurationSec = item.burstDurationSec;
                            conf.captureMode = item.captureMode;
                            conf.controls = item.controls;
                            drawer.openTestEditorDrawer(conf, index);
                        }
                    })
                );
                return;
            }

            gui::TestConfiguration conf;
            conf.testName = item.title;
            conf.stimulusType = item.stimulusType;
            conf.burstDurationSec = item.burstDurationSec;
            conf.captureMode = item.captureMode;
            conf.controls = item.controls;
            drawer.openTestEditorDrawer(conf, index);
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

            if (conf.stimulusType == audio::StimulusType::SyncPulses3) { item.badgeText = "ENV"; item.badgeColor = juce::Colour(0xff8b5cf6); }
            else if (conf.stimulusType == audio::StimulusType::AmplitudeRamp) { item.badgeText = "SAT"; item.badgeColor = juce::Colour(0xfff59e0b); }
            else if (conf.stimulusType == audio::StimulusType::SineWave1kHz) { item.badgeText = "MOD"; item.badgeColor = juce::Colour(0xff0284c7); }
            else { item.badgeText = "FLT"; item.badgeColor = juce::Colour(0xff10b981); }

            juce::String formulaStr;
            for (const auto& c : item.controls) {
                if (c.steps > 1) {
                    if (formulaStr.isNotEmpty()) formulaStr += " x ";
                    formulaStr += juce::String(c.steps);
                }
            }
            if (formulaStr.isEmpty()) formulaStr = "1";

            item.description = "Sweep • " + formulaStr + " = " + juce::String(item.totalPoints) + " points";
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
            startProfilingSession();
        };

        suiteList.onContinueTestClicked = [this](int index) {
            suiteList.updateItemStatus(index, gui::QueueItemStatus::Running, totalPointsMeasured);
            startProfilingSession();
        };

        suiteList.onToggleSessionRunClicked = [this](bool start) {
            if (start) startProfilingSession();
            else stopProfilingSession();
        };

        suiteList.onDuplicateWarning = [this](const juce::String& msg) {
            manualPromptLabel.setText(msg, juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            juce::Timer::callAfterDelay(4000, [this] {
                if (sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
                    manualPromptLabel.setVisible(false);
            });
        };
        addAndMakeVisible(suiteList);

        // 6. Right Master Level & Meter Strip
        meterStrip.onProfilingToggled = [this](bool start) {
            if (start) startProfilingSession();
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
            suiteList.clearQueue();
            curvePlotter.clear();
            sessionPoints.clear();
            totalPointsMeasured = 0;
            sessionSerializer.cleanupTempSession();
            drawer.setHardwareLocked(false);
            drawer.openHardwareDrawer();
            manualPromptLabel.setText("New session initialized. Select hardware and active submodule, then click Accept.", juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            juce::Timer::callAfterDelay(4000, [this] {
                if (sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
                    manualPromptLabel.setVisible(false);
            });
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
            if (sessionSerializer.getActiveSessionFile().existsAsFile())
                sessionSerializer.getActiveSessionFile().getParentDirectory().revealToUser();
            else
                exportDirectory.revealToUser();
        };
        drawer.onExitAppClicked = [this] {
            confirmAndExit();
        };
        addChildComponent(drawer);

        loopbackModal.onCalibrationApplied = [this](const math::LoopbackCalibrationData& cal) {
            float gainDb = 20.0f * std::log10(std::max(cal.recommendedTrimGain, 1e-4f));
            juce::String sign = (gainDb >= 0.0f) ? "+" : "";
            juce::String msg = "Loopback Calibration complete! Auto-trim applied: " + 
                               sign + juce::String(gainDb, 1) + " dB (Target: -3.0 dBfs)";
            manualPromptLabel.setText(msg, juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            juce::Timer::callAfterDelay(4000, [this] {
                if (sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
                    manualPromptLabel.setVisible(false);
            });
        };
        addChildComponent(loopbackModal);
        addChildComponent(aboutModal);

        // 8. Progress and Sequencer Callbacks
        sequencer.setProgressCallback([this](float progress, const juce::String& task, core::SequencerState state) {
            juce::ignoreUnused(progress);
            if (state == core::SequencerState::WaitingForOperator)
            {
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
                manualPromptLabel.setVisible(false);
                confirmManualButton.setVisible(false);
                btnRepeatStep.setVisible(false);
                btnStepBack.setVisible(false);
                meterStrip.setProfilingActive(false);
                suiteList.setSessionRunning(false);
                for (int i = 0; i < suiteList.getQueueSize(); ++i)
                {
                    if (!suiteList.getQueue()[static_cast<size_t>(i)].isSkipped)
                        suiteList.updateItemStatus(i, gui::QueueItemStatus::Completed);
                }
            }
            else if (state == core::SequencerState::ErrorState)
            {
                manualPromptLabel.setText("Error during profiling session. Check audio connections.", juce::dontSendNotification);
                manualPromptLabel.setVisible(true);
                meterStrip.setProfilingActive(false);
                suiteList.setSessionRunning(false);
                confirmManualButton.setVisible(false);
                btnRepeatStep.setVisible(false);
                btnStepBack.setVisible(false);
                if (suiteList.getQueueSize() > 0)
                    suiteList.updateItemStatus(0, gui::QueueItemStatus::Invalidated);
            }
        });

        sequencer.setTestIndexCallback([this](int queueIndex, int currentPoint, int totalPoints) {
            juce::ignoreUnused(totalPoints);
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
            }
        });

        sequencer.setPointMeasuredCallback([this](const exporting::MeasuredPoint& pt) {
            curvePlotter.addMeasuredPoint(pt);
            sessionPoints.push_back(pt);
            totalPointsMeasured++;

            // Trigger non-blocking auto-save update
            sessionSerializer.triggerIncrementalAutoSave(buildCurrentSessionManifest(), sessionPoints);
        });

        addKeyListener(this);
        setWantsKeyboardFocus(true);
        startTimerHz(30);
        setSize(1040, 720);
    }

    ~MainContentComponent() override
    {
        setLookAndFeel(nullptr);
        removeKeyListener(this);
        stopTimer();
        sequencer.stopSession();
        audioEngine.saveAudioSettings(settingsFile);
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (key.isKeyCode(juce::KeyPress::spaceKey))
        {
            if (confirmManualButton.isEnabled() && confirmManualButton.isVisible())
            {
                confirmManualStep();
                return true;
            }
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(gui::SoundIdTheme::bgLight);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);
        // 1. Top Header Area (Title left, Controls right)
        auto topArea = bounds.removeFromTop(36);
        titleLabel.setBounds(topArea.removeFromLeft(140).withHeight(30));
        topArea.removeFromLeft(4);
        btnFileMenu.setBounds(topArea.removeFromLeft(70).withHeight(32));
        topArea.removeFromLeft(8);

        btnInfo.setBounds(topArea.removeFromRight(32).withSizeKeepingCentre(28, 28));
        topArea.removeFromRight(8);

        btnHardwareSelector.setBounds(topArea.removeFromRight(340).withHeight(32));
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

        // 4. Bottom Test Suite List
        auto bottomArea = bounds.removeFromBottom(180);
        suiteList.setBounds(bottomArea);

        bounds.removeFromBottom(12);

        // 5. Center Curve Plotter
        curvePlotter.setBounds(bounds);

        // 6. Slide-in Drawer & Modals fill full window bounds
        drawer.setBounds(getLocalBounds());
        aboutModal.setBounds(getLocalBounds());
    }

    void timerCallback() override
    {
        meterStrip.setLevels(audioEngine.getInputPeakL(), audioEngine.getInputPeakR(), audioEngine.getInputRmsL(),
                             audioEngine.getOutputPeakL(), audioEngine.getOutputPeakR(), audioEngine.getOutputRmsL());
    }

private:
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
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        "Export Destination Updated",
                        "Target export folder set to:\n" + exportDirectory.getFullPathName(),
                        "OK"
                    );
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
            hardware::AiraModel model = hardware::AiraModel::GenericModular;
            if (contract->id == "roland_aira_bitrazer") model = hardware::AiraModel::Bitrazer;
            else if (contract->id == "roland_aira_demora") model = hardware::AiraModel::Demora;
            else if (contract->id == "roland_aira_torcido") model = hardware::AiraModel::Torcido;
            else if (contract->id == "roland_aira_scooper") model = hardware::AiraModel::Scooper;

            airaController = std::make_unique<hardware::AiraSysExController>(model);
            bool connected = airaController->connect();
            connStatus = connected ? gui::HardwareConnectionStatus::Connected : gui::HardwareConnectionStatus::Disconnected;
        }
        else if (contract->deviceType == "AUTOMATED_MIDI_CC")
        {
            audioEngine.setMockHardware(nullptr);
            if (midiCcController == nullptr)
                midiCcController = std::make_unique<hardware::MidiCcController>();
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

    void startProfilingSession()
    {
        if (suiteList.getQueueSize() == 0)
        {
            manualPromptLabel.setText("Please add at least one test to the Session Plan before starting.", juce::dontSendNotification);
            manualPromptLabel.setVisible(true);
            meterStrip.setProfilingActive(false);
            return;
        }

        suiteList.resetAllStatuses();
        suiteList.updateItemStatus(0, gui::QueueItemStatus::Running, 0);
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
                hardware::AiraModel model = hardware::AiraModel::GenericModular;
                if (contract->id == "roland_aira_bitrazer") model = hardware::AiraModel::Bitrazer;
                else if (contract->id == "roland_aira_demora") model = hardware::AiraModel::Demora;
                else if (contract->id == "roland_aira_torcido") model = hardware::AiraModel::Torcido;
                else if (contract->id == "roland_aira_scooper") model = hardware::AiraModel::Scooper;

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

        core::ProfilingSession session = buildProfilingSessionFromQueue(hwName, modeStr);
        juce::String baseName = juce::String(hwName) + "_" + selectedFuncId;

        curvePlotter.clear();
        sessionPoints.clear();
        totalPointsMeasured = 0;
        meterStrip.setProfilingActive(true);

        sequencer.startSession(session, exportDirectory, baseName);
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
        aboutModal.showDialog(this);
    }

    core::ProfilingSession buildProfilingSessionFromQueue(const std::string& hwName, const std::string& modeStr)
    {
        core::ProfilingSession session;
        core::ProfilingMetadata meta;
        meta.hardwareName = hwName;
        meta.targetModule = drawer.getSelectedFunctionId().toStdString();
        meta.operatorMode = modeStr;
        meta.sampleRate = audioEngine.getSampleRate();
        meta.bitDepth = 24;
        meta.timestamp = juce::Time::getCurrentTime().toISO8601(true).toStdString();
        session.setMetadata(meta);

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
                session.addTestCase(tc);
                continue;
            }

            int step1Count = 1;
            int step2Count = 1;
            std::string param1Name = "Control1";
            std::string param2Name = "Control2";

            if (!item.controls.empty())
            {
                step1Count = std::max(1, item.controls[0].steps);
                param1Name = item.controls[0].name.toStdString();
            }
            if (item.controls.size() > 1)
            {
                step2Count = std::max(1, item.controls[1].steps);
                param2Name = item.controls[1].name.toStdString();
            }

            int totalTestPoints = step1Count * step2Count;
            int pointCounter = 0;

            for (int s1 = 0; s1 < step1Count; ++s1)
            {
                float norm1 = (step1Count > 1) ? (static_cast<float>(s1) / static_cast<float>(step1Count - 1)) : 0.5f;
                int raw1 = static_cast<int>(norm1 * 127.0f);

                for (int s2 = 0; s2 < step2Count; ++s2)
                {
                    float norm2 = (step2Count > 1) ? (static_cast<float>(s2) / static_cast<float>(step2Count - 1)) : 0.5f;
                    int raw2 = static_cast<int>(norm2 * 127.0f);

                    pointCounter++;
                    core::TestCase tc;
                    tc.queueItemIndex = qIdx;
                    tc.pointIndexInTest = pointCounter;
                    tc.totalPointsInTest = totalTestPoints;
                    tc.testId = item.title.toStdString();
                    
                    if (item.badgeText == "FLT") tc.functionalBlockType = "SpectrumFilter";
                    else if (item.badgeText == "ENV") tc.functionalBlockType = "TimeDynamic";
                    else if (item.badgeText == "SAT") tc.functionalBlockType = "WaveShaper";
                    else if (item.badgeText == "MOD") tc.functionalBlockType = "CyclicModulator";
                    else tc.functionalBlockType = "AmplitudeGain";

                    tc.stimulusType = item.stimulusType;
                    tc.stimulusDurationSec = item.burstDurationSec;
                    tc.startFreqHz = 20.0f;
                    tc.endFreqHz = 20000.0f;
                    tc.numPasses = 1;
                    tc.stabilizationWaitMs = 50.0;

                    core::ParameterStep p1 { 1, param1Name, norm1, raw1 };
                    tc.parameterSteps.push_back(p1);

                    if (item.controls.size() > 1)
                    {
                        core::ParameterStep p2 { 2, param2Name, norm2, raw2 };
                        tc.parameterSteps.push_back(p2);
                    }

                    session.addTestCase(tc);
                }
            }
        }
        return session;
    }

    core::SessionManifest buildCurrentSessionManifest()
    {
        core::SessionManifest sm;
        sm.appVersion = "1.0.0";
        sm.buildNumber = 130;
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

            if (tc.stimulusType == audio::StimulusType::SyncPulses3) { item.badgeText = "ENV"; item.badgeColor = juce::Colour(0xff8b5cf6); }
            else if (tc.stimulusType == audio::StimulusType::AmplitudeRamp) { item.badgeText = "SAT"; item.badgeColor = juce::Colour(0xfff59e0b); }
            else if (tc.stimulusType == audio::StimulusType::SineWave1kHz) { item.badgeText = "MOD"; item.badgeColor = juce::Colour(0xff0284c7); }
            else { item.badgeText = "FLT"; item.badgeColor = juce::Colour(0xff10b981); }

            suiteList.addTestToQueue(item);
        }

        manualPromptLabel.setText("Session loaded: " + juce::String(manifest.hardwareDisplayName) + " (" + juce::String(points.size()) + " points)", juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        juce::Timer::callAfterDelay(4000, [this] {
            if (sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
                manualPromptLabel.setVisible(false);
        });
    }

    void handleSaveSession()
    {
        if (sessionSerializer.getActiveSessionFile().existsAsFile())
        {
            bool ok = sessionSerializer.saveSessionToPackage(sessionSerializer.getActiveSessionFile(), buildCurrentSessionManifest(), sessionPoints);
            if (ok)
            {
                manualPromptLabel.setText("Session saved: " + sessionSerializer.getActiveSessionFile().getFileName(), juce::dontSendNotification);
                manualPromptLabel.setVisible(true);
                juce::Timer::callAfterDelay(3000, [this] {
                    if (sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
                        manualPromptLabel.setVisible(false);
                });
            }
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
        auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File())
            {
                if (file.getFileExtension() != ".abdlabtest")
                    file = file.withFileExtension(".abdlabtest");

                bool ok = sessionSerializer.saveSessionToPackage(file, buildCurrentSessionManifest(), sessionPoints);
                if (ok)
                {
                    manualPromptLabel.setText("Session saved to: " + file.getFileName(), juce::dontSendNotification);
                    manualPromptLabel.setVisible(true);
                    juce::Timer::callAfterDelay(3000, [this] {
                        if (sequencer.getCurrentState() != core::SequencerState::WaitingForOperator)
                            manualPromptLabel.setVisible(false);
                    });
                }
            }
        });
    }

    void handleOpenSession()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Open ABDAudioLab Session Package (.abdlabtest)...",
            exportDirectory,
            "*.abdlabtest;*.json"
        );
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                core::SessionManifest manifest;
                std::vector<exporting::MeasuredPoint> points;
                juce::String err;
                if (sessionSerializer.loadSessionFromPackage(file, manifest, points, err))
                {
                    applyLoadedSession(manifest, points);
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
            juce::AlertWindow::showYesNoCancelBox(
                juce::AlertWindow::QuestionIcon,
                "Delete Measured Test",
                "Test '" + item.title + "' contains recorded measurement data.\n\n"
                "What would you like to do?",
                "Discard & Delete",
                "Mark as Invalid (Keep)",
                "Cancel",
                this,
                juce::ModalCallbackFunction::create([this, index](int result) {
                    if (result == 1) // Discard & Delete
                    {
                        suiteList.removeTestDirectly(index);
                    }
                    else if (result == 2) // Mark as Invalid
                    {
                        suiteList.invalidateTest(index);
                    }
                })
            );
        }
        else
        {
            suiteList.removeTestDirectly(index);
        }
    }

    void confirmAndExit()
    {
        if (totalPointsMeasured > 0)
        {
            juce::AlertWindow::showYesNoCancelBox(
                juce::AlertWindow::QuestionIcon,
                "Exit ABDAudioLab",
                "You have active measured points in this session. Do you want to save before exiting?",
                "Save and Exit",
                "Exit Without Saving",
                "Cancel",
                this,
                juce::ModalCallbackFunction::create([this](int result) {
                    if (result == 1) // Save and Exit
                    {
                        handleSaveSession();
                        juce::JUCEApplication::getInstance()->systemRequestedQuit();
                    }
                    else if (result == 2) // Exit Without Saving
                    {
                        juce::JUCEApplication::getInstance()->systemRequestedQuit();
                    }
                })
            );
        }
        else
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    }

    // Engine & Controllers
    audio::LabAudioEngine audioEngine;
    core::HardwareContractRegistry contractRegistry;
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
    int currentSuiteId { 1 };
    int totalPointsMeasured { 0 };

    // UI Widgets & Visualizers
    juce::Label titleLabel;
    juce::TextButton btnFileMenu;
    juce::TextButton btnCalibratePill;
    gui::HardwareSelectorPill btnHardwareSelector;
    MonochromeInfoButton btnInfo;

    gui::SoundIdCurvePlotter curvePlotter;
    gui::SoundIdMeterStrip meterStrip;
    gui::SoundIdSuiteList suiteList;
    gui::SlideInDrawer drawer;
    gui::AboutModalDialog aboutModal;
    gui::LoopbackCalibrationModal loopbackModal { audioEngine };

    juce::Label manualPromptLabel;
    juce::TextButton btnStepBack;
    juce::TextButton btnRepeatStep;
    juce::TextButton confirmManualButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

class LabMainWindow : public juce::DocumentWindow
{
public:
    explicit LabMainWindow(juce::String name)
        : DocumentWindow(name,
                         gui::SoundIdTheme::bgLight,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainContentComponent(), true);

        #if JUCE_IOS || JUCE_ANDROID
         setFullScreen(true);
        #else
         setResizable(true, true);
         setResizeLimits(850, 580, 1920, 1080);
         centreWithSize(getWidth(), getHeight());
        #endif

        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabMainWindow)
};

class LabApplication : public juce::JUCEApplication
{
public:
    LabApplication() = default;

    const juce::String getApplicationName() override       { return "ABDAudioLab"; }
    const juce::String getApplicationVersion() override    { return version::kAppVersion; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // 1. Show Instant Floating Splash Window (< 50ms)
        splashWindow = std::make_unique<gui::SoundIdSplashWindow>();
        splashWindow->setStatus("Scanning Audio Interfaces & ASIO Drivers...", 0.25f);

        // 2. Initialize Main Engine and Window
        juce::MessageManager::callAsync([this]() {
            if (splashWindow)
                splashWindow->setStatus("Loading Hardware Contracts & DSP Profiles...", 0.65f);

            mainWindow = std::make_unique<LabMainWindow>(getApplicationName());

            if (splashWindow)
                splashWindow->setStatus("Ready.", 1.0f);

            // 3. Smooth fade-out transition
            juce::Timer::callAfterDelay(450, [this]() {
                if (splashWindow)
                {
                    splashWindow->dismiss([this]() {
                        splashWindow.reset();
                    });
                }
            });
        });
    }

    void shutdown() override
    {
        splashWindow.reset();
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
    }

private:
    std::unique_ptr<gui::SoundIdSplashWindow> splashWindow;
    std::unique_ptr<LabMainWindow> mainWindow;
};

} // namespace abdaudiolab

START_JUCE_APPLICATION(abdaudiolab::LabApplication)
