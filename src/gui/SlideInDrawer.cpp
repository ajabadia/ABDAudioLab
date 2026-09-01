#include "SlideInDrawer.h"
#include <cmath>

namespace abdaudiolab::gui
{

static juce::File locateAssetFile(const juce::String& relPath)
{
    if (relPath.isEmpty()) return {};

    auto findExisting = [](const juce::File& file) -> juce::File {
        // ALWAYS prioritize .png because JUCE ImageFileFormat does not decode .webp natively!
        if (file.getFileExtension().equalsIgnoreCase(".webp"))
        {
            auto png = file.withFileExtension(".png");
            if (png.existsAsFile()) return png;
        }
        else if (file.getFileExtension().equalsIgnoreCase(".png"))
        {
            if (file.existsAsFile()) return file;
            auto webp = file.withFileExtension(".webp");
            if (webp.existsAsFile()) return webp;
        }
        if (file.existsAsFile()) return file;
        return {};
    };

    // 1. Direct path
    juce::File f(relPath);
    auto found = findExisting(f);
    if (found.existsAsFile()) return found;

    // 2. Direct absolute link to ABDSharedAssets
    juce::File sharedAssetsDir("D:/desarrollos/ABDSynths/ABDSharedAssets");
    if (sharedAssetsDir.isDirectory())
    {
        found = findExisting(sharedAssetsDir.getChildFile(relPath));
        if (found.existsAsFile()) return found;

        found = findExisting(sharedAssetsDir.getChildFile("models").getChildFile(relPath));
        if (found.existsAsFile()) return found;
        found = findExisting(sharedAssetsDir.getChildFile("brands").getChildFile(relPath));
        if (found.existsAsFile()) return found;
        found = findExisting(sharedAssetsDir.getChildFile("models/logos").getChildFile(relPath));
        if (found.existsAsFile()) return found;
    }

    // 3. Current Working Directory
    found = findExisting(juce::File::getCurrentWorkingDirectory().getChildFile(relPath));
    if (found.existsAsFile()) return found;

    // 4. Executable Directory relative path traversal
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    found = findExisting(exeDir.getChildFile(relPath));
    if (found.existsAsFile()) return found;

    // 5. Project root and parent shared assets
    auto projectRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory();
    found = findExisting(projectRoot.getChildFile(relPath));
    if (found.existsAsFile()) return found;

    auto sharedRel = projectRoot.getParentDirectory().getChildFile("ABDSharedAssets");
    if (sharedRel.isDirectory())
    {
        found = findExisting(sharedRel.getChildFile(relPath));
        if (found.existsAsFile()) return found;
    }

    return {};
}

SlideInDrawer::SlideInDrawer()
{
    setAlwaysOnTop(true);
    setVisible(false);

    addAndMakeVisible(panel);
    panel.addAndMakeVisible(viewport);
    viewport.setViewedComponent(&contentComp, false);

    btnClose.setButtonText("X");
    btnClose.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClose.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnClose.onClick = [this] { closeDrawer(); };
    panel.addAndMakeVisible(btnClose);

    // ==========================================
    // 0. File & Session Setup
    // ==========================================
    lblFileSection.setText("SESSION & PROJECT ACTIONS", juce::dontSendNotification);
    lblFileSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblFileSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblFileSection);

    btnFileNew.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnFileNew.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileNew.onClick = [this] {
        closeDrawer();
        if (onNewSessionClicked) onNewSessionClicked();
    };
    contentComp.addChildComponent(btnFileNew);

    btnFileOpen.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnFileOpen.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileOpen.onClick = [this] {
        closeDrawer();
        if (onOpenSessionClicked) onOpenSessionClicked();
    };
    contentComp.addChildComponent(btnFileOpen);

    btnFileSave.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnFileSave.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileSave.onClick = [this] {
        closeDrawer();
        if (onSaveSessionClicked) onSaveSessionClicked();
    };
    contentComp.addChildComponent(btnFileSave);

    btnFileSaveAs.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnFileSaveAs.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileSaveAs.onClick = [this] {
        closeDrawer();
        if (onSaveSessionAsClicked) onSaveSessionAsClicked();
    };
    contentComp.addChildComponent(btnFileSaveAs);

