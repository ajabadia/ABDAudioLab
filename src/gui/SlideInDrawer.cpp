#include "SlideInDrawer.h"
#include "HardwareMidiDetect/HardwareMidiDetector.h"
#include "HardwareMidiDetect/JuceHardwareMidiPicker.h"
#include "HardwareMidiDetect/MidiHardwareBackend.h"
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

    // 2. Portable shared assets relative search
    juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::File sharedAssetsDir = exeDir.getChildFile("../ABDSharedAssets");
    if (!sharedAssetsDir.isDirectory())
        sharedAssetsDir = exeDir.getChildFile("../../ABDSharedAssets");
    if (!sharedAssetsDir.isDirectory())
        sharedAssetsDir = exeDir.getChildFile("../../../ABDSharedAssets");

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

class LabMidiHardwareBackend : public abd::hwid::MidiHardwareBackend,
                               private juce::MidiInputCallback
{
public:
    LabMidiHardwareBackend() = default;
    ~LabMidiHardwareBackend() override { stopListening(); }

    std::string getOutputPortName() const override
    {
        return activeOutDevice.name.toStdString();
    }

    void sendBytes(const std::vector<uint8_t>& bytes) override
    {
        if (outPort != nullptr && !bytes.empty())
        {
            auto msg = juce::MidiMessage::createSysExMessage(bytes.data(), static_cast<int>(bytes.size()));
            outPort->sendMessageNow(msg);
        }
    }

    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&)> cb) override
    {
        receiveCallback = std::move(cb);
    }

    void startListening() override
    {
        if (inPort == nullptr)
        {
            auto inDevs = juce::MidiInput::getAvailableDevices();
            if (!inDevs.isEmpty())
                inPort = juce::MidiInput::openDevice(inDevs[0].identifier, this);
            if (inPort != nullptr)
                inPort->start();
        }
    }

    void stopListening() override
    {
        if (inPort != nullptr)
        {
            inPort->stop();
            inPort.reset();
        }
    }

    void refreshPorts() override {}

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) override
    {
        if (receiveCallback && msg.isSysEx())
        {
            auto* data = msg.getSysExData();
            std::vector<uint8_t> bytes(data, data + msg.getSysExDataSize());
            juce::MessageManager::callAsync([this, bytes]() {
                if (receiveCallback) receiveCallback(bytes);
            });
        }
    }

    juce::MidiDeviceInfo activeOutDevice;
    std::unique_ptr<juce::MidiOutput> outPort;
    std::unique_ptr<juce::MidiInput> inPort;
    std::function<void(const std::vector<uint8_t>&)> receiveCallback;
};

class HardwarePickerWindow : public juce::DocumentWindow
{
public:
    HardwarePickerWindow(abd::hwid::MidiHardwareBackend& backend,
                         const std::vector<abd::hwid::HardwareContract>& contracts,
                         std::function<void(const abd::hwid::HardwarePickResult&)> onResult)
        : DocumentWindow("Hardware MIDI Auto-Detection (ABDSharedCode)",
                         juce::Colour(0xff12141c),
                         DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setResizeLimits(540, 480, 800, 700);

        auto* picker = new abd::hwid::JuceHardwareMidiPicker(
            backend,
            [this, onResult = std::move(onResult)](const abd::hwid::HardwarePickResult& res) {
                if (onResult)
                    onResult(res);
                setVisible(false);
            },
            contracts
        );

        setContentOwned(picker, true);
        centreWithSize(640, 520);
        setVisible(true);

        juce::Timer::callAfterDelay(400, [picker]() {
            if (picker != nullptr)
                picker->startPick();
        });
    }

