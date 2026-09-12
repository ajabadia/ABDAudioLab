#include "HardwareRoutingPanel.h"
#include "SoundIdTheme.h"
#include "AppTheme.h"

namespace abdaudiolab::gui
{

HardwareRoutingPanel::HardwareRoutingPanel()
{
    // Auto-detect button
    btnAutoDetect.setTooltip("Detectar automaticamente el sintetizador conectado via MIDI SysEx Identity Inquiry / USB");
    btnAutoDetect.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnAutoDetect.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAutoDetect.onClick = [this] {
        if (onAutoDetectRequested)
            onAutoDetectRequested();
    };
    addAndMakeVisible(btnAutoDetect);

    // Hardware locked banner and change button (for loaded sessions)
    lblHardwareLockedBanner.setText("Hardware profile locked for the active session.\nTo measure different hardware or submodule, click Unlock on the right.", juce::dontSendNotification);
    lblHardwareLockedBanner.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::italic));
    lblHardwareLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblHardwareLockedBanner.setColour(juce::Label::backgroundColourId, SoundIdTheme::bgCardHover);
    lblHardwareLockedBanner.setJustificationType(juce::Justification::centredLeft);
    lblHardwareLockedBanner.setVisible(false);
    addChildComponent(lblHardwareLockedBanner);

    btnChangeHwOrNewFlow.setButtonText("Change Hardware / New Flow");
    btnChangeHwOrNewFlow.setTooltip("Desbloquear selector para elegir otro sintetizador o submodulo e iniciar un nuevo flujo");
    btnChangeHwOrNewFlow.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnChangeHwOrNewFlow.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnChangeHwOrNewFlow.onClick = [this] {
        if (onNewFlowRequested != nullptr)
            onNewFlowRequested();
        else
            setHardwareLocked(false);
    };
    btnChangeHwOrNewFlow.setVisible(false);
    addChildComponent(btnChangeHwOrNewFlow);

    // Hardware device selector
    hwDeviceCombo.setTextWhenNothingSelected("Select Hardware Device...");
    hwDeviceCombo.setTooltip("Choose analog synthesizer, guitar pedal, or rack unit to profile");
    hwDeviceCombo.onChange = [this] {
        updateFunctionsCombo();
        updateRoutingDisplay();
        updateBrandAndModelGraphics();
        if (onHardwareSelected)
            onHardwareSelected(getSelectedHardwareId(), getSelectedFunctionId());
    };
    addAndMakeVisible(hwDeviceCombo);

    // Hardware function / block selector
    hwFunctionCombo.setTextWhenNothingSelected("Select Function / Block...");
    hwFunctionCombo.setTooltip("Select hardware submodule/block to characterize (e.g., VCF Cutoff, VCA, Overdrive)");
    hwFunctionCombo.onChange = [this] {
        updateRoutingDisplay();
        if (onHardwareSelected)
            onHardwareSelected(getSelectedHardwareId(), getSelectedFunctionId());
    };
    addAndMakeVisible(hwFunctionCombo);

    // Subcomponentes visuales autónomos
    addAndMakeVisible(wiringDiagram);
    addAndMakeVisible(deviceDisplayCard);

    // Navigation and advanced buttons
    btnContinue.setButtonText(juce::String::fromUTF8("Proceed to Run Session (Step 3) \xE2\x86\x92"));
    btnContinue.setTooltip("Confirm configuration and advance to Step 3: Run Session");
    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnContinue.onClick = [this] {
        if (onContinueToCalibration)
            onContinueToCalibration();
    };
    addAndMakeVisible(btnContinue);

    btnAdvanced.setButtonText("Advanced Settings...");
    btnAdvanced.setTooltip("Open side drawer for advanced port and parameter configuration");
    btnAdvanced.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnAdvanced.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnAdvanced.onClick = [this] {
        if (onOpenAdvancedSettings)
            onOpenAdvancedSettings();
    };
    addAndMakeVisible(btnAdvanced);

    btnOpenTopology.setTooltip("Open interactive studio wiring and device topology map");
    btnOpenTopology.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnOpenTopology.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnOpenTopology.onClick = [this] {
        if (onOpenTopologyModal)
            onOpenTopologyModal();
    };
    addAndMakeVisible(btnOpenTopology);

    hotplugMonitor.onDevicePlugged = [this](const hardware::DiscoveredDevice& dev) {
        juce::MessageManager::callAsync([this, dev] {
            setAutoDetectButtonText(juce::String::fromUTF8(u8"✓ ") + juce::String(dev.displayName));
            for (size_t i = 0; i < contractsList.size(); ++i)
            {
                if (contractsList[i].id == dev.hardwareId)
                {
                    setSelectedHardware(juce::String(dev.hardwareId));
                    if (onHardwareSelected)
                        onHardwareSelected(getSelectedHardwareId(), getSelectedFunctionId());
                    break;
                }
            }
        });
    };

    hotplugMonitor.onDeviceUnplugged = [this](const juce::String& portName) {
        juce::MessageManager::callAsync([this, portName] {
            setAutoDetectButtonText("Disconnected: " + portName);
        });
    };
}