    lblFileExportSection.setText("TARGET EXPORT DIRECTORY", juce::dontSendNotification);
    lblFileExportSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblFileExportSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblFileExportSection);

    lblFileExportPathVal.setFont(juce::FontOptions(10.0f));
    lblFileExportPathVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblFileExportPathVal);

    btnFileChangeExport.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnFileChangeExport.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileChangeExport.onClick = [this] {
        if (onChangeExportFolderClicked) onChangeExportFolderClicked();
    };
    contentComp.addChildComponent(btnFileChangeExport);

    btnFileRevealExport.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnFileRevealExport.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileRevealExport.onClick = [this] {
        if (onRevealExportFolderClicked) onRevealExportFolderClicked();
    };
    contentComp.addChildComponent(btnFileRevealExport);

    btnFileExit.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfffee2e2));
    btnFileExit.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
    btnFileExit.onClick = [this] {
        if (onExitAppClicked) onExitAppClicked();
    };
    contentComp.addChildComponent(btnFileExit);

    // ==========================================
    // 1. Hardware & Routing Setup
    // ==========================================
    contentComp.addChildComponent(imgDisplay);

    lblHardwareLockedBanner.setText("Hardware Profile Locked to active session.\nTo measure a different device or submodule, create a New Session (File > New Session).", juce::dontSendNotification);
    lblHardwareLockedBanner.setFont(juce::FontOptions(10.0f, juce::Font::italic));
    lblHardwareLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblHardwareLockedBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0xfff3f4f6));
    lblHardwareLockedBanner.setJustificationType(juce::Justification::centred);
    contentComp.addChildComponent(lblHardwareLockedBanner);

    lblHwTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    lblHwTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblHwTitle);

    lblSelectHw.setText("Hardware Model / Profile:", juce::dontSendNotification);
    lblSelectHw.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    lblSelectHw.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblSelectHw);

    hwModeCombo.onChange = [this] {
        int selId = hwModeCombo.getSelectedId();
        if (selId >= 1 && selId <= static_cast<int>(hardwareList.size()))
        {
            const auto& item = hardwareList[static_cast<size_t>(selId - 1)];
            lblHwTitle.setText(item.displayName, juce::dontSendNotification);
            updateBrandAndModelGraphics();
            updateFunctionSelectionUI(item);
            if (onHardwareSelected)
                onHardwareSelected(item.id, getSelectedFunctionId());
        }
        layoutDrawerContent();
        repaint();
    };
    contentComp.addChildComponent(hwModeCombo);

    lblSelectFunc.setText("Active Hardware Function / Block:", juce::dontSendNotification);
    lblSelectFunc.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    lblSelectFunc.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblSelectFunc);

    hwFunctionCombo.onChange = [this] {
        int selHw = hwModeCombo.getSelectedId();
        int selFunc = hwFunctionCombo.getSelectedId();
        if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
        {
            const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
            if (selFunc >= 1 && selFunc <= static_cast<int>(hw.functions.size()))
            {
                const auto& f = hw.functions[static_cast<size_t>(selFunc - 1)];
                cardWiring.stimulusText = f.stimulusOutput;
                cardWiring.responseText = f.responseInput;
                cardWiring.repaint();

                if (onHardwareSelected)
                    onHardwareSelected(hw.id, f.id);
            }
        }
        layoutDrawerContent();
        repaint();
    };
    contentComp.addChildComponent(hwFunctionCombo);

    contentComp.addChildComponent(cardWiring);

    btnAutoDetect.setTooltip("Automatically query connected MIDI ports via SysEx Identity Inquiry to identify hardware");
    btnAutoDetect.setEnabled(false);
    contentComp.addChildComponent(btnAutoDetect);

    // ==========================================
    // 2. Test & Parameter Editor Setup
    // ==========================================
    lblPresetSelector.setText("1. SELECT TEST PRESET OR CUSTOM PROFILE", juce::dontSendNotification);
    lblPresetSelector.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblPresetSelector.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblPresetSelector);

    comboTestPresets.onChange = [this] {
        int selId = comboTestPresets.getSelectedId();
        int selHw = hwModeCombo.getSelectedId();
        if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
        {
            const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
            if (selId >= 1 && selId <= static_cast<int>(hw.functions.size()))
            {
                const auto& f = hw.functions[static_cast<size_t>(selId - 1)];
                testEditorConfig.testName = hw.displayName + " (" + f.name + ")";
                
                if (f.blockType == "TimeDynamic") testEditorConfig.stimulusType = audio::StimulusType::SyncPulses3;
                else if (f.blockType == "WaveShaper") testEditorConfig.stimulusType = audio::StimulusType::AmplitudeRamp;
                else if (f.blockType == "CyclicModulator") testEditorConfig.stimulusType = audio::StimulusType::SineWave1kHz;
                else testEditorConfig.stimulusType = audio::StimulusType::LogFarinaSweep;

                testEditorConfig.burstDurationSec = f.defaultBurstDurationSec > 0.05f ? f.defaultBurstDurationSec : 1.0f;
                testEditorConfig.captureMode = f.captureMode;

                testEditorConfig.controls.clear();
                for (size_t k = 0; k < f.controls.size(); ++k)
                {
                    ControlStepConfig cs;
                    cs.name = f.controls[k].name;
                    cs.type = f.controls[k].type;
                    cs.steps = (k == 0) ? 8 : ((k == 1) ? 4 : 1);
                    testEditorConfig.controls.push_back(cs);
                }
            }
            else if (selId == 999) // Custom Profile
            {
                testEditorConfig.testName = hw.displayName + " (Custom Sweep)";
                testEditorConfig.stimulusType = audio::StimulusType::LogFarinaSweep;
                testEditorConfig.burstDurationSec = 1.0f;
                testEditorConfig.captureMode = "FIXED_TIME";

                testEditorConfig.controls.clear();
                int curFuncId = hwFunctionCombo.getSelectedId();
                if (curFuncId >= 1 && curFuncId <= static_cast<int>(hw.functions.size()))
                {
                    const auto& f = hw.functions[static_cast<size_t>(curFuncId - 1)];
                    for (size_t k = 0; k < f.controls.size(); ++k)
                    {
                        ControlStepConfig cs;
                        cs.name = f.controls[k].name;
                        cs.type = f.controls[k].type;
                        cs.steps = (k == 0) ? 8 : ((k == 1) ? 4 : 1);
                        testEditorConfig.controls.push_back(cs);
                    }
                }
                if (testEditorConfig.controls.empty())
                {
                    ControlStepConfig c1; c1.name = "Primary Parameter"; c1.type = "Knob"; c1.steps = 8;
                    ControlStepConfig c2; c2.name = "Secondary Parameter"; c2.type = "Knob"; c2.steps = 4;
                    testEditorConfig.controls.push_back(c1);
                    testEditorConfig.controls.push_back(c2);
                }
            }

            txtTestName.setText(testEditorConfig.testName);
            
            if (testEditorConfig.stimulusType == audio::StimulusType::LogFarinaSweep) comboStimulusType.setSelectedId(1, juce::dontSendNotification);
            else if (testEditorConfig.stimulusType == audio::StimulusType::AmplitudeRamp) comboStimulusType.setSelectedId(2, juce::dontSendNotification);
            else if (testEditorConfig.stimulusType == audio::StimulusType::SyncPulses3) comboStimulusType.setSelectedId(3, juce::dontSendNotification);
            else if (testEditorConfig.stimulusType == audio::StimulusType::SineWave1kHz) comboStimulusType.setSelectedId(4, juce::dontSendNotification);

            isUpdatingFromPreset = true;
            txtManualDuration.setText(juce::String(testEditorConfig.burstDurationSec, 2), juce::dontSendNotification);
            comboDurationPreset.setSelectedId(2, juce::dontSendNotification);
            txtManualDuration.setEnabled(false);
            isUpdatingFromPreset = false;

            btnAdaptiveTail.setToggleState(testEditorConfig.captureMode == "ADAPTIVE_ENVELOPE", juce::dontSendNotification);

            rebuildTestEditorControls();
            updateTestEditorEstimatedTime();
            layoutDrawerContent();
            repaint();
        }
    };
    contentComp.addChildComponent(comboTestPresets);

    lblTestName.setText("Test Name:", juce::dontSendNotification);
    lblTestName.setFont(juce::FontOptions(10.5f));
    lblTestName.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblTestName);

    txtTestName.setText("Custom Acoustic / Hardware Profile");
    contentComp.addChildComponent(txtTestName);

    lblStimulusType.setText("Stimulus Type:", juce::dontSendNotification);
    lblStimulusType.setFont(juce::FontOptions(10.5f));
    lblStimulusType.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblStimulusType);

    comboStimulusType.addItem("Farina Log Sweep (20 Hz - 40 kHz)", 1);
    comboStimulusType.addItem("Amplitude Ramp (-40 dBfs to 0 dBfs)", 2);
    comboStimulusType.addItem("Triple Sync Pulses (Gate Step)", 3);
    comboStimulusType.addItem("Pure Sine Wave (1 kHz Continuous)", 4);
    comboStimulusType.addItem("LCG White Noise (Broadband)", 5);
    comboStimulusType.setSelectedId(1, juce::dontSendNotification);
    comboStimulusType.onChange = [this] {
        int id = comboStimulusType.getSelectedId();
        if (id == 1) lblStimulusDesc.setText("Logarithmic swept-sine for frequency response, THD% and H2-H5 harmonics.", juce::dontSendNotification);
        else if (id == 2) lblStimulusDesc.setText("Stepped amplitude ramps for non-linear saturation and compression knees.", juce::dontSendNotification);
        else if (id == 3) lblStimulusDesc.setText("Gate step pulses for attack, decay, sustain and release envelope timings.", juce::dontSendNotification);
        else if (id == 4) lblStimulusDesc.setText("Stable 1 kHz tone for LFO rate, chorus depth, wow & flutter, and thermal drift.", juce::dontSendNotification);
        else lblStimulusDesc.setText("Broadband Gaussian-distributed white noise for spectral density mapping.", juce::dontSendNotification);
    };
    contentComp.addChildComponent(comboStimulusType);

    lblStimulusDesc.setText("Logarithmic swept-sine for frequency response, THD% and H2-H5 harmonics.", juce::dontSendNotification);
    lblStimulusDesc.setFont(juce::FontOptions(9.5f, juce::Font::italic));
    lblStimulusDesc.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblStimulusDesc);

    lblDurationSection.setText("2. BURST DURATION & ADAPTIVE CAPTURE", juce::dontSendNotification);
    lblDurationSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblDurationSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblDurationSection);

    comboDurationPreset.addItem("0.5s (Fast Check / Rapid Calibration)", 1);
    comboDurationPreset.addItem("1.0s (Standard Lab Precision - Recommended)", 2);
    comboDurationPreset.addItem("2.5s (High SNR & Harmonic Separation)", 3);
    comboDurationPreset.addItem("5.0s (Ultra Precision & Thermal Drift)", 4);
    comboDurationPreset.addItem("10.0s (Long Integration / Modulated Pads)", 5);
    comboDurationPreset.addItem("Custom Manual Duration...", 6);
    comboDurationPreset.setSelectedId(2, juce::dontSendNotification);
    comboDurationPreset.onChange = [this] {
        int id = comboDurationPreset.getSelectedId();
        isUpdatingFromPreset = true;
        if (id == 1) { txtManualDuration.setText("0.5", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 2) { txtManualDuration.setText("1.0", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 3) { txtManualDuration.setText("2.5", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 4) { txtManualDuration.setText("5.0", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 5) { txtManualDuration.setText("10.0", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 6) { txtManualDuration.setEnabled(true); txtManualDuration.grabKeyboardFocus(); }
        isUpdatingFromPreset = false;
        updateTestEditorEstimatedTime();
    };
    contentComp.addChildComponent(comboDurationPreset);

    lblManualDuration.setText("Custom Duration (s):", juce::dontSendNotification);
    lblManualDuration.setFont(juce::FontOptions(10.5f));
    lblManualDuration.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblManualDuration);

    txtManualDuration.setText("1.0");
    txtManualDuration.setEnabled(false);
    txtManualDuration.onTextChange = [this] {
        if (!isUpdatingFromPreset)
        {
            if (comboDurationPreset.getSelectedId() != 6)
                comboDurationPreset.setSelectedId(6, juce::dontSendNotification);
        }
        updateTestEditorEstimatedTime();
    };
    contentComp.addChildComponent(txtManualDuration);

    lblSecondsUnit.setText("sec", juce::dontSendNotification);
    lblSecondsUnit.setFont(juce::FontOptions(10.5f));
    lblSecondsUnit.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblSecondsUnit);

    btnAdaptiveTail.setTooltip("Automatically stops capture when envelope or reverb tail drops below -60 dBfs");
    btnAdaptiveTail.onClick = [this] { updateTestEditorEstimatedTime(); };
    contentComp.addChildComponent(btnAdaptiveTail);

    lblMatrixSection.setText("3. MEASUREMENT MATRIX RESOLUTION (SET STEPS PER CONTROL)", juce::dontSendNotification);
    lblMatrixSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblMatrixSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblMatrixSection);

    lblTestSummary.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblTestSummary.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    contentComp.addChildComponent(lblTestSummary);

    // ==========================================
    // 3. Audio Setup & Telemetry Setup
    // ==========================================
    auto setupLbl = [this](juce::Label& lbl, const juce::String& text, bool bold) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::FontOptions(10.5f, bold ? juce::Font::bold : juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, bold ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
        contentComp.addChildComponent(lbl);
    };

    lblSetupTargetSection.setText("1. ACTIVE SESSION TARGET & HARDWARE", juce::dontSendNotification);
    lblSetupTargetSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSetupTargetSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblSetupTargetSection);

    contentComp.addChildComponent(setupImgDisplay);

    lblSetupTargetHwName.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    lblSetupTargetHwName.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblSetupTargetHwName);

    lblSetupTargetSubmodule.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSetupTargetSubmodule.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    contentComp.addChildComponent(lblSetupTargetSubmodule);

    lblSetupTargetRouting.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    lblSetupTargetRouting.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblSetupTargetRouting);

    lblSetupAudioSection.setText("2. AUDIO INTERFACE & MIDI TELEMETRY", juce::dontSendNotification);
    lblSetupAudioSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSetupAudioSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblSetupAudioSection);

    setupLbl(lblSetupAudioDevice, "Active Audio Interface:", true);
    setupLbl(lblSetupAudioDeviceVal, "Windows Audio (Default)", false);
    setupLbl(lblSetupSampleRate, "Sample Rate:", true);
    setupLbl(lblSetupSampleRateVal, "96,000 Hz", false);
    setupLbl(lblSetupLatency, "Buffer Size / Latency:", true);
    setupLbl(lblSetupLatencyVal, "256 samples (2.67 ms)", false);
    setupLbl(lblSetupMidiInput, "MIDI Input Device:", true);
    setupLbl(lblSetupMidiInputVal, "None", false);
    setupLbl(lblSetupMidiOutput, "MIDI Output Device:", true);
    setupLbl(lblSetupMidiOutputVal, "None", false);

    btnSetupAudioMidi.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnSetupAudioMidi.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnSetupAudioMidi.onClick = [this] { if (onOpenAudioSettingsClicked) onOpenAudioSettingsClicked(); };
    contentComp.addChildComponent(btnSetupAudioMidi);

    btnSetupAbout.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnSetupAbout.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnSetupAbout.onClick = [this] { if (onAboutClicked) onAboutClicked(); };
    contentComp.addChildComponent(btnSetupAbout);

    // ==========================================
    // Bottom Action Bar
    // ==========================================
    panel.addAndMakeVisible(bottomBar);

    btnCancel.onClick = [this] { closeDrawer(); };
    bottomBar.addAndMakeVisible(btnCancel);

    btnConfirm.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillBlackBg);
    btnConfirm.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnConfirm.onClick = [this] {
        if (currentViewMode == DrawerViewMode::HardwareAndRouting)
        {
            if (!isHardwareLocked)
            {
                int selHw = hwModeCombo.getSelectedId();
                int selFunc = hwFunctionCombo.getSelectedId();
                if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
                {
                    const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
                    juce::String funcId;
                    if (selFunc >= 1 && selFunc <= static_cast<int>(hw.functions.size()))
                        funcId = hw.functions[static_cast<size_t>(selFunc - 1)].id;
                    
                    setHardwareLocked(true);

                    if (onHardwareSelected)
                        onHardwareSelected(hw.id, funcId);
                }
            }
        }
        else if (currentViewMode == DrawerViewMode::TestAndParametersEditor)
        {
            testEditorConfig.testName = txtTestName.getText();
            int stimId = comboStimulusType.getSelectedId();
            if (stimId == 1) testEditorConfig.stimulusType = audio::StimulusType::LogFarinaSweep;
            else if (stimId == 2) testEditorConfig.stimulusType = audio::StimulusType::AmplitudeRamp;
            else if (stimId == 3) testEditorConfig.stimulusType = audio::StimulusType::SyncPulses3;
            else if (stimId == 4) testEditorConfig.stimulusType = audio::StimulusType::SineWave1kHz;
            else testEditorConfig.stimulusType = audio::StimulusType::Silence;

            float parsedDuration = txtManualDuration.getText().getFloatValue();
            testEditorConfig.burstDurationSec = std::clamp(parsedDuration, 0.05f, 300.0f);
            testEditorConfig.captureMode = btnAdaptiveTail.getToggleState() ? "ADAPTIVE_ENVELOPE" : "FIXED_TIME";

            for (size_t i = 0; i < testEditorRows.size() && i < testEditorConfig.controls.size(); ++i)
            {
                int sId = testEditorRows[i].combo->getSelectedId();
                if (sId == 99)
                {
                    int cVal = testEditorRows[i].txtCustomSteps->getText().getIntValue();
                    testEditorConfig.controls[i].steps = std::max(1, cVal);
                }
                else
                {
                    testEditorConfig.controls[i].steps = (sId > 0) ? sId : 1;
                }
            }

            if (onTestConfigConfirmed)
                onTestConfigConfirmed(testEditorConfig, currentEditingTestIndex);
        }
        closeDrawer();
    };
    bottomBar.addAndMakeVisible(btnConfirm);
}