    ~HardwarePickerWindow() override = default;

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwarePickerWindow)
};

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

    btnFileExportReport.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnFileExportReport.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    btnFileExportReport.onClick = [this] {
        if (onExportReportClicked) onExportReportClicked();
    };
    contentComp.addChildComponent(btnFileExportReport);

    btnFileExit.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfffee2e2));
    btnFileExit.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
    btnFileExit.onClick = [this] {
        if (onExitAppClicked) onExitAppClicked();
    };
    contentComp.addChildComponent(btnFileExit);

    btnCheckUpdates.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe0e7ff));
    btnCheckUpdates.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff3730a3));
    btnCheckUpdates.onClick = [this] {
        if (onCheckUpdatesClicked) onCheckUpdatesClicked();
    };
    contentComp.addChildComponent(btnCheckUpdates);

    // File Previewer Controls
    lblFilePreviewSection.setText("PREVIEW EXPORTED CODE & DATA (.H / .JSON)", juce::dontSendNotification);
    lblFilePreviewSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblFilePreviewSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    contentComp.addChildComponent(lblFilePreviewSection);

    comboPreviewFiles.onChange = [this] {
        int selId = comboPreviewFiles.getSelectedId();
        if (selId >= 1)
        {
            juce::File expDir(currentExportFolderPath);
            auto files = expDir.findChildFiles(juce::File::findFiles, false, "*.h;*.json;*.abdlabtest");
            if (selId - 1 < static_cast<int>(files.size()))
            {
                auto targetFile = files[static_cast<size_t>(selId - 1)];
                txtCodePreview.setText(targetFile.loadFileAsString().substring(0, 8000));
            }
        }
    };
    contentComp.addChildComponent(comboPreviewFiles);

    txtCodePreview.setMultiLine(true);
    txtCodePreview.setReadOnly(true);
    txtCodePreview.setFont(juce::FontOptions("Consolas", 11.0f, juce::Font::plain));
    txtCodePreview.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e));
    txtCodePreview.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd4d4d4));
    contentComp.addChildComponent(txtCodePreview);

    btnCopyPreview.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnCopyPreview.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnCopyPreview.onClick = [this] {
        juce::SystemClipboard::copyTextToClipboard(txtCodePreview.getText());
    };
    contentComp.addChildComponent(btnCopyPreview);

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

    lblAutoDetectSection.setText("AUTOMATED HARDWARE DETECTION", juce::dontSendNotification);
    lblAutoDetectSection.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    lblAutoDetectSection.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    contentComp.addChildComponent(lblAutoDetectSection);

    lblOrSeparator.setText(juce::String::fromUTF8(u8"— OR SELECT MANUALLY FROM CATALOG —"), juce::dontSendNotification);
    lblOrSeparator.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    lblOrSeparator.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    lblOrSeparator.setJustificationType(juce::Justification::centred);
    contentComp.addChildComponent(lblOrSeparator);

    btnAutoDetect.setTooltip("Automatically query connected MIDI ports via SysEx Identity Inquiry to identify hardware (WebUI)");
    btnAutoDetect.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillWhiteBg);
    btnAutoDetect.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAutoDetect.setEnabled(true);
    btnAutoDetect.onClick = [this] {
        btnAutoDetect.setButtonText("Opening Hardware Detector (WebUI)...");

        // Convert availableContracts to shared abd::hwid::HardwareContract
        std::vector<abd::hwid::HardwareContract> sharedContracts;
        sharedContracts.reserve(availableContracts.size());
        for (const auto& c : availableContracts)
        {
            abd::hwid::HardwareContract hc;
            hc.id = c.id;
            hc.displayName = c.displayName;
            hc.description = c.description;
            hc.deviceType = c.deviceType;
            hc.brand = c.brand;
            hc.brandLogo = c.brandLogo;
            hc.modelImage = c.modelImage;
            hc.manufacturer = c.manufacturer;
            hc.model = c.model;
            hc.modelIdHex = c.modelIdHex;
            hc.autoDetectSysEx = c.autoDetectSysEx;
            hc.midiIdentity.manufacturer = c.midiIdentity.manufacturer;
            hc.midiIdentity.manufacturerIdHex = c.midiIdentity.manufacturerIdHex;
            hc.midiIdentity.model = c.midiIdentity.model;
            hc.midiIdentity.modelIdHex = c.midiIdentity.modelIdHex;
            hc.midiIdentity.familyIdHex = c.midiIdentity.familyIdHex;
            hc.midiIdentity.sysexHeaderHex = c.midiIdentity.sysexHeaderHex;
            hc.midiIdentity.portNameMatches = c.midiIdentity.portNameMatches;
            sharedContracts.push_back(hc);
        }

        if (midiBackend == nullptr)
            midiBackend = std::make_unique<LabMidiHardwareBackend>();

        pickerWindow = std::make_unique<HardwarePickerWindow>(
            *midiBackend,
            sharedContracts,
            [this](const abd::hwid::HardwarePickResult& res) {
                if (!res.cancelled && !res.hardwareId.empty())
                {
                    bool matched = false;
                    for (size_t i = 0; i < hardwareList.size(); ++i)
                    {
                        if (hardwareList[i].id.equalsIgnoreCase(juce::String(res.hardwareId)))
                        {
                            hwModeCombo.setSelectedId(static_cast<int>(i + 1), juce::sendNotification);
                            matched = true;
                            break;
                        }
                    }
                    btnAutoDetect.setButtonText(matched ? ("Detected: " + juce::String(res.displayName)) : "Device Selected");
                }
                else
                {
                    btnAutoDetect.setButtonText("Auto-Detect Device (MIDI / USB)");
                }
            }
        );
    };
    contentComp.addChildComponent(btnAutoDetect);

    // ==========================================
    // 2. Test & Parameter Editor Setup
    // ==========================================
    testEditorPanel.onConfigChanged = [this] {
        testEditorConfig = testEditorPanel.getConfiguration();
    };
    contentComp.addChildComponent(testEditorPanel);

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
            testEditorConfig = testEditorPanel.getConfiguration();

            if (onTestConfigConfirmed)
                onTestConfigConfirmed(testEditorConfig, currentEditingTestIndex);
        }
        closeDrawer();
    };
    bottomBar.addAndMakeVisible(btnConfirm);
}