void HardwareRoutingPanel::setHardwareLocked(bool locked)
{
    isHardwareLocked = locked;
    hwDeviceCombo.setEnabled(!locked);
    hwFunctionCombo.setEnabled(!locked);
    btnAutoDetect.setVisible(!locked);
    lblHardwareLockedBanner.setVisible(locked);
    btnChangeHwOrNewFlow.setVisible(locked);
    btnContinue.setButtonText(juce::String::fromUTF8("Proceed to Run Session (Step 3) \xE2\x86\x92"));

    if (locked)
    {
        hotplugMonitor.stopMonitoring();
    }
    else if (isVisible())
    {
        hotplugMonitor.startMonitoring(1500);
    }

    resized();
    repaint();
}

void HardwareRoutingPanel::visibilityChanged()
{
    if (isVisible() && !isHardwareLocked)
    {
        hotplugMonitor.startMonitoring(1500);
    }
    else
    {
        hotplugMonitor.stopMonitoring();
    }
}

void HardwareRoutingPanel::setAutoDetectButtonText(const juce::String& text)
{
    btnAutoDetect.setButtonText(text);
}

void HardwareRoutingPanel::setContracts(const std::vector<core::HardwareContract>& contracts)
{
    contractsList = contracts;
    hotplugMonitor.setContracts(contractsList);
    hwDeviceCombo.clear(juce::dontSendNotification);

    for (size_t i = 0; i < contractsList.size(); ++i)
    {
        const auto& c = contractsList[i];
        juce::String label = juce::String(c.displayName) + " (" + juce::String(c.deviceType) + ")";
        hwDeviceCombo.addItem(label, static_cast<int>(i + 1));
    }

    hwDeviceCombo.setSelectedId(0, juce::dontSendNotification);
    hwFunctionCombo.clear(juce::dontSendNotification);
    deviceDisplayCard.clear();
    wiringDiagram.clear();
}

void HardwareRoutingPanel::setSelectedHardware(const juce::String& hwId, const juce::String& funcId)
{
    for (size_t i = 0; i < contractsList.size(); ++i)
    {
        if (contractsList[i].id == hwId.toStdString())
        {
            hwDeviceCombo.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
            updateFunctionsCombo();

            if (funcId.isNotEmpty())
            {
                const auto& fns = contractsList[i].functions;
                for (size_t f = 0; f < fns.size(); ++f)
                {
                    if (fns[f].id == funcId.toStdString())
                    {
                        hwFunctionCombo.setSelectedId(static_cast<int>(f + 1), juce::dontSendNotification);
                        break;
                    }
                }
            }
            updateRoutingDisplay();
            updateBrandAndModelGraphics();
            repaint();
            return;
        }
    }
}