void SlideInDrawer::rebuildTestEditorControls()
{
    testEditorRows.clear();

    for (size_t i = 0; i < testEditorConfig.controls.size(); ++i)
    {
        const auto& ctrl = testEditorConfig.controls[i];
        TestEditorRowWidgets row;
        row.label = std::make_unique<juce::Label>();
        row.label->setText(ctrl.name + " (" + ctrl.type + "):", juce::dontSendNotification);
        row.label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
        row.label->setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        contentComp.addAndMakeVisible(row.label.get());

        row.combo = std::make_unique<juce::ComboBox>();
        row.combo->addItem("Fixed (1 step - default)", 1);
        row.combo->addItem("3 Steps (0%, 50%, 100%)", 3);
        row.combo->addItem("5 Steps (Standard: 0, 25, 50, 75, 100%)", 5);
        row.combo->addItem("8 Steps (Detailed: 8 steps)", 8);
        row.combo->addItem("16 Steps (High-Res: 16 steps)", 16);
        row.combo->addItem("32 Steps (Ultra High-Res: 32 steps)", 32);
        row.combo->addItem("64 Steps (Extreme: 64 steps)", 64);
        row.combo->addItem("Custom Steps (Manual)...", 99);
        contentComp.addAndMakeVisible(row.combo.get());

        row.txtCustomSteps = std::make_unique<juce::TextEditor>();
        row.txtCustomSteps->setInputRestrictions(3, "0123456789");
        contentComp.addAndMakeVisible(row.txtCustomSteps.get());

        int initialStep = (ctrl.steps > 0) ? ctrl.steps : 1;
        bool isStandard = (initialStep == 1 || initialStep == 3 || initialStep == 5 || 
                           initialStep == 8 || initialStep == 16 || initialStep == 32 || initialStep == 64);

        if (isStandard)
        {
            row.combo->setSelectedId(initialStep, juce::dontSendNotification);
            row.txtCustomSteps->setText(juce::String(initialStep), juce::dontSendNotification);
            row.txtCustomSteps->setEnabled(false);
        }
        else
        {
            row.combo->setSelectedId(99, juce::dontSendNotification);
            row.txtCustomSteps->setText(juce::String(initialStep), juce::dontSendNotification);
            row.txtCustomSteps->setEnabled(true);
        }

        size_t rowIdx = i;
        row.combo->onChange = [this, rowIdx] {
            if (rowIdx < testEditorRows.size() && rowIdx < testEditorConfig.controls.size())
            {
                int sId = testEditorRows[rowIdx].combo->getSelectedId();
                if (sId == 99)
                {
                    testEditorRows[rowIdx].txtCustomSteps->setEnabled(true);
                    testEditorRows[rowIdx].txtCustomSteps->grabKeyboardFocus();
                    int cVal = testEditorRows[rowIdx].txtCustomSteps->getText().getIntValue();
                    testEditorConfig.controls[rowIdx].steps = std::max(1, cVal);
                }
                else
                {
                    testEditorRows[rowIdx].txtCustomSteps->setText(juce::String(sId), juce::dontSendNotification);
                    testEditorRows[rowIdx].txtCustomSteps->setEnabled(false);
                    testEditorConfig.controls[rowIdx].steps = sId;
                }
                updateTestEditorEstimatedTime();
            }
        };

        row.txtCustomSteps->onTextChange = [this, rowIdx] {
            if (rowIdx < testEditorRows.size() && rowIdx < testEditorConfig.controls.size())
            {
                int cVal = testEditorRows[rowIdx].txtCustomSteps->getText().getIntValue();
                if (testEditorRows[rowIdx].combo->getSelectedId() != 99)
                {
                    testEditorRows[rowIdx].combo->setSelectedId(99, juce::dontSendNotification);
                }
                testEditorConfig.controls[rowIdx].steps = std::max(1, cVal);
                updateTestEditorEstimatedTime();
            }
        };

        testEditorRows.push_back(std::move(row));
    }
}

