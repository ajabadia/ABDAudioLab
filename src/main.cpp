#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "audio/LabAudioEngine.h"
#include "hardware/AiraSysExController.h"
#include "hardware/MockHardwareController.h"
#include "hardware/MidiCcController.h"
#include "hardware/ManualAnalogueController.h"
#include "core/ProfilingSession.h"
#include "core/ProfilingSequencer.h"

namespace abdaudiolab
{

class MainContentComponent : public juce::Component,
                             public juce::Timer,
                             public juce::KeyListener
{
public:
    MainContentComponent()
        : mockController(),
          sequencer(audioEngine, mockController)
    {
        // 1. Initialize Audio Engine & Restore State
        juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ABDAudioLab");
        settingsFile = appData.getChildFile("AudioSettings.xml");
        audioEngine.initializeAudioDevices(settingsFile);
        audioEngine.setMockHardware(&mockController);

        // 2. Setup Manual Controller Callback
        manualController.setPromptCallback([this](const juce::String& paramName, float norm, int raw) {
            juce::MessageManager::callAsync([this, paramName, norm, raw]() {
                manualPromptLabel.setText("MANUAL ACTION REQUIRED: Adjust [" + paramName + 
                                          "] to " + juce::String(norm, 2) + " (Raw: " + juce::String(raw) + 
                                          ") and press SPACEBAR or click Confirm.", juce::dontSendNotification);
                confirmManualButton.setEnabled(true);
                confirmManualButton.setVisible(true);
            });
        });

        // 3. UI Header
        titleLabel.setText("ABDAudioLab - Universal Hardware Profiler", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff61afef));
        addAndMakeVisible(titleLabel);

        statusLabel.setText("Status: Ready. Select hardware mode and test suite.", juce::dontSendNotification);
        statusLabel.setFont(juce::FontOptions(14.0f));
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(statusLabel);

        // 4. Hardware Mode Selector
        hwModeLabel.setText("Hardware Mode:", juce::dontSendNotification);
        hwModeLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        addAndMakeVisible(hwModeLabel);

        hwModeCombo.addItem("1. Mock Virtual-Analog DSP (Self-Test)", 1);
        hwModeCombo.addItem("2. Roland AIRA Modular (USB SysEx/CC Automated)", 2);
        hwModeCombo.addItem("3. Generic MIDI CC Synthesizer (Automated)", 3);
        hwModeCombo.addItem("4. Manual Analog / Eurorack (Operator-Assisted)", 4);
        hwModeCombo.setSelectedId(1);
        hwModeCombo.onChange = [this]() { onHardwareModeChanged(); };
        addAndMakeVisible(hwModeCombo);

        // 5. Test Suite Selector
        testSuiteLabel.setText("Test Suite:", juce::dontSendNotification);
        testSuiteLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        addAndMakeVisible(testSuiteLabel);

        testSuiteCombo.addItem("SpectrumFilter: 2D Farina Sweep (Cutoff x Res)", 1);
        testSuiteCombo.addItem("TimeDynamic: ADSR Envelope (Attack x Decay)", 2);
        testSuiteCombo.addItem("TimeDynamic: Delay Impulse (Time x Feedback)", 3);
        testSuiteCombo.addItem("WaveShaper: Linear Ramp Saturation & THD", 4);
        testSuiteCombo.addItem("AmplitudeGain: 1 kHz Sine VCA Attenuation", 5);
        testSuiteCombo.addItem("Load Custom TestProfile.json from disk...", 6);
        testSuiteCombo.setSelectedId(1);
        testSuiteCombo.onChange = [this]() { onTestSuiteChanged(); };
        addAndMakeVisible(testSuiteCombo);

        // 6. Action Buttons
        audioSettingsButton.setButtonText("Audio & MIDI Setup...");
        audioSettingsButton.onClick = [this]() { openAudioMidiSettings(); };
        addAndMakeVisible(audioSettingsButton);

        testToneButton.setButtonText("Diagnostic Tone (1 kHz)");
        testToneButton.onClick = [this]() {
            bool current = audioEngine.isDiagnosticTestToneActive();
            audioEngine.enableDiagnosticTestTone(!current);
            testToneButton.setButtonText(!current ? "Stop Tone" : "Diagnostic Tone (1 kHz)");
            testToneButton.setColour(juce::TextButton::buttonColourId, !current ? juce::Colour(0xffe06c75) : juce::Colour(0xff3e4451));
        };
        addAndMakeVisible(testToneButton);

        startProfilingButton.setButtonText("START PROFILING SESSION");
        startProfilingButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff98c379));
        startProfilingButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        startProfilingButton.onClick = [this]() { startProfilingSession(); };
        addAndMakeVisible(startProfilingButton);

        stopProfilingButton.setButtonText("STOP / PAUSE SAFE");
        stopProfilingButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe06c75));
        stopProfilingButton.setEnabled(false);
        stopProfilingButton.onClick = [this]() { stopProfilingSession(); };
        addAndMakeVisible(stopProfilingButton);

        // 7. Manual Operator Confirmation Box
        manualPromptLabel.setText("", juce::dontSendNotification);
        manualPromptLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        manualPromptLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe5c07b));
        manualPromptLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff2c313a));
        addAndMakeVisible(manualPromptLabel);

        confirmManualButton.setButtonText("CONFIRM STEP (SPACEBAR)");
        confirmManualButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe5c07b));
        confirmManualButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        confirmManualButton.setEnabled(false);
        confirmManualButton.setVisible(false);
        confirmManualButton.onClick = [this]() { confirmManualStep(); };
        addAndMakeVisible(confirmManualButton);

        // 8. Progress and Console Log
        progressBar.setPercentageDisplay(true);
        addAndMakeVisible(progressBar);

        logBox.setMultiLine(true);
        logBox.setReadOnly(true);
        logBox.setCaretVisible(false);
        logBox.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
        logBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff181a1f));
        logBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xffabb2bf));
        addAndMakeVisible(logBox);

        // Setup Progress Callback
        sequencer.setProgressCallback([this](float progress, const juce::String& task, core::SequencerState state) {
            currentProgress = progress;
            currentTaskString = task;

            logBox.moveCaretToEnd();
            logBox.insertTextAtCaret(juce::Time::getCurrentTime().formatted("[%H:%M:%S] ") + task + "\n");

            if (state == core::SequencerState::Finished || state == core::SequencerState::ErrorState)
            {
                startProfilingButton.setEnabled(true);
                stopProfilingButton.setEnabled(false);
                confirmManualButton.setEnabled(false);
                manualPromptLabel.setText("", juce::dontSendNotification);
                statusLabel.setText("Status: Session Ended.", juce::dontSendNotification);
            }
        });

        addKeyListener(this);
        setWantsKeyboardFocus(true);
        startTimerHz(20);
        setSize(920, 680);
    }

    ~MainContentComponent() override
    {
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
        g.fillAll(juce::Colour(0xff21252b));

        g.setColour(juce::Colour(0xff282c34));
        g.fillRoundedRectangle(16.0f, 64.0f, static_cast<float>(getWidth() - 32), 80.0f, 6.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(16);

        titleLabel.setBounds(bounds.removeFromTop(30));
        statusLabel.setBounds(bounds.removeFromTop(22));

        bounds.removeFromTop(12);

        // Row 1: Selectors & Settings
        auto row1 = bounds.removeFromTop(32);
        hwModeLabel.setBounds(row1.removeFromLeft(110));
        hwModeCombo.setBounds(row1.removeFromLeft(280));
        row1.removeFromLeft(12);
        testSuiteLabel.setBounds(row1.removeFromLeft(80));
        testSuiteCombo.setBounds(row1.removeFromLeft(260));
        row1.removeFromLeft(12);
        audioSettingsButton.setBounds(row1);

        bounds.removeFromTop(8);

        // Row 2: Action Controls
        auto row2 = bounds.removeFromTop(36);
        startProfilingButton.setBounds(row2.removeFromLeft(240));
        row2.removeFromLeft(10);
        stopProfilingButton.setBounds(row2.removeFromLeft(160));
        row2.removeFromLeft(10);
        testToneButton.setBounds(row2.removeFromLeft(180));

        bounds.removeFromTop(12);

        // Row 3: Manual Operator Banner & Confirm Button
        auto row3 = bounds.removeFromTop(34);
        confirmManualButton.setBounds(row3.removeFromRight(200));
        row3.removeFromRight(8);
        manualPromptLabel.setBounds(row3);

        bounds.removeFromTop(10);
        progressBar.setBounds(bounds.removeFromTop(20));

        bounds.removeFromTop(10);
        logBox.setBounds(bounds);
    }

    void timerCallback() override
    {
        progressValue = currentProgress;
        statusLabel.setText("Status: " + currentTaskString, juce::dontSendNotification);
        repaint();
    }

private:
    void openAudioMidiSettings()
    {
        // Set 5th and 6th booleans to true to enable MIDI Input and Output device selectors!
        auto* selector = new juce::AudioDeviceSelectorComponent(
            audioEngine.getDeviceManager(),
            0, 2,  // min/max input channels
            0, 2,  // min/max output channels
            true,  // show MIDI input selector
            true,  // show MIDI output selector
            false, // no channels as stereo pairs check
            false  // hide advanced options
        );
        selector->setSize(520, 520);

        juce::DialogWindow::LaunchOptions opt;
        opt.content.setOwned(selector);
        opt.dialogTitle = "Audio & MIDI Configuration";
        opt.dialogBackgroundColour = juce::Colour(0xff282c34);
        opt.escapeKeyTriggersCloseButton = true;
        opt.useNativeTitleBar = true;
        opt.resizable = false;
        opt.launchAsync();
    }

    void onHardwareModeChanged()
    {
        int id = hwModeCombo.getSelectedId();
        switch (id)
        {
            case 1: // Mock DSP
                audioEngine.setMockHardware(&mockController);
                logBox.insertTextAtCaret("[Setup] Switched to Mock Virtual-Analog DSP mode.\n");
                break;
            case 2: // Roland AIRA
                audioEngine.setMockHardware(nullptr);
                if (airaController == nullptr)
                    airaController = std::make_unique<hardware::AiraSysExController>(hardware::AiraModel::Bitrazer);
                airaController->connect();
                logBox.insertTextAtCaret("[Setup] Switched to Roland AIRA Modular mode (SysEx/CC).\n");
                break;
            case 3: // Generic MIDI CC
                audioEngine.setMockHardware(nullptr);
                if (midiCcController == nullptr)
                    midiCcController = std::make_unique<hardware::MidiCcController>();
                midiCcController->connect();
                logBox.insertTextAtCaret("[Setup] Switched to Generic MIDI CC Synthesizer mode.\n");
                break;
            case 4: // Manual Eurorack
                audioEngine.setMockHardware(nullptr);
                manualController.connect();
                logBox.insertTextAtCaret("[Setup] Switched to Manual Analog / Eurorack Operator-Assisted mode.\n");
                break;
        }
    }

    void onTestSuiteChanged()
    {
        if (testSuiteCombo.getSelectedId() == 6)
        {
            fileChooser = std::make_unique<juce::FileChooser>(
                "Select TestProfile.json file",
                juce::File::getCurrentWorkingDirectory(),
                "*.json"
            );

            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                    {
                        customProfilePath = file.getFullPathName();
                        logBox.insertTextAtCaret("[Profile] Selected custom profile: " + file.getFileName() + "\n");
                    }
                    else
                    {
                        testSuiteCombo.setSelectedId(1);
                    }
                });
        }
    }

    void startProfilingSession()
    {
        hardware::IHardwareController* activeHw = &mockController;
        std::string modeStr = "MOCK_DSP";
        std::string hwName = "MOCK_VA_SYNTH";

        int hwId = hwModeCombo.getSelectedId();
        if (hwId == 2)
        {
            if (airaController == nullptr)
                airaController = std::make_unique<hardware::AiraSysExController>(hardware::AiraModel::Bitrazer);
            airaController->connect();
            activeHw = airaController.get();
            modeStr = "AUTOMATIC_ROLAND_SYSEX";
            hwName = "ROLAND_AIRA_MODULAR";
        }
        else if (hwId == 3)
        {
            if (midiCcController == nullptr)
                midiCcController = std::make_unique<hardware::MidiCcController>();
            midiCcController->connect();
            activeHw = midiCcController.get();
            modeStr = "AUTOMATIC_MIDI_CC";
            hwName = "GENERIC_MIDI_SYNTH";
        }
        else if (hwId == 4)
        {
            manualController.connect();
            activeHw = &manualController;
            modeStr = "MANUAL_EURORACK";
            hwName = "MANUAL_ANALOG_EURORACK";
        }

        core::ProfilingSession session;
        int suiteId = testSuiteCombo.getSelectedId();

        if (suiteId == 1) session = core::ProfilingSession::createFilterSuite(hwName, modeStr, 8, 4);
        else if (suiteId == 2) session = core::ProfilingSession::createAdsrSuite(hwName, modeStr, 6, 4);
        else if (suiteId == 3) session = core::ProfilingSession::createDelaySuite(hwName, modeStr, 8, 4);
        else if (suiteId == 4) session = core::ProfilingSession::createWaveShaperSuite(hwName, modeStr, 10);
        else if (suiteId == 5) session = core::ProfilingSession::createGainVcaSuite(hwName, modeStr, 10);
        else if (suiteId == 6 && customProfilePath.isNotEmpty())
        {
            session.loadProfileFromFile(customProfilePath.toStdString());
        }
        else
        {
            session = core::ProfilingSession::createFilterSuite(hwName, modeStr, 8, 4);
        }

        juce::File exportDir = juce::File::getCurrentWorkingDirectory().getChildFile("exported_luts");
        juce::String baseName = juce::String(hwName) + "_" + juce::String(session.getMetadata().targetModule);

        startProfilingButton.setEnabled(false);
        stopProfilingButton.setEnabled(true);
        logBox.clear();
        logBox.insertTextAtCaret("=== STARTING PROFILING SESSION: " + baseName + " ===\n");
        logBox.insertTextAtCaret("[Session] Total test cases queued: " + juce::String(session.getTestCases().size()) + "\n");

        sequencer.startSession(session, exportDir, baseName);
    }

    void stopProfilingSession()
    {
        sequencer.stopSession();
        startProfilingButton.setEnabled(true);
        stopProfilingButton.setEnabled(false);
        confirmManualButton.setEnabled(false);
        manualPromptLabel.setText("", juce::dontSendNotification);
        logBox.insertTextAtCaret("[Session] Aborted by user.\n");
    }

    void confirmManualStep()
    {
        confirmManualButton.setEnabled(false);
        manualPromptLabel.setText("Stabilizing after operator adjustment...", juce::dontSendNotification);
        sequencer.confirmOperatorStep();
    }

    // Engine & Controllers
    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockController;
    std::unique_ptr<hardware::AiraSysExController> airaController;
    std::unique_ptr<hardware::MidiCcController> midiCcController;
    hardware::ManualAnalogueController manualController;
    core::ProfilingSequencer sequencer;

    // Files
    juce::File settingsFile;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String customProfilePath;

    // UI Widgets
    juce::Label titleLabel;
    juce::Label statusLabel;

    juce::Label hwModeLabel;
    juce::ComboBox hwModeCombo;

    juce::Label testSuiteLabel;
    juce::ComboBox testSuiteCombo;

    juce::TextButton audioSettingsButton;
    juce::TextButton testToneButton;
    juce::TextButton startProfilingButton;
    juce::TextButton stopProfilingButton;

    juce::Label manualPromptLabel;
    juce::TextButton confirmManualButton;

    juce::ProgressBar progressBar { progressValue };
    juce::TextEditor logBox;

    double progressValue { 0.0 };
    float currentProgress { 0.0f };
    juce::String currentTaskString { "Ready" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

class LabMainWindow : public juce::DocumentWindow
{
public:
    explicit LabMainWindow(juce::String name)
        : DocumentWindow(name,
                         juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainContentComponent(), true);

        #if JUCE_IOS || JUCE_ANDROID
         setFullScreen(true);
        #else
         setResizable(true, true);
         setResizeLimits(800, 550, 1920, 1080);
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
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<LabMainWindow>(getApplicationName());
    }

    void shutdown() override
    {
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
    std::unique_ptr<LabMainWindow> mainWindow;
};

} // namespace abdaudiolab

START_JUCE_APPLICATION(abdaudiolab::LabApplication)
