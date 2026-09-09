#include "SlideInDrawer.h"
#include <cmath>

namespace abdaudiolab::gui
{

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
    // Attach Modular Tabs
    // ==========================================
    contentComp.addChildComponent(tabFileSession);
    tabFileSession.onNewSessionClicked = [this] {
        closeDrawer();
        if (onNewSessionClicked) onNewSessionClicked();
    };
    tabFileSession.onOpenSessionClicked = [this] {
        closeDrawer();
        if (onOpenSessionClicked) onOpenSessionClicked();
    };
    tabFileSession.onSaveSessionClicked = [this] {
        closeDrawer();
        if (onSaveSessionClicked) onSaveSessionClicked();
    };
    tabFileSession.onSaveSessionAsClicked = [this] {
        closeDrawer();
        if (onSaveSessionAsClicked) onSaveSessionAsClicked();
    };
    tabFileSession.onReanalyzeSessionClicked = [this] {
        closeDrawer();
        if (onReanalyzeSessionClicked) onReanalyzeSessionClicked();
    };
    tabFileSession.onChangeExportFolderClicked = [this] {
        if (onChangeExportFolderClicked) onChangeExportFolderClicked();
    };
    tabFileSession.onRevealExportFolderClicked = [this] {
        if (onRevealExportFolderClicked) onRevealExportFolderClicked();
    };
    tabFileSession.onExportReportClicked = [this] {
        if (onExportReportClicked) onExportReportClicked();
    };
    tabFileSession.onCheckUpdatesClicked = [this] {
        if (onCheckUpdatesClicked) onCheckUpdatesClicked();
    };
    tabFileSession.onExitAppClicked = [this] {
        if (onExitAppClicked) onExitAppClicked();
    };

    contentComp.addChildComponent(tabHardware);
    tabHardware.onHardwareSelected = [this](const juce::String& hwId, const juce::String& funcId) {
        if (onHardwareSelected) onHardwareSelected(hwId, funcId);
    };
    tabHardware.onDeviceDetected = [this](const juce::String& displayName) {
        if (onDeviceDetected) onDeviceDetected(displayName);
    };
    tabHardware.onNewFlowRequested = [this] {
        if (onNewFlowRequested) onNewFlowRequested();
    };

    contentComp.addChildComponent(testEditorPanel);
    testEditorPanel.onConfigChanged = [this] {
        testEditorConfig = testEditorPanel.getConfiguration();
    };

    contentComp.addChildComponent(tabSetup);
    tabSetup.onOpenAudioSettingsClicked = [this] {
        if (onOpenAudioSettingsClicked) onOpenAudioSettingsClicked();
    };
    tabSetup.onAboutClicked = [this] {
        if (onAboutClicked) onAboutClicked();
    };

    // ==========================================
    // Bottom Action Bar
    // ==========================================
    panel.addAndMakeVisible(bottomBar);

    btnCancel.setTooltip("Cancel - Discard changes and close drawer");
    btnCancel.onClick = [this] { closeDrawer(); };
    bottomBar.addAndMakeVisible(btnCancel);