void SlideInDrawer::updateTestEditorEstimatedTime()
{
    int total = 1;
    juce::String formulaStr;
    int activeCount = 0;

    for (const auto& ctrl : testEditorConfig.controls)
    {
        if (ctrl.steps > 1)
        {
            total *= ctrl.steps;
            if (formulaStr.isNotEmpty()) formulaStr += " x ";
            formulaStr += juce::String(ctrl.steps);
            activeCount++;
        }
    }

    if (activeCount == 0)
    {
        total = 1;
        formulaStr = "1 (Fixed Reference)";
    }

    float dur = txtManualDuration.getText().getFloatValue();
    if (dur <= 0.01f) dur = 1.0f;

    double estSec = total * (static_cast<double>(dur) + 0.35);
    int estMin = static_cast<int>(estSec / 60.0);
    int estRemSec = static_cast<int>(estSec) % 60;

    juce::String timeStr;
    if (estMin > 0)
        timeStr = juce::String(estMin) + "m " + juce::String(estRemSec) + "s";
    else
        timeStr = juce::String(estRemSec) + "s";

    juce::String adaptiveStr = btnAdaptiveTail.getToggleState() ? " [Adaptive Auto-Tail]" : "";
    lblTestSummary.setText("Total: " + formulaStr + " = " + juce::String(total) + " measurements (~" + timeStr + ")" + adaptiveStr, juce::dontSendNotification);
}

