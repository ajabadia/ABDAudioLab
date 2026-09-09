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
    lblHardwareLockedBanner.setText(juce::String::fromUTF8(u8"Perfil de hardware bloqueado para la sesión activa.\nPara medir otro hardware o submódulo, haz clic a la derecha."), juce::dontSendNotification);
    lblHardwareLockedBanner.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::italic));
    lblHardwareLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblHardwareLockedBanner.setColour(juce::Label::backgroundColourId, SoundIdTheme::bgCardHover);
    lblHardwareLockedBanner.setJustificationType(juce::Justification::centredLeft);
    lblHardwareLockedBanner.setVisible(false);
    addChildComponent(lblHardwareLockedBanner);

    btnChangeHwOrNewFlow.setButtonText(juce::String::fromUTF8(u8"Cambiar Hardware / Nuevo Flujo"));
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
    hwDeviceCombo.setTextWhenNothingSelected(juce::String::fromUTF8(u8"Seleccionar Dispositivo de Hardware..."));
    hwDeviceCombo.setTooltip(juce::String::fromUTF8(u8"Elige el sintetizador analógico, pedal o módulo a perfilar"));
    hwDeviceCombo.onChange = [this] {
        updateFunctionsCombo();
        updateRoutingDisplay();
        updateBrandAndModelGraphics();
        if (onHardwareSelected)
            onHardwareSelected(getSelectedHardwareId(), getSelectedFunctionId());
    };
    addAndMakeVisible(hwDeviceCombo);

    // Hardware function / block selector
    hwFunctionCombo.setTextWhenNothingSelected(juce::String::fromUTF8(u8"Seleccionar Función / Bloque..."));
    hwFunctionCombo.setTooltip(juce::String::fromUTF8(u8"Selecciona el bloque del hardware a caracterizar (ej: VCF Cutoff, VCA)"));
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
    btnContinue.setButtonText(juce::String::fromUTF8(u8"Continuar a Calibración (Paso 2) ➔"));
    btnContinue.setTooltip(juce::String::fromUTF8(u8"Confirmar selección y avanzar al Paso 2: Calibración de Lazo Cerrado"));
    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnContinue.onClick = [this] {
        if (onContinueToCalibration)
            onContinueToCalibration();
    };
    addAndMakeVisible(btnContinue);

    btnAdvanced.setButtonText(juce::String::fromUTF8(u8"Ajustes Avanzados..."));
    btnAdvanced.setTooltip(juce::String::fromUTF8(u8"Abrir el panel lateral para configuración avanzada de puertos y parámetros"));
    btnAdvanced.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnAdvanced.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnAdvanced.onClick = [this] {
        if (onOpenAdvancedSettings)
            onOpenAdvancedSettings();
    };
    addAndMakeVisible(btnAdvanced);

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
            setAutoDetectButtonText(juce::String::fromUTF8(u8"Desconectado: ") + portName);
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
    btnContinue.setButtonText(locked ? juce::String::fromUTF8(u8"Ir a Ejecución de Sesión (Paso 3) ➔")
                                    : juce::String::fromUTF8(u8"Continuar a Calibración (Paso 2) ➔"));

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

    if (!contractsList.empty() && hwDeviceCombo.getSelectedId() == 0)
    {
        hwDeviceCombo.setSelectedId(1, juce::sendNotification);
    }
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
    juce::String routingStimulusText = juce::String::fromUTF8(u8"Salida Audio 1 (DAC) ➔ Entrada de Audio del Hardware");
    juce::String routingResponseText = juce::String::fromUTF8(u8"Salida de Audio del Hardware ➔ Entrada Audio 1 (ADC)");
    juce::String routingNotesText = juce::String::fromUTF8(u8"Conecta los cables de audio analógicos y el interfaz MIDI antes de continuar.");

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
                routingStimulusText = juce::String::fromUTF8(u8"Entrada MIDI / USB (Sintetizador)");

            if (!fn.routingGuide.responseInput.empty())
                routingResponseText = juce::String(fn.routingGuide.responseInput);
            else if (isMidiAutonomous)
                routingResponseText = juce::String::fromUTF8(u8"Salida de Audio del Sintetizador");

            if (!fn.routingGuide.notes.empty())
                routingNotesText = juce::String(fn.routingGuide.notes);
            else if (isMidiAutonomous)
                routingNotesText = juce::String::fromUTF8(u8"El sintetizador genera el audio internamente excitado por notas MIDI. No es necesario conectar la salida DAC de la tarjeta.");
        }
        else if (isSynthHardware)
        {
            isMidiAutonomous = true;
            routingStimulusText = juce::String::fromUTF8(u8"Entrada MIDI / USB (Sintetizador)");
            routingResponseText = juce::String::fromUTF8(u8"Salida de Audio del Sintetizador");
            routingNotesText = juce::String::fromUTF8(u8"El sintetizador genera el audio internamente excitado por notas MIDI. No es necesario conectar la salida DAC de la tarjeta.");
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
    g.drawText(juce::String::fromUTF8(u8"Paso 1: Selección de Hardware y Enrutamiento Físico"),
               headerRow.removeFromLeft(500.0f), juce::Justification::centredLeft, true);

    auto badgeRect = headerRow.removeFromRight(130.0f).reduced(0.0f, 3.0f);
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(badgeRect, 6.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText("PASO 1 / 4", badgeRect, juce::Justification::centred, true);

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
    g.drawText(juce::String::fromUTF8(u8"FUNCIÓN ANALÓGICA / BLOQUE A PERFILAR"), lblRow2, juce::Justification::centredLeft, true);
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
    btnAdvanced.setBounds(contentX, bottomY, 180, 36);
    btnContinue.setBounds(cardBounds.getRight() - 28 - 300, bottomY, 300, 36);
}

} // namespace abdaudiolab::gui