    btnConfirm.setTooltip("Accept - Apply selected hardware and routing");
    btnConfirm.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillBlackBg);
    btnConfirm.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnConfirm.onClick = [this] {
        if (currentViewMode == DrawerViewMode::HardwareAndRouting)
        {
            if (!tabHardware.getHardwareLocked())
            {
                tabHardware.setHardwareLocked(true);
                if (onHardwareSelected)
                    onHardwareSelected(tabHardware.getSelectedHardwareId(), tabHardware.getSelectedFunctionId());
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

void SlideInDrawer::updateTheme()
{
    tabFileSession.updateTheme();
    tabHardware.updateTheme();
    testEditorPanel.updateTheme();
    tabSetup.updateTheme();

    btnCancel.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnCancel.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);

    if (AppTheme::currentMode == AppTheme::ThemeMode::Dark)
    {
        btnConfirm.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
        btnConfirm.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    }
    else
    {
        btnConfirm.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillBlackBg);
        btnConfirm.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    contentComp.repaint();
    panel.repaint();
    repaint();
}

void SlideInDrawer::switchViewMode(DrawerViewMode mode)
{
    currentViewMode = mode;

    tabFileSession.setVisible(mode == DrawerViewMode::FileSessionAndStorage);
    tabHardware.setVisible(mode == DrawerViewMode::HardwareAndRouting);
    testEditorPanel.setVisible(mode == DrawerViewMode::TestAndParametersEditor);
    tabSetup.setVisible(mode == DrawerViewMode::EngineCalibrationAndInfo);

    if (mode == DrawerViewMode::HardwareAndRouting)
    {
        btnConfirm.setButtonText(tabHardware.getHardwareLocked() ? "Close" : "Accept Hardware Selection");
        btnCancel.setVisible(!tabHardware.getHardwareLocked());
    }
    else if (mode == DrawerViewMode::TestAndParametersEditor)
    {
        btnConfirm.setButtonText(currentEditingTestIndex >= 0 ? "Save Changes" : "Add to Session Plan");
        btnCancel.setVisible(true);
    }
    else
    {
        btnConfirm.setButtonText("Close");
        btnCancel.setVisible(true);
    }

    layoutDrawerContent();
    repaint();
}

void SlideInDrawer::openFileDrawer(const juce::String& currentExportPath)
{
    tabFileSession.setExportDirectory(currentExportPath);
    tabFileSession.refreshFilePreviewList();
    switchViewMode(DrawerViewMode::FileSessionAndStorage);
    openDrawer();
}

void SlideInDrawer::openHardwareDrawer()
{
    tabHardware.updateBrandAndModelGraphics();
    switchViewMode(DrawerViewMode::HardwareAndRouting);
    btnCancel.setVisible(!tabHardware.getHardwareLocked());
    btnConfirm.setVisible(true);
    btnConfirm.setButtonText(tabHardware.getHardwareLocked() ? "Close" : "Accept Hardware Selection");
    openDrawer();
}

void SlideInDrawer::openTestEditorDrawer(const TestConfiguration& initialConfig, int editingIndex)
{
    testEditorConfig = initialConfig;
    currentEditingTestIndex = editingIndex;
    testEditorPanel.populateWithAutoTestPresets();
    testEditorPanel.setConfiguration(testEditorConfig);
    switchViewMode(DrawerViewMode::TestAndParametersEditor);
    openDrawer();
}

void SlideInDrawer::openSetupDrawer(const TelemetryInfo& info)
{
    setTelemetryInfo(info);
    tabSetup.setTargetHardwareInfo(
        tabHardware.getActiveHardwareDisplayName(),
        tabHardware.getActiveFunctionDisplayName(),
        "Routing: Self-Contained / Direct Loopback",
        tabHardware.getActiveModelRasterImage(),
        tabHardware.getModelSvgDrawable()
    );
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

void SlideInDrawer::setTelemetryInfo(const TelemetryInfo& info)
{
    telemetryInfo = info;
    tabSetup.setTelemetryInfo(info);
}

void SlideInDrawer::setHardwareList(const std::vector<HardwareItem>& list)
{
    tabHardware.setHardwareList(list);
}

void SlideInDrawer::setContracts(std::vector<core::HardwareContract> contractsList)
{
    tabHardware.setContracts(std::move(contractsList));
}

void SlideInDrawer::setSelectedHardwareId(const juce::String& id)
{
    tabHardware.setSelectedHardwareId(id);
}

void SlideInDrawer::clearSelectedHardware()
{
    tabHardware.clearSelectedHardware();
}

void SlideInDrawer::setHardwareLocked(bool locked)
{
    tabHardware.setHardwareLocked(locked);
    if (currentViewMode == DrawerViewMode::HardwareAndRouting)
    {
        btnConfirm.setButtonText(locked ? "Close" : "Accept Hardware Selection");
        btnCancel.setVisible(!locked);
    }
    layoutDrawerContent();
    repaint();
}

void SlideInDrawer::triggerAutoDetect()
{
    tabHardware.triggerAutoDetect();
}

bool SlideInDrawer::getHardwareLocked() const
{
    return tabHardware.getHardwareLocked();
}

juce::String SlideInDrawer::getSelectedHardwareId() const
{
    return tabHardware.getSelectedHardwareId();
}

juce::String SlideInDrawer::getSelectedFunctionId() const
{
    return tabHardware.getSelectedFunctionId();
}

juce::String SlideInDrawer::getActiveHardwareDisplayName() const
{
    return tabHardware.getActiveHardwareDisplayName();
}

juce::String SlideInDrawer::getActiveFunctionDisplayName() const
{
    return tabHardware.getActiveFunctionDisplayName();
}

const juce::Image& SlideInDrawer::getActiveModelRasterImage() const noexcept
{
    return tabHardware.getActiveModelRasterImage();
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

float SlideInDrawer::getBurstDurationSeconds() const
{
    return testEditorConfig.burstDurationSec;
}

bool SlideInDrawer::isAdaptiveEnvelopeMode() const
{
    return testEditorConfig.captureMode == "ADAPTIVE_ENVELOPE";
}

int SlideInDrawer::getSelectedHardwareModeIndex() const
{
    return tabHardware.getSelectedHardwareModeIndex();
}

audio::StimulusType SlideInDrawer::getSelectedStimulusType() const
{
    return testEditorConfig.stimulusType;
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
    int contentH = 0;

    if (currentViewMode == DrawerViewMode::FileSessionAndStorage)
    {
        contentH = tabFileSession.getPreferredHeight();
        tabFileSession.setBounds(padX, 8, contentW, contentH);
    }
    else if (currentViewMode == DrawerViewMode::HardwareAndRouting)
    {
        contentH = tabHardware.getPreferredHeight();
        tabHardware.setBounds(padX, 8, contentW, contentH);
    }
    else if (currentViewMode == DrawerViewMode::TestAndParametersEditor)
    {
        contentH = testEditorPanel.getPreferredHeight();
        testEditorPanel.setBounds(padX, 8, contentW, contentH);
    }
    else if (currentViewMode == DrawerViewMode::EngineCalibrationAndInfo)
    {
        contentH = tabSetup.getPreferredHeight();
        tabSetup.setBounds(padX, 8, contentW, contentH);
    }

    contentComp.setBounds(0, 0, static_cast<int>(panelWidth) - 10, contentH + 20);
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

    // 2. Solid OPAQUE Panel Background
    g.setColour(SoundIdTheme::bgCard);
    g.fillRect(drawerX, 0.0f, panelWidth, static_cast<float>(getHeight()));

    // 3. Bottom Action Bar Solid Opaque Background
    float bottomH = 58.0f;
    g.setColour(SoundIdTheme::bgCardHover);
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
        auto topLogoArea = headerRect.removeFromLeft(240.0f);

        if (currentViewMode == DrawerViewMode::FileSessionAndStorage)
        {
            g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText("FILE & SESSION STORAGE", topLogoArea, juce::Justification::centredLeft, true);
        }
        else if (currentViewMode == DrawerViewMode::HardwareAndRouting)
        {
            if (tabHardware.getBrandLogoDrawable() != nullptr)
            {
                tabHardware.getBrandLogoDrawable()->drawWithin(g, topLogoArea, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
            }
            else if (tabHardware.getCurrentHwBrand().isNotEmpty())
            {
                g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
                g.setColour(SoundIdTheme::textSecondary);
                g.drawText(tabHardware.getCurrentHwBrand().toUpperCase(), topLogoArea, juce::Justification::centredLeft, true);
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