void SlideInDrawer::switchViewMode(DrawerViewMode mode)
{
    currentViewMode = mode;

    // View 0: File & Session
    bool isFile = (mode == DrawerViewMode::FileSessionAndStorage);
    lblFileSection.setVisible(isFile);
    btnFileNew.setVisible(isFile);
    btnFileOpen.setVisible(isFile);
    btnFileSave.setVisible(isFile);
    btnFileSaveAs.setVisible(isFile);
    lblFileExportSection.setVisible(isFile);
    lblFileExportPathVal.setVisible(isFile);
    btnFileChangeExport.setVisible(isFile);
    btnFileRevealExport.setVisible(isFile);
    btnFileExit.setVisible(isFile);

    // View 1: Hardware & Routing
    bool isHw = (mode == DrawerViewMode::HardwareAndRouting);
    imgDisplay.setVisible(isHw);
    lblHardwareLockedBanner.setVisible(isHw && isHardwareLocked);
    lblHwTitle.setVisible(isHw);
    lblSelectHw.setVisible(isHw);
    hwModeCombo.setVisible(isHw);
    hwModeCombo.setEnabled(!isHardwareLocked);
    lblSelectFunc.setVisible(isHw);
    hwFunctionCombo.setVisible(isHw);
    hwFunctionCombo.setEnabled(!isHardwareLocked);
    cardWiring.setVisible(isHw);
    btnAutoDetect.setVisible(isHw);

    // View 2: Test & Parameter Editor
    bool isTest = (mode == DrawerViewMode::TestAndParametersEditor);
    lblPresetSelector.setVisible(isTest);
    comboTestPresets.setVisible(isTest);
    lblTestName.setVisible(isTest);
    txtTestName.setVisible(isTest);
    lblStimulusType.setVisible(isTest);
    comboStimulusType.setVisible(isTest);
    lblStimulusDesc.setVisible(isTest);
    lblDurationSection.setVisible(isTest);
    comboDurationPreset.setVisible(isTest);
    lblManualDuration.setVisible(isTest);
    txtManualDuration.setVisible(isTest);
    lblSecondsUnit.setVisible(isTest);
    btnAdaptiveTail.setVisible(isTest);
    lblMatrixSection.setVisible(isTest);
    lblTestSummary.setVisible(isTest);

    for (auto& row : testEditorRows)
    {
        if (row.label) row.label->setVisible(isTest);
        if (row.combo) row.combo->setVisible(isTest);
        if (row.txtCustomSteps) row.txtCustomSteps->setVisible(isTest);
    }

    // View 3: Setup & Telemetry
    bool isSetup = (mode == DrawerViewMode::EngineCalibrationAndInfo);
    lblSetupTargetSection.setVisible(isSetup);
    setupImgDisplay.setVisible(isSetup);
    lblSetupTargetHwName.setVisible(isSetup);
    lblSetupTargetSubmodule.setVisible(isSetup);
    lblSetupTargetRouting.setVisible(isSetup);
    lblSetupAudioSection.setVisible(isSetup);
    lblSetupAudioDevice.setVisible(isSetup);
    lblSetupAudioDeviceVal.setVisible(isSetup);
    lblSetupSampleRate.setVisible(isSetup);
    lblSetupSampleRateVal.setVisible(isSetup);
    lblSetupLatency.setVisible(isSetup);
    lblSetupLatencyVal.setVisible(isSetup);
    lblSetupMidiInput.setVisible(isSetup);
    lblSetupMidiInputVal.setVisible(isSetup);
    lblSetupMidiOutput.setVisible(isSetup);
    lblSetupMidiOutputVal.setVisible(isSetup);
    btnSetupAudioMidi.setVisible(isSetup);
    btnSetupAbout.setVisible(isSetup);

    if (mode == DrawerViewMode::HardwareAndRouting)
        btnConfirm.setButtonText(isHardwareLocked ? "Close" : "Accept Hardware Selection");
    else if (mode == DrawerViewMode::TestAndParametersEditor)
        btnConfirm.setButtonText(currentEditingTestIndex >= 0 ? "Save Changes" : "Add to Session Plan");
    else
        btnConfirm.setButtonText("Close");

    layoutDrawerContent();
    repaint();
}

void SlideInDrawer::openFileDrawer(const juce::String& currentExportPath)
{
    currentExportDirStr = currentExportPath;
    lblFileExportPathVal.setText(currentExportPath, juce::dontSendNotification);
    switchViewMode(DrawerViewMode::FileSessionAndStorage);
    openDrawer();
}

void SlideInDrawer::setHardwareLocked(bool locked)
{
    isHardwareLocked = locked;
    hwModeCombo.setEnabled(!locked);
    hwFunctionCombo.setEnabled(!locked);
    lblHardwareLockedBanner.setVisible(locked && currentViewMode == DrawerViewMode::HardwareAndRouting);
    if (currentViewMode == DrawerViewMode::HardwareAndRouting)
    {
        btnConfirm.setButtonText(locked ? "Close" : "Accept Hardware Selection");
        btnCancel.setVisible(!locked);
    }
    layoutDrawerContent();
    repaint();
}

void SlideInDrawer::openHardwareDrawer()
{
    updateBrandAndModelGraphics();
    switchViewMode(DrawerViewMode::HardwareAndRouting);
    btnCancel.setVisible(!isHardwareLocked);
    btnConfirm.setVisible(true);
    btnConfirm.setButtonText(isHardwareLocked ? "Close" : "Accept Hardware Selection");

    hwModeCombo.setEnabled(!isHardwareLocked);
    hwFunctionCombo.setEnabled(!isHardwareLocked);
    lblHardwareLockedBanner.setVisible(isHardwareLocked);

    openDrawer();
}