juce::String HardwareRoutingPanel::getSelectedHardwareId() const
{
    int sel = hwDeviceCombo.getSelectedId() - 1;
    if (sel >= 0 && sel < static_cast<int>(contractsList.size()))
        return juce::String(contractsList[static_cast<size_t>(sel)].id);
    return {};
}

juce::String HardwareRoutingPanel::getSelectedFunctionId() const
{
    int selHw = hwDeviceCombo.getSelectedId() - 1;
    if (selHw >= 0 && selHw < static_cast<int>(contractsList.size()))
    {
        const auto& fns = contractsList[static_cast<size_t>(selHw)].functions;
        int selFn = hwFunctionCombo.getSelectedId() - 1;
        if (selFn >= 0 && selFn < static_cast<int>(fns.size()))
            return juce::String(fns[static_cast<size_t>(selFn)].id);
    }
    return {};
}

void HardwareRoutingPanel::updateFunctionsCombo()
{
    hwFunctionCombo.clear(juce::dontSendNotification);
    int sel = hwDeviceCombo.getSelectedId() - 1;
    if (sel >= 0 && sel < static_cast<int>(contractsList.size()))
    {
        const auto& c = contractsList[static_cast<size_t>(sel)];
        for (size_t f = 0; f < c.functions.size(); ++f)
        {
            const auto& fn = c.functions[f];
            juce::String label = juce::String(fn.name) + " [" + juce::String(fn.blockType) + "]";
            hwFunctionCombo.addItem(label, static_cast<int>(f + 1));
        }

        if (!c.functions.empty())
            hwFunctionCombo.setSelectedId(1, juce::dontSendNotification);
    }
}

void HardwareRoutingPanel::updateBrandAndModelGraphics()
{
    int sel = hwDeviceCombo.getSelectedId() - 1;
    if (sel >= 0 && sel < static_cast<int>(contractsList.size()))
    {
        deviceDisplayCard.setDevice(&contractsList[static_cast<size_t>(sel)]);
    }
    else
    {
        deviceDisplayCard.setDevice(nullptr);
    }
}

void HardwareRoutingPanel::updateRoutingDisplay()
{
    bool isMidiAutonomous = false;
    juce::String routingStimulusText = "Audio Out 1 (DAC) -> Hardware Audio In";
    juce::String routingResponseText = "Hardware Audio Out -> Audio In 1 (ADC)";
    juce::String routingNotesText = "Connect analog audio patch cables and MIDI interface before proceeding.";

    int selHw = hwDeviceCombo.getSelectedId() - 1;
    if (selHw >= 0 && selHw < static_cast<int>(contractsList.size()))
    {
        const auto& c = contractsList[static_cast<size_t>(selHw)];
        juce::String name = juce::String(c.displayName);
        bool isSynthHardware = (c.deviceType == "AUTOMATED_MIDI_CC" || c.deviceType == "AUTOMATED_SYSEX") &&
                               (name.containsIgnoreCase("Juno") || name.containsIgnoreCase("DeepMind")
                                || name.containsIgnoreCase("Prophecy") || name.containsIgnoreCase("MS2000")
                                || name.containsIgnoreCase("CZ-101") || name.containsIgnoreCase("PRO-800")
                                || name.containsIgnoreCase("Bass Station") || name.containsIgnoreCase("DX7"));

        int selFn = hwFunctionCombo.getSelectedId() - 1;
        if (selFn >= 0 && selFn < static_cast<int>(c.functions.size()))
        {
            const auto& fn = c.functions[static_cast<size_t>(selFn)];
            if (fn.suggestedStimulus == "SILENT_CAPTURE" || fn.suggestedStimulus == "GATE_PULSE"
                || juce::String(fn.routingGuide.stimulusOutput).containsIgnoreCase("MIDI")
                || juce::String(fn.routingGuide.stimulusOutput).containsIgnoreCase("NONE")
                || isSynthHardware)
            {
                isMidiAutonomous = true;
            }

            if (!fn.routingGuide.stimulusOutput.empty())
                routingStimulusText = juce::String(fn.routingGuide.stimulusOutput);
            else if (isMidiAutonomous)
                routingStimulusText = "MIDI / USB Input (Synthesizer)";

            if (!fn.routingGuide.responseInput.empty())
                routingResponseText = juce::String(fn.routingGuide.responseInput);
            else if (isMidiAutonomous)
                routingResponseText = "Synthesizer Audio Output";

            if (!fn.routingGuide.notes.empty())
                routingNotesText = juce::String(fn.routingGuide.notes);
            else if (isMidiAutonomous)
                routingNotesText = "Synthesizer generates audio internally via MIDI notes. DAC output cable is not required.";
        }
        else if (isSynthHardware)
        {
            isMidiAutonomous = true;
            routingStimulusText = "MIDI / USB Input (Synthesizer)";
            routingResponseText = "Synthesizer Audio Output";
            routingNotesText = "Synthesizer generates audio internally via MIDI notes. DAC output cable is not required.";
        }
    }

    wiringDiagram.setRouting(routingStimulusText, routingResponseText, routingNotesText, isMidiAutonomous);
}

void HardwareRoutingPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Fondo del área central
    g.fillAll(SoundIdTheme::bgLight);

    // Tarjeta central principal
    float maxCardW = juce::jmin(920.0f, area.getWidth() - 32.0f);
    float maxCardH = juce::jmin(580.0f, area.getHeight() - 24.0f);
    auto cardBounds = juce::Rectangle<float>((area.getWidth() - maxCardW) * 0.5f,
                                            (area.getHeight() - maxCardH) * 0.5f,
                                            maxCardW, maxCardH);

    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(cardBounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(cardBounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = cardBounds.reduced(28.0f, 22.0f);

    // 1. Header Row
    auto headerRow = content.removeFromTop(30.0f);
    g.setFont(juce::FontOptions("Inter", 18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Step 2: Hardware Selection & Physical Routing",
               headerRow.removeFromLeft(500.0f), juce::Justification::centredLeft, true);

    auto badgeRect = headerRow.removeFromRight(130.0f).reduced(0.0f, 3.0f);
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(badgeRect, 6.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText("PASO 2 / 4", badgeRect, juce::Justification::centred, true);

    content.removeFromTop(10.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(content.removeFromTop(1.0f));
    content.removeFromTop(12.0f);

    // Fila superior: botón de autodetección o banner de bloqueo
    content.removeFromTop(36.0f);
    content.removeFromTop(12.0f);

    // 2 Columnas de información: Izquierda (Selectores) y Derecha (Tarjeta de Hardware)
    float colGap = 20.0f;
    float leftColW = (content.getWidth() - colGap) * 0.52f;
    auto leftCol = content.removeFromLeft(leftColW);

    // Etiquetas de los desplegables en columna izquierda
    auto lblRow = leftCol.removeFromTop(18.0f);
    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("DISPOSITIVO DE HARDWARE", lblRow, juce::Justification::centredLeft, true);
    leftCol.removeFromTop(36.0f); // hwDeviceCombo
    leftCol.removeFromTop(10.0f);

    auto lblRow2 = leftCol.removeFromTop(18.0f);
    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("ANALOG FUNCTION / TARGET SUBMODULE", lblRow2, juce::Justification::centredLeft, true);
}

void HardwareRoutingPanel::resized()
{
    auto area = getLocalBounds();
    int maxCardW = juce::jmin(920, area.getWidth() - 32);
    int maxCardH = juce::jmin(580, area.getHeight() - 24);
    auto cardBounds = juce::Rectangle<int>((area.getWidth() - maxCardW) / 2,
                                          (area.getHeight() - maxCardH) / 2,
                                          maxCardW, maxCardH);

    int contentX = cardBounds.getX() + 28;
    int contentW = cardBounds.getWidth() - 56;

    // 1. Parte superior: Auto-detección o Banner de bloqueo con botón de nuevo flujo
    int topActionY = cardBounds.getY() + 72;
    if (isHardwareLocked)
    {
        int changeBtnW = 260;
        int bannerW = contentW - changeBtnW - 14;
        lblHardwareLockedBanner.setBounds(contentX, topActionY, bannerW, 36);
        btnChangeHwOrNewFlow.setBounds(contentX + bannerW + 14, topActionY, changeBtnW, 36);
        btnAutoDetect.setBounds(0, 0, 0, 0);
    }
    else
    {
        btnAutoDetect.setBounds(contentX, topActionY, contentW, 34);
        lblHardwareLockedBanner.setBounds(0, 0, 0, 0);
        btnChangeHwOrNewFlow.setBounds(0, 0, 0, 0);
    }

    // 2. Columna izquierda: Desplegables y Diagrama de Cableado
    int colGap = 20;
    int leftColW = static_cast<int>((contentW - colGap) * 0.52f);
    int rightColX = contentX + leftColW + colGap;
    int rightColW = contentW - leftColW - colGap;

    int combo1Y = topActionY + 36 + 28; // Después de etiqueta "DISPOSITIVO DE HARDWARE"
    hwDeviceCombo.setBounds(contentX, combo1Y, leftColW, 34);

    int combo2Y = combo1Y + 34 + 28;     // Después de etiqueta "FUNCIÓN ANALÓGICA"
    hwFunctionCombo.setBounds(contentX, combo2Y, leftColW, 34);

    int diagramY = combo2Y + 34 + 14;
    wiringDiagram.setBounds(contentX, diagramY, leftColW, 180);

    // 3. Columna derecha: Tarjeta de Hardware (Logo e Imagen)
    deviceDisplayCard.setBounds(rightColX, combo1Y - 18, rightColW, 310);

    // 4. Botones de acción inferiores
    int bottomY = cardBounds.getBottom() - 52;
    btnAdvanced.setBounds(contentX, bottomY, 150, 36);
    btnOpenTopology.setBounds(contentX + 150 + 10, bottomY, 160, 36);
    btnContinue.setBounds(cardBounds.getRight() - 28 - 280, bottomY, 280, 36);
}

void HardwareRoutingPanel::resetSelection()
{
    isHardwareLocked = false;
    hwDeviceCombo.clear(juce::dontSendNotification);
    hwFunctionCombo.clear(juce::dontSendNotification);
    deviceDisplayCard.clear();
    wiringDiagram.clear();
    btnAutoDetect.setButtonText("Auto-Detect Hardware");
    btnContinue.setEnabled(true);

    for (size_t i = 0; i < contractsList.size(); ++i)
        hwDeviceCombo.addItem(contractsList[i].displayName, static_cast<int>(i + 1));

    repaint();
}

void HardwareRoutingPanel::setPluginVirtualRouting(const juce::String& pluginName, const juce::String& format, bool isInstrument)
{
    isHardwareLocked = false;
    deviceDisplayCard.setPluginInfo(pluginName, "", format, isInstrument);
    if (isInstrument)
    {
        wiringDiagram.setPluginRouting(
            "Internal MIDI Sequencer",
            "Plugin (MIDI In)",
            "Plugin (Audio Out)",
            "ABDAudioLab Capture",
            "Direct digital loop. Zero converter latency.");
    }
    else
    {
        wiringDiagram.setPluginRouting(
            "Stimulus Generator",
            "Plugin (Audio In)",
            "Plugin (Audio Out)",
            "ABDAudioLab Capture",
            "Direct digital closed loop. Zero converter coloration.");
    }
    repaint();
}

} // namespace abdaudiolab::gui