void SlideInDrawer::rebuildTestEditorControls()
{
    testEditorPanel.setConfiguration(testEditorConfig);
}

void SlideInDrawer::updateTestEditorEstimatedTime()
{
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
    btnCheckUpdates.setVisible(isFile);

    lblFilePreviewSection.setVisible(isFile);
    comboPreviewFiles.setVisible(isFile);
    txtCodePreview.setVisible(isFile);
    btnCopyPreview.setVisible(isFile);

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
    btnAutoDetect.setVisible(isHw && !isHardwareLocked);
    lblAutoDetectSection.setVisible(isHw && !isHardwareLocked);
    lblOrSeparator.setVisible(isHw && !isHardwareLocked);

    // View 2: Test & Parameter Editor
    bool isTest = (mode == DrawerViewMode::TestAndParametersEditor);
    testEditorPanel.setVisible(isTest);

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
    currentExportFolderPath = currentExportPath;
    lblFileExportPathVal.setText(currentExportPath, juce::dontSendNotification);
    refreshFilePreviewList();
    switchViewMode(DrawerViewMode::FileSessionAndStorage);
    openDrawer();
}

void SlideInDrawer::refreshFilePreviewList()
{
    comboPreviewFiles.clear(juce::dontSendNotification);
    juce::File expDir(currentExportFolderPath);
    if (expDir.exists() && expDir.isDirectory())
    {
        auto files = expDir.findChildFiles(juce::File::findFiles, false, "*.h;*.json;*.abdlabtest");
        int itemId = 1;
        for (const auto& f : files)
        {
            comboPreviewFiles.addItem(f.getFileName(), itemId++);
        }
        if (itemId > 1)
        {
            comboPreviewFiles.setSelectedId(1, juce::sendNotification);
        }
        else
        {
            txtCodePreview.setText("// No exported files found in target folder.\n// Run a test suite to generate .h look-up tables and manifests.");
        }
    }
    else
    {
        txtCodePreview.setText("// Export folder does not exist yet.");
    }
}