void SlideInDrawer::openTestEditorDrawer(const TestConfiguration& initialConfig, int editingIndex)
{
    testEditorConfig = initialConfig;
    currentEditingTestIndex = editingIndex;

    // Populate test preset dropdown from active hardware functions
    comboTestPresets.clear(juce::dontSendNotification);
    int selHw = hwModeCombo.getSelectedId();
    if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
    {
        const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
        for (size_t i = 0; i < hw.functions.size(); ++i)
        {
            comboTestPresets.addItem(hw.functions[i].name + " (" + hw.functions[i].blockType + ")", static_cast<int>(i + 1));
        }
    }
    comboTestPresets.addItem("Custom Test Profile...", 999);

    int matchedId = 999;
    if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
    {
        const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
        for (size_t i = 0; i < hw.functions.size(); ++i)
        {
            if (testEditorConfig.testName.contains(hw.functions[i].name))
            {
                matchedId = static_cast<int>(i + 1);
                break;
            }
        }
    }
    comboTestPresets.setSelectedId(matchedId, juce::dontSendNotification);

    txtTestName.setText(testEditorConfig.testName);

    if (testEditorConfig.stimulusType == audio::StimulusType::LogFarinaSweep) comboStimulusType.setSelectedId(1, juce::dontSendNotification);
    else if (testEditorConfig.stimulusType == audio::StimulusType::AmplitudeRamp) comboStimulusType.setSelectedId(2, juce::dontSendNotification);
    else if (testEditorConfig.stimulusType == audio::StimulusType::SyncPulses3) comboStimulusType.setSelectedId(3, juce::dontSendNotification);
    else if (testEditorConfig.stimulusType == audio::StimulusType::SineWave1kHz) comboStimulusType.setSelectedId(4, juce::dontSendNotification);

    isUpdatingFromPreset = true;
    float dur = testEditorConfig.burstDurationSec;
    if (std::abs(dur - 0.5f) < 0.01f) comboDurationPreset.setSelectedId(1, juce::dontSendNotification);
    else if (std::abs(dur - 1.0f) < 0.01f) comboDurationPreset.setSelectedId(2, juce::dontSendNotification);
    else if (std::abs(dur - 2.5f) < 0.01f) comboDurationPreset.setSelectedId(3, juce::dontSendNotification);
    else if (std::abs(dur - 5.0f) < 0.01f) comboDurationPreset.setSelectedId(4, juce::dontSendNotification);
    else if (std::abs(dur - 10.0f) < 0.01f) comboDurationPreset.setSelectedId(5, juce::dontSendNotification);
    else comboDurationPreset.setSelectedId(6, juce::dontSendNotification);

    txtManualDuration.setText(juce::String(dur, 2), juce::dontSendNotification);
    txtManualDuration.setEnabled(comboDurationPreset.getSelectedId() == 6);
    isUpdatingFromPreset = false;

    btnAdaptiveTail.setToggleState(testEditorConfig.captureMode == "ADAPTIVE_ENVELOPE", juce::dontSendNotification);

    rebuildTestEditorControls();
    updateTestEditorEstimatedTime();

    switchViewMode(DrawerViewMode::TestAndParametersEditor);
    openDrawer();
}

void SlideInDrawer::openSetupDrawer(const TelemetryInfo& info)
{
    telemetryInfo = info;
    lblSetupAudioDeviceVal.setText(info.audioDeviceName, juce::dontSendNotification);
    lblSetupSampleRateVal.setText(juce::String(info.sampleRate, 0) + " Hz", juce::dontSendNotification);
    lblSetupLatencyVal.setText(juce::String(info.bufferSize) + " samples (" + juce::String(info.latencyMs, 2) + " ms)", juce::dontSendNotification);
    lblSetupMidiInputVal.setText(info.midiInputName.isNotEmpty() ? info.midiInputName : "None (Manual / Mock)", juce::dontSendNotification);
    lblSetupMidiOutputVal.setText(info.midiOutputName.isNotEmpty() ? info.midiOutputName : "None (Manual / Mock)", juce::dontSendNotification);

    // Update active hardware and submodule info
    lblSetupTargetHwName.setText(getActiveHardwareDisplayName(), juce::dontSendNotification);
    lblSetupTargetSubmodule.setText("Active Submodule: " + getActiveFunctionDisplayName(), juce::dontSendNotification);

    if (cardWiring.stimulusText.isNotEmpty() || cardWiring.responseText.isNotEmpty())
    {
        lblSetupTargetRouting.setText(cardWiring.stimulusText + "  |  " + cardWiring.responseText, juce::dontSendNotification);
    }
    else
    {
        lblSetupTargetRouting.setText("Routing: Self-Contained / Direct Loopback", juce::dontSendNotification);
    }

    switchViewMode(DrawerViewMode::EngineCalibrationAndInfo);
    openDrawer();
}

void SlideInDrawer::openDrawer()
{
    isOpen = true;
    setVisible(true);
    toFront(true);
    startTimer(16);
}

void SlideInDrawer::closeDrawer()
{
    isOpen = false;
    startTimer(16);
}

void SlideInDrawer::timerCallback()
{
    const float speed = 0.16f;
    if (isOpen)
    {
        currentAnimationPos += (1.0f - currentAnimationPos) * speed;
        if (currentAnimationPos > 0.99f)
        {
            currentAnimationPos = 1.0f;
            stopTimer();
        }
    }
    else
    {
        currentAnimationPos += (0.0f - currentAnimationPos) * speed;
        if (currentAnimationPos < 0.01f)
        {
            currentAnimationPos = 0.0f;
            stopTimer();
            setVisible(false);
        }
    }
    resized();
    repaint();
}

float SlideInDrawer::getResponsivePanelWidth() const
{
    return juce::jlimit(420.0f, 680.0f, static_cast<float>(getWidth()) * 0.52f);
}

void SlideInDrawer::setHardwareList(const std::vector<HardwareItem>& list)
{
    hardwareList = list;
    hwModeCombo.clear(juce::dontSendNotification);

    for (size_t i = 0; i < hardwareList.size(); ++i)
    {
        hwModeCombo.addItem(hardwareList[i].displayName, static_cast<int>(i + 1));
    }

    if (!hardwareList.empty())
    {
        hwModeCombo.setSelectedId(1, juce::sendNotification);
    }
}

void SlideInDrawer::setSelectedHardwareId(const juce::String& id)
{
    for (size_t i = 0; i < hardwareList.size(); ++i)
    {
        if (hardwareList[i].id == id)
        {
            hwModeCombo.setSelectedId(static_cast<int>(i + 1), juce::sendNotification);
            break;
        }
    }
}

juce::String SlideInDrawer::getSelectedHardwareId() const
{
    int selId = hwModeCombo.getSelectedId();
    if (selId >= 1 && selId <= static_cast<int>(hardwareList.size()))
    {
        return hardwareList[static_cast<size_t>(selId - 1)].id;
    }
    return {};
}

juce::String SlideInDrawer::getSelectedFunctionId() const
{
    int selHw = hwModeCombo.getSelectedId();
    int selFunc = hwFunctionCombo.getSelectedId();
    if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
    {
        const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
        if (selFunc >= 1 && selFunc <= static_cast<int>(hw.functions.size()))
        {
            return hw.functions[static_cast<size_t>(selFunc - 1)].id;
        }
    }
    return {};
}

juce::String SlideInDrawer::getActiveHardwareDisplayName() const
{
    int selId = hwModeCombo.getSelectedId();
    if (selId >= 1 && selId <= static_cast<int>(hardwareList.size()))
    {
        return hardwareList[static_cast<size_t>(selId - 1)].displayName;
    }
    return "No Hardware Selected";
}

