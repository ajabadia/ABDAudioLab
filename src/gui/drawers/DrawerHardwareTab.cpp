/**
 * @file DrawerHardwareTab.cpp
 * @brief Implementation of the Hardware inspection, routing, and MIDI detection drawer tab.
 * @author ABDSynths
 * @date 2026
 */

#include "DrawerHardwareTab.h"
#include "HardwareMidiDetect/HardwareMidiDetector.h"
#include "HardwareMidiDetect/JuceHardwareMidiPicker.h"
#include "HardwareMidiDetect/MidiHardwareBackend.h"
#include "../../hardware/AudioLabMidiBackend.h"

namespace abdaudiolab::gui
{

class HardwarePickerWindow : public juce::DocumentWindow
{
public:
    HardwarePickerWindow(abd::hwid::MidiHardwareBackend& backend,
                         const std::vector<abd::hwid::HardwareContract>& contracts,
                         std::function<void(const abd::hwid::HardwarePickResult&)> onResult,
                         const abd::hwid::HardwareMidiDetector::DetectionConfig& config = {},
                         bool startVisible = true)
        : DocumentWindow("Hardware MIDI Auto-Detection (ABDSharedCode)",
                         AppTheme::BackgroundApp,
                         DocumentWindow::closeButton),
          resultCallback(std::move(onResult))
    {
        setUsingNativeTitleBar(false);
        setResizable(true, false);
        setResizeLimits(540, 480, 800, 700);

        auto* picker = new abd::hwid::JuceHardwareMidiPicker(
            backend,
            [this](const abd::hwid::HardwarePickResult& res) {
                if (resultCallback) resultCallback(res);
                setVisible(false);
            },
            contracts,
            config,
            AppTheme::currentMode == AppTheme::ThemeMode::Dark ? "dark" : "audiolab-light"
        );

        setContentOwned(picker, true);
        centreWithSize(620, 540);

        if (startVisible)
        {
            setVisible(true);
            juce::MessageManager::callAsync([picker]() {
                if (picker != nullptr)
                    picker->startPick();
            });
        }
        else
        {
            setVisible(false);
        }
    }

    ~HardwarePickerWindow() override = default;

    void closeButtonPressed() override
    {
        setVisible(false);
    }

    void showAndStartPick(std::function<void(const abd::hwid::HardwarePickResult&)> onResult = nullptr)
    {
        if (onResult)
            resultCallback = std::move(onResult);

        centreWithSize(620, 540);
        setVisible(true);
        toFront(true);

        if (auto* picker = dynamic_cast<abd::hwid::JuceHardwareMidiPicker*>(getContentComponent()))
        {
            picker->startPick();
        }
    }

    void updateTheme()
    {
        setBackgroundColour(AppTheme::BackgroundApp);
        if (auto* picker = dynamic_cast<abd::hwid::JuceHardwareMidiPicker*>(getContentComponent()))
        {
            picker->setTheme(AppTheme::currentMode == AppTheme::ThemeMode::Dark ? "dark" : "audiolab-light");
        }
        repaint();
    }

private:
    std::function<void(const abd::hwid::HardwarePickResult&)> resultCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwarePickerWindow)
};

// ==============================================================================
// DrawerHardwareTab Implementation
// ==============================================================================