void SlideInDrawer::setHardwareLocked(bool locked)
{
    isHardwareLocked = locked;
    hwModeCombo.setEnabled(!locked);
    hwFunctionCombo.setEnabled(!locked);
    lblHardwareLockedBanner.setVisible(locked && currentViewMode == DrawerViewMode::HardwareAndRouting);
    btnAutoDetect.setVisible(!locked && currentViewMode == DrawerViewMode::HardwareAndRouting);
    lblAutoDetectSection.setVisible(!locked && currentViewMode == DrawerViewMode::HardwareAndRouting);
    lblOrSeparator.setVisible(!locked && currentViewMode == DrawerViewMode::HardwareAndRouting);
    if (currentViewMode == DrawerViewMode::HardwareAndRouting)
    {
        btnConfirm.setButtonText(locked ? "Close" : "Accept Hardware Selection");
        btnCancel.setVisible(!locked);
    }
    layoutDrawerContent();
    repaint();
}

void SlideInDrawer::clearSelectedHardware()
{
    hwModeCombo.setSelectedId(0, juce::dontSendNotification);
    hwFunctionCombo.clear(juce::dontSendNotification);
    modelRasterImage = juce::Image();
    cardWiring.stimulusText = "Select hardware device to view routing";
    cardWiring.responseText = "";
    cardWiring.repaint();
    lblHwTitle.setText("No Hardware Selected", juce::dontSendNotification);
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

    testEditorPanel.setConfiguration(testEditorConfig);

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
    return juce::jlimit(540.0f, 820.0f, static_cast<float>(getWidth()) * 0.62f);
}

void SlideInDrawer::setHardwareList(const std::vector<HardwareItem>& list)
{
    hardwareList = list;
    hwModeCombo.clear(juce::dontSendNotification);

    hwModeCombo.setTextWhenNothingSelected("< Select Target Hardware >");
    for (size_t i = 0; i < hardwareList.size(); ++i)
    {
        hwModeCombo.addItem(hardwareList[i].displayName, static_cast<int>(i + 1));
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
        y += 18;
        lblFileExportPathVal.setBounds(padX + 2, y, contentW - 2, 18);
        y += 22;

        btnFileChangeExport.setBounds(padX, y, contentW, 30);
        y += 34;
        btnFileRevealExport.setBounds(padX, y, contentW, 30);
        y += 34;
        btnFileExportReport.setBounds(padX, y, contentW, 32);
        y += 40;

        lblFilePreviewSection.setBounds(padX, y, contentW, 16);
        y += 18;
        comboPreviewFiles.setBounds(padX, y, contentW, 28);
        y += 32;
        txtCodePreview.setBounds(padX, y, contentW, 110);
        y += 114;
        btnCopyPreview.setBounds(padX, y, contentW, 28);
        y += 34;

        btnCheckUpdates.setBounds(padX, y, contentW, 30);
        y += 36;

        btnFileExit.setBounds(padX, y, contentW, 32);
        y += 36;
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
        if (!isHardwareLocked)
        {
            lblAutoDetectSection.setBounds(padX, y, contentW, 16);
            y += 18;

            btnAutoDetect.setBounds(padX, y, contentW, 30);
            y += 34;

            lblOrSeparator.setBounds(padX, y, contentW, 16);
            y += 22;
        }

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
    }
    else if (currentViewMode == DrawerViewMode::TestAndParametersEditor)
    {
        testEditorPanel.setBounds(padX, y, contentW, testEditorPanel.getPreferredHeight());
        y += testEditorPanel.getPreferredHeight() + 10;
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