juce::String SlideInDrawer::getActiveFunctionDisplayName() const
{
    int selHw = hwModeCombo.getSelectedId();
    int selFunc = hwFunctionCombo.getSelectedId();
    if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
    {
        const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
        if (selFunc >= 1 && selFunc <= static_cast<int>(hw.functions.size()))
        {
            return hw.functions[static_cast<size_t>(selFunc - 1)].name;
        }
    }
    return "Default Profile";
}

audio::StimulusType SlideInDrawer::getSelectedStimulusType() const
{
    return testEditorConfig.stimulusType;
}

float SlideInDrawer::getBurstDurationSeconds() const
{
    return testEditorConfig.burstDurationSec;
}

bool SlideInDrawer::isAdaptiveEnvelopeMode() const
{
    return testEditorConfig.captureMode == "ADAPTIVE_ENVELOPE";
}

int SlideInDrawer::getPrimaryControlSteps() const
{
    if (!testEditorConfig.controls.empty())
        return testEditorConfig.controls[0].steps > 0 ? testEditorConfig.controls[0].steps : 1;
    return 8;
}

int SlideInDrawer::getSecondaryControlSteps() const
{
    if (testEditorConfig.controls.size() > 1)
        return testEditorConfig.controls[1].steps > 0 ? testEditorConfig.controls[1].steps : 1;
    return 4;
}

void SlideInDrawer::updateFunctionSelectionUI(const HardwareItem& item)
{
    hwFunctionCombo.clear(juce::dontSendNotification);

    for (size_t i = 0; i < item.functions.size(); ++i)
    {
        hwFunctionCombo.addItem(item.functions[i].name, static_cast<int>(i + 1));
    }

    if (!item.functions.empty())
    {
        hwFunctionCombo.setSelectedId(1, juce::sendNotification);
    }
}

void SlideInDrawer::updateBrandAndModelGraphics()
{
    int selId = hwModeCombo.getSelectedId();
    if (selId < 1 || selId > static_cast<int>(hardwareList.size())) return;

    const auto& item = hardwareList[static_cast<size_t>(selId - 1)];
    currentHwBrand = item.brand;

    brandLogoDrawable.reset();
    if (item.brandLogo.isNotEmpty())
    {
        auto brandFile = locateAssetFile(item.brandLogo);
        if (brandFile.existsAsFile())
        {
            brandLogoDrawable = juce::Drawable::createFromImageDataStream(*brandFile.createInputStream());
        }
    }

    modelSvgDrawable.reset();
    modelRasterImage = juce::Image();
    if (item.modelImage.isNotEmpty())
    {
        auto modelFile = locateAssetFile(item.modelImage);
        if (modelFile.existsAsFile())
        {
            if (modelFile.getFileExtension().equalsIgnoreCase(".svg"))
            {
                modelSvgDrawable = juce::Drawable::createFromImageDataStream(*modelFile.createInputStream());
            }
            else
            {
                modelRasterImage = juce::ImageFileFormat::loadFrom(modelFile);
            }
        }
    }

    imgDisplay.repaint();
    cardWiring.repaint();
}