DrawerHardwareTab::DrawerHardwareTab()
{
    addAndMakeVisible(imgDisplay);

    lblHardwareLockedBanner.setText("Hardware Profile Locked to active session.\nTo measure a different device or submodule, click below.", juce::dontSendNotification);
    lblHardwareLockedBanner.setFont(juce::FontOptions(10.0f, juce::Font::italic));
    lblHardwareLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblHardwareLockedBanner.setColour(juce::Label::backgroundColourId, SoundIdTheme::surfaceSubtle);
    lblHardwareLockedBanner.setJustificationType(juce::Justification::centred);
    lblHardwareLockedBanner.setVisible(false);
    addAndMakeVisible(lblHardwareLockedBanner);

    btnChangeHwOrNewFlow.setTooltip("Desbloquear selector para elegir otro sintetizador o submódulo e iniciar un nuevo flujo");
    btnChangeHwOrNewFlow.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnChangeHwOrNewFlow.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnChangeHwOrNewFlow.onClick = [this] {
        if (onNewFlowRequested != nullptr)
            onNewFlowRequested();
        else
            setHardwareLocked(false);
    };
    btnChangeHwOrNewFlow.setVisible(false);
    addAndMakeVisible(btnChangeHwOrNewFlow);

    lblHwTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    lblHwTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblHwTitle);

    lblSelectHw.setText("Hardware Model / Profile:", juce::dontSendNotification);
    lblSelectHw.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    lblSelectHw.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblSelectHw);

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
        resized();
        repaint();
    };
    addAndMakeVisible(hwModeCombo);

    lblSelectFunc.setText("Active Hardware Function / Block:", juce::dontSendNotification);
    lblSelectFunc.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    lblSelectFunc.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblSelectFunc);

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
        resized();
        repaint();
    };
    addAndMakeVisible(hwFunctionCombo);

    addAndMakeVisible(cardWiring);

    lblAutoDetectSection.setText("AUTOMATED HARDWARE DETECTION", juce::dontSendNotification);
    lblAutoDetectSection.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    lblAutoDetectSection.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblAutoDetectSection);

    lblOrSeparator.setText(juce::String::fromUTF8(u8"— OR SELECT MANUALLY FROM CATALOG —"), juce::dontSendNotification);
    lblOrSeparator.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    lblOrSeparator.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    lblOrSeparator.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblOrSeparator);

    btnAutoDetect.setTooltip("Automatically query connected MIDI ports via SysEx Identity Inquiry to identify hardware (WebUI)");
    btnAutoDetect.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillWhiteBg);
    btnAutoDetect.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAutoDetect.setEnabled(true);
    btnAutoDetect.onClick = [this] { openHardwarePickerModal(); };
    addAndMakeVisible(btnAutoDetect);
}

DrawerHardwareTab::~DrawerHardwareTab()
{
    if (pickerWindow != nullptr)
        pickerWindow.reset();
}

void DrawerHardwareTab::openHardwarePickerModal()
{
    auto onPickCallback = [this](const abd::hwid::HardwarePickResult& res) {
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
            if (onDeviceDetected && matched)
                onDeviceDetected(juce::String(res.displayName));
        }
        else
        {
            btnAutoDetect.setButtonText("Auto-Detect Device (MIDI / USB)");
        }
    };

    if (auto* hwWindow = dynamic_cast<HardwarePickerWindow*>(pickerWindow.get()))
    {
        btnAutoDetect.setButtonText("Detecting Hardware (MIDI / USB)...");
        hwWindow->showAndStartPick(std::move(onPickCallback));
        return;
    }

    btnAutoDetect.setButtonText("Opening Hardware Detector (WebUI)...");

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
        midiBackend = std::make_unique<hardware::AudioLabMidiBackend>();

    abd::hwid::HardwareMidiDetector::DetectionConfig detectionConfig;
    detectionConfig.allowedHardwareIds = {};
    detectionConfig.maxResults = 1;
    detectionConfig.autoSelectIfSingle = true;
    detectionConfig.includeHeuristic = true;
    detectionConfig.requireSysExVerified = false;

    pickerWindow = std::make_unique<HardwarePickerWindow>(
        *midiBackend,
        sharedContracts,
        std::move(onPickCallback),
        detectionConfig,
        true
    );
}

void DrawerHardwareTab::preWarmHardwarePicker()
{
    if (pickerWindow != nullptr)
        return;

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
        midiBackend = std::make_unique<hardware::AudioLabMidiBackend>();

    abd::hwid::HardwareMidiDetector::DetectionConfig detectionConfig;
    detectionConfig.allowedHardwareIds = {};
    detectionConfig.maxResults = 1;
    detectionConfig.autoSelectIfSingle = true;
    detectionConfig.includeHeuristic = true;
    detectionConfig.requireSysExVerified = false;

    pickerWindow = std::make_unique<HardwarePickerWindow>(
        *midiBackend,
        sharedContracts,
        nullptr,
        detectionConfig,
        false // Start invisible to pre-warm WebView2 runtime in background
    );
}

void DrawerHardwareTab::setHardwareList(const std::vector<HardwareItem>& list)
{
    hardwareList = list;
    hwModeCombo.clear(juce::dontSendNotification);
    for (size_t i = 0; i < hardwareList.size(); ++i)
    {
        hwModeCombo.addItem(hardwareList[i].displayName, static_cast<int>(i + 1));
    }
    hwModeCombo.setSelectedId(0, juce::dontSendNotification);
    clearSelectedHardware();
}

void DrawerHardwareTab::setContracts(std::vector<core::HardwareContract> contractsList)
{
    availableContracts = std::move(contractsList);
}