void SlideInDrawer::ImageDisplayComponent::paint(juce::Graphics& g)
{
    auto renderArea = getLocalBounds().toFloat();
    if (owner.modelSvgDrawable != nullptr)
    {
        owner.modelSvgDrawable->drawWithin(g, renderArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
    else if (owner.modelRasterImage.isValid())
    {
        g.drawImage(owner.modelRasterImage, renderArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
}

void SlideInDrawer::WiringGuideCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    auto content = bounds.reduced(14.0f, 10.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("PHYSICAL AUDIO LOOPBACK ROUTING", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    content.removeFromTop(4.0f);
    g.setFont(juce::FontOptions(10.5f));
    auto outRow = content.removeFromTop(16.0f);
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText("Stimulus Out:  ", outRow.removeFromLeft(105.0f), juce::Justification::centredLeft, true);
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(stimulusText.isNotEmpty() ? stimulusText : "DAC Output 1 (L) -> Hardware Audio In", outRow, juce::Justification::centredLeft, true);

    auto inRow = content.removeFromTop(16.0f);
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText("Response In:   ", inRow.removeFromLeft(105.0f), juce::Justification::centredLeft, true);
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(responseText.isNotEmpty() ? responseText : "Hardware Audio Out -> ADC Input 1 (L)", inRow, juce::Justification::centredLeft, true);
}

void SlideInDrawer::layoutDrawerContent()
{
    float panelWidth = getResponsivePanelWidth();
    float drawerX = (currentAnimationPos - 1.0f) * panelWidth;

    panel.setBounds(static_cast<int>(drawerX), 0, static_cast<int>(panelWidth), getHeight());
    btnClose.setBounds(static_cast<int>(panelWidth) - 36, 12, 24, 24);

    int bottomH = 58;
    bottomBar.setBounds(0, getHeight() - bottomH, static_cast<int>(panelWidth), bottomH);
    btnCancel.setBounds(24, 12, 110, 34);
    btnConfirm.setBounds(static_cast<int>(panelWidth) - 200, 12, 176, 34);

    int viewTop = 44;
    int viewHeight = getHeight() - viewTop - bottomH;
    viewport.setBounds(0, viewTop, static_cast<int>(panelWidth), viewHeight);

    int padX = 24;
    int contentW = static_cast<int>(panelWidth) - 48;
    int y = 8;

    if (currentViewMode == DrawerViewMode::FileSessionAndStorage)
    {
        lblFileSection.setBounds(padX, y, contentW, 16);
        y += 22;

        btnFileNew.setBounds(padX, y, contentW, 30);
        y += 36;
        btnFileOpen.setBounds(padX, y, contentW, 30);
        y += 36;
        btnFileSave.setBounds(padX, y, contentW, 30);
        y += 36;
        btnFileSaveAs.setBounds(padX, y, contentW, 30);
        y += 44;

        lblFileExportSection.setBounds(padX, y, contentW, 16);
        y += 20;
        lblFileExportPathVal.setBounds(padX + 2, y, contentW - 2, 20);
        y += 24;

        btnFileChangeExport.setBounds(padX, y, contentW, 30);
        y += 36;
        btnFileRevealExport.setBounds(padX, y, contentW, 30);
        y += 48;

        btnFileExit.setBounds(padX, y, contentW, 32);
        y += 40;
    }
    else if (currentViewMode == DrawerViewMode::HardwareAndRouting)
    {
        imgDisplay.setBounds(padX, y, contentW, 160);
        y += 168;

        if (isHardwareLocked)
        {
            lblHardwareLockedBanner.setBounds(padX, y, contentW, 36);
            y += 42;
        }

        lblHwTitle.setBounds(padX, y, contentW, 20);
        y += 24;

        lblSelectHw.setBounds(padX, y, contentW, 16);
        y += 18;
        hwModeCombo.setBounds(padX, y, contentW, 28);
        y += 34;

        lblSelectFunc.setBounds(padX, y, contentW, 16);
        y += 18;
        hwFunctionCombo.setBounds(padX, y, contentW, 28);
        y += 34;

        cardWiring.setBounds(padX, y, contentW, 72);
        y += 82;

        btnAutoDetect.setBounds(padX, y, contentW, 28);
        y += 34;
    }
    else if (currentViewMode == DrawerViewMode::TestAndParametersEditor)
    {
        lblPresetSelector.setBounds(padX, y, contentW, 16);
        y += 18;
        comboTestPresets.setBounds(padX, y, contentW, 28);
        y += 34;

        lblTestName.setBounds(padX, y, 90, 24);
        txtTestName.setBounds(padX + 95, y, contentW - 95, 24);
        y += 28;

        lblStimulusType.setBounds(padX, y, 90, 24);
        comboStimulusType.setBounds(padX + 95, y, contentW - 95, 24);
        y += 26;

        lblStimulusDesc.setBounds(padX + 95, y, contentW - 95, 14);
        y += 24;

        lblDurationSection.setBounds(padX, y, contentW, 16);
        y += 20;

        comboDurationPreset.setBounds(padX, y, contentW - 200, 26);
        lblManualDuration.setBounds(padX + contentW - 190, y, 105, 26);
        txtManualDuration.setBounds(padX + contentW - 80, y, 55, 26);
        lblSecondsUnit.setBounds(padX + contentW - 20, y, 20, 26);
        y += 30;

        btnAdaptiveTail.setBounds(padX, y, contentW, 20);
        y += 26;

        lblMatrixSection.setBounds(padX, y, contentW, 16);
        y += 20;

        int colW = (contentW - 16) / 2;
        for (size_t i = 0; i < testEditorRows.size(); ++i)
        {
            int col = static_cast<int>(i % 2);
            int curX = padX + col * (colW + 16);

            testEditorRows[i].label->setBounds(curX, y, colW, 15);
            int comboW = colW - 46;
            testEditorRows[i].combo->setBounds(curX, y + 16, comboW, 25);
            testEditorRows[i].txtCustomSteps->setBounds(curX + comboW + 4, y + 16, 42, 25);

            if (col == 1 || i == testEditorRows.size() - 1)
            {
                y += 46;
            }
        }
        y += 6;

        lblTestSummary.setBounds(padX, y, contentW, 18);
        y += 28;
    }
    else if (currentViewMode == DrawerViewMode::EngineCalibrationAndInfo)
    {
        lblSetupTargetSection.setBounds(padX, y, contentW, 16);
        y += 22;

        setupImgDisplay.setBounds(padX, y, contentW, 130);
        y += 136;

        lblSetupTargetHwName.setBounds(padX, y, contentW, 20);
        y += 22;

        lblSetupTargetSubmodule.setBounds(padX, y, contentW, 18);
        y += 20;

        lblSetupTargetRouting.setBounds(padX, y, contentW, 16);
        y += 28;

        lblSetupAudioSection.setBounds(padX, y, contentW, 16);
        y += 24;

        auto addRow = [&](juce::Label& lbl, juce::Label& val) {
            lbl.setBounds(padX, y, contentW, 15);
            val.setBounds(padX + 4, y + 16, contentW - 4, 18);
            y += 38;
        };

        addRow(lblSetupAudioDevice, lblSetupAudioDeviceVal);
        addRow(lblSetupSampleRate, lblSetupSampleRateVal);
        addRow(lblSetupLatency, lblSetupLatencyVal);
        addRow(lblSetupMidiInput, lblSetupMidiInputVal);
        addRow(lblSetupMidiOutput, lblSetupMidiOutputVal);

        y += 6;
        btnSetupAudioMidi.setBounds(padX, y, contentW, 28);
        y += 34;
        btnSetupAbout.setBounds(padX, y, contentW, 28);
        y += 36;
    }

    contentComp.setBounds(0, 0, static_cast<int>(panelWidth) - 10, y + 10);
}

void SlideInDrawer::mouseDown(const juce::MouseEvent& e)
{
    float panelWidth = getResponsivePanelWidth();
    float drawerX = (currentAnimationPos - 1.0f) * panelWidth;

    if (e.position.x > drawerX + panelWidth)
    {
        closeDrawer();
    }
}

void SlideInDrawer::resized()
{
    layoutDrawerContent();
}

void SlideInDrawer::paint(juce::Graphics& g)
{
    if (currentAnimationPos <= 0.001f) return;

    // 1. Dark dim overlay on main window
    g.fillAll(juce::Colours::black.withAlpha(0.35f * currentAnimationPos));

    float panelWidth = getResponsivePanelWidth();
    float drawerX = (currentAnimationPos - 1.0f) * panelWidth;

    // 2. Solid OPAQUE Panel Background (Pure White)
    g.setColour(juce::Colours::white);
    g.fillRect(drawerX, 0.0f, panelWidth, static_cast<float>(getHeight()));

    // 3. Bottom Action Bar Solid Opaque Background (Light Gray)
    float bottomH = 58.0f;
    g.setColour(juce::Colour(0xfff9fafb));
    g.fillRect(drawerX, static_cast<float>(getHeight()) - bottomH, panelWidth, bottomH);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawHorizontalLine(getHeight() - static_cast<int>(bottomH), drawerX, drawerX + panelWidth);

    // 4. Panel Right Edge & Soft Shadow
    g.setColour(SoundIdTheme::borderCard);
    g.drawVerticalLine(static_cast<int>(drawerX + panelWidth), 0.0f, static_cast<float>(getHeight()));

    g.setColour(juce::Colours::black.withAlpha(0.18f * currentAnimationPos));
    g.fillRect(drawerX + panelWidth, 0.0f, 6.0f, static_cast<float>(getHeight()));

    // 5. Header Area & Logos
    if (panel.isVisible())
    {
        auto headerRect = juce::Rectangle<float>(drawerX + 24.0f, 10.0f, panelWidth - 48.0f, 32.0f);
        auto topLogoArea = headerRect.removeFromLeft(200.0f);

        if (currentViewMode == DrawerViewMode::FileSessionAndStorage)
        {
            g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText("FILE & SESSION STORAGE", topLogoArea, juce::Justification::centredLeft, true);
        }
        else if (currentViewMode == DrawerViewMode::HardwareAndRouting)
        {
            if (brandLogoDrawable != nullptr)
            {
                brandLogoDrawable->drawWithin(g, topLogoArea, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
            }
            else if (currentHwBrand.isNotEmpty())
            {
                g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
                g.setColour(SoundIdTheme::textSecondary);
                g.drawText(currentHwBrand.toUpperCase(), topLogoArea, juce::Justification::centredLeft, true);
            }
        }
        else if (currentViewMode == DrawerViewMode::TestAndParametersEditor)
        {
            g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText("TEST & PARAMETER CONFIGURATION", topLogoArea, juce::Justification::centredLeft, true);
        }
        else
        {
            g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText("AUDIO SETUP & TELEMETRY", topLogoArea, juce::Justification::centredLeft, true);
        }
    }
}

} // namespace abdaudiolab::gui