void DrawerHardwareTab::setSelectedHardwareId(const juce::String& id)
{
    for (size_t i = 0; i < hardwareList.size(); ++i)
    {
        if (hardwareList[i].id == id)
        {
            hwModeCombo.setSelectedId(static_cast<int>(i + 1), juce::sendNotification);
            return;
        }
    }
}

void DrawerHardwareTab::clearSelectedHardware()
{
    hwModeCombo.setSelectedId(0, juce::dontSendNotification);
    hwFunctionCombo.clear(juce::dontSendNotification);
    lblHwTitle.setText("", juce::dontSendNotification);
    brandLogoDrawable.reset();
    modelSvgDrawable.reset();
    modelRasterImage = juce::Image();
    currentHwBrand = "";
    cardWiring.stimulusText = "";
    cardWiring.responseText = "";
    btnAutoDetect.setButtonText("Auto-Detect Device (MIDI / USB)");
    imgDisplay.repaint();
    cardWiring.repaint();
}

void DrawerHardwareTab::setHardwareLocked(bool locked)
{
    isHardwareLocked = locked;
    hwModeCombo.setEnabled(!locked);
    hwFunctionCombo.setEnabled(!locked);
    btnAutoDetect.setEnabled(!locked);
    lblHardwareLockedBanner.setVisible(locked);
    btnChangeHwOrNewFlow.setVisible(locked);
    lblAutoDetectSection.setVisible(!locked);
    lblOrSeparator.setVisible(!locked);
    btnAutoDetect.setVisible(!locked);
    resized();
}

void DrawerHardwareTab::triggerAutoDetect()
{
    btnAutoDetect.triggerClick();
}

juce::String DrawerHardwareTab::getSelectedHardwareId() const
{
    int selId = hwModeCombo.getSelectedId();
    if (selId >= 1 && selId <= static_cast<int>(hardwareList.size()))
    {
        return hardwareList[static_cast<size_t>(selId - 1)].id;
    }
    return {};
}

juce::String DrawerHardwareTab::getSelectedFunctionId() const
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

juce::String DrawerHardwareTab::getActiveHardwareDisplayName() const
{
    int selId = hwModeCombo.getSelectedId();
    if (selId >= 1 && selId <= static_cast<int>(hardwareList.size()))
    {
        return hardwareList[static_cast<size_t>(selId - 1)].displayName;
    }
    return {};
}

juce::String DrawerHardwareTab::getActiveFunctionDisplayName() const
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
    return {};
}

float DrawerHardwareTab::getBurstDurationSeconds() const
{
    int selHw = hwModeCombo.getSelectedId();
    int selFunc = hwFunctionCombo.getSelectedId();
    if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
    {
        const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
        if (selFunc >= 1 && selFunc <= static_cast<int>(hw.functions.size()))
        {
            return hw.functions[static_cast<size_t>(selFunc - 1)].defaultBurstDurationSec;
        }
    }
    return 1.0f;
}

bool DrawerHardwareTab::isAdaptiveEnvelopeMode() const
{
    int selHw = hwModeCombo.getSelectedId();
    int selFunc = hwFunctionCombo.getSelectedId();
    if (selHw >= 1 && selHw <= static_cast<int>(hardwareList.size()))
    {
        const auto& hw = hardwareList[static_cast<size_t>(selHw - 1)];
        if (selFunc >= 1 && selFunc <= static_cast<int>(hw.functions.size()))
        {
            return hw.functions[static_cast<size_t>(selFunc - 1)].captureMode.equalsIgnoreCase("ADAPTIVE_ENVELOPE");
        }
    }
    return false;
}

void DrawerHardwareTab::updateBrandAndModelGraphics()
{
    int selId = hwModeCombo.getSelectedId();
    if (selId < 1 || selId > static_cast<int>(hardwareList.size())) return;

    const auto& item = hardwareList[static_cast<size_t>(selId - 1)];
    currentHwBrand = item.brand;

    brandLogoDrawable.reset();
    if (item.brandLogo.isNotEmpty())
    {
        auto brandFile = juce::File(item.brandLogo);
        if (!brandFile.existsAsFile())
            brandFile = locateAssetFile(item.brandLogo);

        if (brandFile.existsAsFile())
        {
            if (brandFile.getFileExtension().equalsIgnoreCase(".svg"))
            {
                juce::String svgText = brandFile.loadFileAsString();
                if (AppTheme::currentMode == AppTheme::ThemeMode::Dark)
                {
                    if (item.brand.containsIgnoreCase("roland"))
                    {
                        // Roland preserves its orange
                    }
                    else if (item.brand.containsIgnoreCase("yamaha"))
                    {
                        svgText = svgText.replace("fill:#48217a", "fill:#A855F7", true)
                                         .replace("fill: #48217a", "fill:#A855F7", true)
                                         .replace("fill=\"#48217a\"", "fill=\"#A855F7\"", true);
                    }
                    else
                    {
                        svgText = svgText.replace("fill=\"#333\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"#333333\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"#000000\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"#000\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"black\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"#111111\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"#111827\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"#222222\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill=\"#231f20\"", "fill=\"#FFFFFF\"", true)
                                         .replace("fill:#231f20", "fill:#FFFFFF", true)
                                         .replace("fill: #231f20", "fill:#FFFFFF", true)
                                         .replace("fill: #000000", "fill: #FFFFFF", true)
                                         .replace("fill:#000000", "fill:#FFFFFF", true)
                                         .replace("fill: black", "fill: #FFFFFF", true)
                                         .replace("fill:black", "fill:#FFFFFF", true)
                                         .replace("fill:#003296", "fill:#FFFFFF", true)
                                         .replace("fill: #003296", "fill:#FFFFFF", true)
                                         .replace("stroke=\"#000000\"", "stroke=\"#FFFFFF\"", true)
                                         .replace("stroke=\"#000\"", "stroke=\"#FFFFFF\"", true)
                                         .replace("stroke=\"black\"", "stroke=\"#FFFFFF\"", true);

                        if (!svgText.containsIgnoreCase("fill="))
                            svgText = svgText.replace("<svg ", "<svg fill=\"#FFFFFF\" ", true);
                    }
                }

                auto xml = juce::parseXML(svgText);
                if (xml != nullptr)
                    brandLogoDrawable = juce::Drawable::createFromSVG(*xml);
            }
            else
            {
                brandLogoDrawable = juce::Drawable::createFromImageDataStream(*brandFile.createInputStream());
            }
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

void DrawerHardwareTab::updateFunctionSelectionUI(const HardwareItem& item)
{
    hwFunctionCombo.clear(juce::dontSendNotification);
    for (size_t i = 0; i < item.functions.size(); ++i)
    {
        hwFunctionCombo.addItem(item.functions[i].name, static_cast<int>(i + 1));
    }
    if (!item.functions.empty())
    {
        hwFunctionCombo.setSelectedId(1, juce::sendNotification);
        cardWiring.stimulusText = item.functions[0].stimulusOutput;
        cardWiring.responseText = item.functions[0].responseInput;
        cardWiring.repaint();
    }
}

void DrawerHardwareTab::updateTheme()
{
    if (pickerWindow != nullptr)
    {
        if (auto* hpw = dynamic_cast<HardwarePickerWindow*>(pickerWindow.get()))
            hpw->updateTheme();
    }

    btnAutoDetect.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnAutoDetect.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);

    lblHardwareLockedBanner.setColour(juce::Label::backgroundColourId, SoundIdTheme::surfaceSubtle);
    lblHardwareLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);

    updateBrandAndModelGraphics();
    repaint();
}

int DrawerHardwareTab::getPreferredHeight() const noexcept
{
    return isHardwareLocked ? 540 : 490;
}

void DrawerHardwareTab::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void DrawerHardwareTab::resized()
{
    int w = getWidth();
    int y = 0;

    imgDisplay.setBounds(0, y, w, 160);
    y += 168;

    if (isHardwareLocked)
    {
        lblHardwareLockedBanner.setBounds(0, y, w, 28);
        y += 32;
        btnChangeHwOrNewFlow.setBounds(0, y, w, 32);
        y += 38;
    }

    lblHwTitle.setBounds(0, y, w, 20);
    y += 24;

    if (!isHardwareLocked)
    {
        lblAutoDetectSection.setBounds(0, y, w, 16);
        y += 18;

        btnAutoDetect.setBounds(0, y, w, 30);
        y += 34;

        lblOrSeparator.setBounds(0, y, w, 16);
        y += 22;
    }

    lblSelectHw.setBounds(0, y, w, 16);
    y += 18;
    hwModeCombo.setBounds(0, y, w, 28);
    y += 34;

    lblSelectFunc.setBounds(0, y, w, 16);
    y += 18;
    hwFunctionCombo.setBounds(0, y, w, 28);
    y += 34;

    cardWiring.setBounds(0, y, w, 72);
}

void DrawerHardwareTab::ImageDisplayComponent::paint(juce::Graphics& g)
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

void DrawerHardwareTab::WiringGuideCard::paint(juce::Graphics& g)
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

} // namespace abdaudiolab::gui
