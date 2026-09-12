import os

def replace_in_file(file_path, replacements):
    print(f"Processing: {file_path}")
    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    modified = False
    for old_text, new_text in replacements:
        if old_text in content:
            content = content.replace(old_text, new_text)
            modified = True
            print(f"  Replaced snippet: {old_text[:40]}...")
        else:
            print(f"  WARNING snippet not found: {old_text[:40]}...")

    if modified:
        with open(file_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
        print(f"Successfully updated {file_path}")
    else:
        print(f"No changes made to {file_path}")

# 1. DrawerSetupTab.h
replace_in_file("src/gui/drawers/DrawerSetupTab.h", [
    (
        '''    void setTargetHardwareInfo(const juce::String& hwName,
                               const juce::String& submoduleName,
                               const juce::String& routingText,
                               const juce::Image& rasterImg,
                               const juce::Drawable* svgDrawable = nullptr,
                               const juce::String& category = {});''',
        '''    void setTargetHardwareInfo(const juce::String& hwName,
                               const juce::String& submoduleName,
                               const juce::String& routingText,
                               const juce::Image& rasterImg,
                               const juce::Drawable* svgDrawable = nullptr,
                               const juce::String& category = {},
                               const juce::String& statusText = {},
                               std::optional<juce::Colour> statusColour = std::nullopt);'''
    ),
    (
        '''    // Target Hardware Data
    juce::String targetHwName { "Direct Loopback (No Target Selected)" };
    juce::String targetSubmoduleName { "DAC/ADC Calibration" };
    juce::String targetRoutingText { "Direct Loopback Cable" };
    juce::Image targetRasterImage;
    std::unique_ptr<juce::Drawable> targetSvgDrawable;''',
        '''    // Target Hardware Data
    juce::String targetHwName { "Direct Loopback (No Target Selected)" };
    juce::String targetSubmoduleName { "DAC/ADC Calibration" };
    juce::String targetRoutingText { "Direct Loopback Cable" };
    juce::Image targetRasterImage;
    std::unique_ptr<juce::Drawable> targetSvgDrawable;
    juce::String targetStatusBadgeText { "DIRECT LOOPBACK" };
    juce::Colour targetStatusBadgeColour { SoundIdTheme::accentAmber };'''
    )
])

# 2. DrawerSetupTab.cpp
replace_in_file("src/gui/drawers/DrawerSetupTab.cpp", [
    (
        '''    g.drawText(juce::String::fromUTF8(u8"DISPOSITIVO BAJO PRUEBA / TARGET"), headerArea, juce::Justification::centredLeft, true);

    // Category / Status Tag on the right of header
    auto tagArea = headerArea.removeFromRight(130.0f);
    g.setColour(SoundIdTheme::accentAmber.withAlpha(0.12f));
    g.fillRoundedRectangle(tagArea, 3.0f);
    g.setColour(SoundIdTheme::accentAmber);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8(u8"HARDWARE ACTIVO"), tagArea, juce::Justification::centred, true);''',
        '''    g.drawText("TARGET DEVICE UNDER TEST", headerArea, juce::Justification::centredLeft, true);

    // Category / Status Tag on the right of header (Dynamic: SELECTED PROFILE vs CONNECTED HARDWARE)
    auto tagArea = headerArea.removeFromRight(150.0f);
    g.setColour(owner.targetStatusBadgeColour.withAlpha(0.12f));
    g.fillRoundedRectangle(tagArea, 3.0f);
    g.setColour(owner.targetStatusBadgeColour);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(owner.targetStatusBadgeText, tagArea, juce::Justification::centred, true);'''
    ),
    (
        '''    g.drawText(juce::String::fromUTF8(u8"CONEXIONES REALES ACTIVAS"), headerArea, juce::Justification::centredLeft, true);''',
        '''    g.drawText("ACTIVE REAL CONNECTIONS", headerArea, juce::Justification::centredLeft, true);'''
    ),
    (
        '''    // 1. Audio Output (Probe / Estímulo)
    juce::String audioOutStr;
    if (owner.hasAudioOutConnected)
        audioOutStr = owner.telemetryInfo.audioDeviceName + juce::String::fromUTF8(u8" -> Salida Principal (DAC)");
    else
        audioOutStr = juce::String::fromUTF8(u8"Sin salida asignada");
    drawConnectionRow(juce::String::fromUTF8(u8"AUDIO OUT"), audioOutStr, SoundIdTheme::accentGreen, owner.hasAudioOutConnected);

    // 2. Audio Input (Return / Medición)
    juce::String audioInStr;
    if (owner.hasAudioInConnected)
        audioInStr = owner.telemetryInfo.audioDeviceName + juce::String::fromUTF8(u8" <- Entrada de Grabación (ADC)");
    else
        audioInStr = juce::String::fromUTF8(u8"Sin entrada asignada");
    drawConnectionRow(juce::String::fromUTF8(u8"AUDIO IN"), audioInStr, SoundIdTheme::accentAmber, owner.hasAudioInConnected);

    // 3. MIDI Out (Control / SysEx / Clock)
    juce::String midiOutStr;
    if (owner.hasMidiOutConnected)
        midiOutStr = owner.telemetryInfo.midiOutputName + juce::String::fromUTF8(u8" (Control / SysEx)");
    else
        midiOutStr = juce::String::fromUTF8(u8"Sin puerto MIDI Out asignado");
    drawConnectionRow(juce::String::fromUTF8(u8"MIDI OUT"), midiOutStr, SoundIdTheme::accentBlue, owner.hasMidiOutConnected);

    // 4. MIDI In (Telemetría / Feedback)
    juce::String midiInStr;
    if (owner.hasMidiInConnected)
        midiInStr = owner.telemetryInfo.midiInputName + juce::String::fromUTF8(u8" (Telemetría / Feedback)");
    else
        midiInStr = juce::String::fromUTF8(u8"Sin puerto MIDI In asignado");
    drawConnectionRow(juce::String::fromUTF8(u8"MIDI IN"), midiInStr, SoundIdTheme::accentCyan, owner.hasMidiInConnected);''',
        '''    // 1. Audio Output (Probe / Excitation)
    juce::String audioOutStr;
    if (owner.hasAudioOutConnected)
        audioOutStr = owner.telemetryInfo.audioDeviceName + " -> Main Output (DAC)";
    else
        audioOutStr = "No output assigned";
    drawConnectionRow("AUDIO OUT", audioOutStr, SoundIdTheme::accentGreen, owner.hasAudioOutConnected);

    // 2. Audio Input (Return / Measurement)
    juce::String audioInStr;
    if (owner.hasAudioInConnected)
        audioInStr = owner.telemetryInfo.audioDeviceName + " <- Recording Input (ADC)";
    else
        audioInStr = "No input assigned";
    drawConnectionRow("AUDIO IN", audioInStr, SoundIdTheme::accentAmber, owner.hasAudioInConnected);

    // 3. MIDI Out (Control / SysEx / Clock)
    juce::String midiOutStr;
    if (owner.hasMidiOutConnected)
        midiOutStr = owner.telemetryInfo.midiOutputName + " (Control / SysEx)";
    else
        midiOutStr = "No MIDI Out port assigned";
    drawConnectionRow("MIDI OUT", midiOutStr, SoundIdTheme::accentBlue, owner.hasMidiOutConnected);

    // 4. MIDI In (Telemetry / Feedback)
    juce::String midiInStr;
    if (owner.hasMidiInConnected)
        midiInStr = owner.telemetryInfo.midiInputName + " (Telemetry / Feedback)";
    else
        midiInStr = "No MIDI In port assigned";
    drawConnectionRow("MIDI IN", midiInStr, SoundIdTheme::accentCyan, owner.hasMidiInConnected);'''
    ),
    (
        '''    lblStudioTitle.setText(juce::String::fromUTF8(u8"ENTORNO DE ESTUDIO, INTERFACES Y CABLEADO"), juce::dontSendNotification);''',
        '''    lblStudioTitle.setText("STUDIO ENVIRONMENT, INTERFACES & ROUTING", juce::dontSendNotification);'''
    ),
    (
        '''void DrawerSetupTab::setTargetHardwareInfo(const juce::String& hwName,
                                          const juce::String& submoduleName,
                                          const juce::String& routingText,
                                          const juce::Image& rasterImg,
                                          const juce::Drawable* svgDrawable,
                                          const juce::String& category)
{
    targetHwName = hwName.isNotEmpty() ? hwName : juce::String::fromUTF8(u8"Loopback Directo (Sin Target)");
    targetSubmoduleName = submoduleName.isNotEmpty() ? (juce::String::fromUTF8(u8"Submódulo: ") + submoduleName)
                                                     : juce::String::fromUTF8(u8"Submódulo: Perfil Predeterminado");
    targetRoutingText = routingText.isNotEmpty() ? routingText : "Routing: Self-Contained / Loopback";''',
        '''void DrawerSetupTab::setTargetHardwareInfo(const juce::String& hwName,
                                          const juce::String& submoduleName,
                                          const juce::String& routingText,
                                          const juce::Image& rasterImg,
                                          const juce::Drawable* svgDrawable,
                                          const juce::String& category,
                                          const juce::String& statusText,
                                          std::optional<juce::Colour> statusColour)
{
    targetHwName = hwName.isNotEmpty() ? hwName : "Direct Loopback (No Target Selected)";
    targetSubmoduleName = submoduleName.isNotEmpty() ? ("Submodule: " + submoduleName)
                                                     : "Profile: Default Factory Setup";
    targetRoutingText = routingText.isNotEmpty() ? routingText : "Direct Loopback / Audio Routing";

    if (statusText.isNotEmpty())
    {
        targetStatusBadgeText = statusText;
        targetStatusBadgeColour = statusColour.value_or(SoundIdTheme::accentAmber);
    }
    else if (category == "PLUGIN_VIRTUAL")
    {
        targetStatusBadgeText = "VIRTUAL PLUGIN";
        targetStatusBadgeColour = SoundIdTheme::accentBlue;
    }
    else if (hwName.isEmpty() || hwName.containsIgnoreCase("Loopback") || hwName.containsIgnoreCase("Sin Target"))
    {
        targetStatusBadgeText = "DIRECT LOOPBACK";
        targetStatusBadgeColour = SoundIdTheme::accentAmber;
    }
    else
    {
        targetStatusBadgeText = "SELECTED PROFILE";
        targetStatusBadgeColour = SoundIdTheme::accentAmber;
    }'''
    ),
    (
        '''    lblLatencyVal.setText(juce::String::fromUTF8(u8"Búfer: ") + juce::String(bufSize) + " samples (" + juce::String(latMs, 2) + " ms)", juce::dontSendNotification);''',
        '''    lblLatencyVal.setText("Buffer: " + juce::String(bufSize) + " samples (" + juce::String(latMs, 2) + " ms)", juce::dontSendNotification);'''
    )
])

# 3. HardwareWiringDiagramComponent.cpp
replace_in_file("src/gui/HardwareWiringDiagramComponent.cpp", [
    (
        '''    g.drawText("ESQUEMA DE CONEXIONADO (CLOSED LOOP)", rInner.removeFromTop(14.0f), juce::Justification::centredLeft, true);''',
        '''    g.drawText("WIRING SCHEMATIC (CLOSED LOOP)", rInner.removeFromTop(14.0f), juce::Justification::centredLeft, true);'''
    ),
    (
        '''        g.drawText(juce::String::fromUTF8(u8"Sin dispositivo seleccionado. Elige hardware o plugin para ver el diagrama de conexiones."),
                   rInner, juce::Justification::centred, true);''',
        '''        g.drawText("No device selected. Choose hardware or plugin to view wiring schematic.",
                   rInner, juce::Justification::centred, true);'''
    ),
    (
        '''    if (isMidiAutonomous)
        drawWire("Salida MIDI / USB", routingStimulusText, SoundIdTheme::accentBlue);
    else
        drawWire("Salida Audio 1 (DAC)", routingStimulusText, SoundIdTheme::accentGreen);

    drawWire("Entrada Audio 1 (ADC)", routingResponseText, SoundIdTheme::accentAmber);

    rInner.removeFromTop(4.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::italic));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("Nota: " + routingNotesText, rInner, juce::Justification::topLeft, true);''',
        '''    if (isMidiAutonomous)
        drawWire("MIDI / USB Output", routingStimulusText, SoundIdTheme::accentBlue);
    else
        drawWire("Audio Out 1 (DAC)", routingStimulusText, SoundIdTheme::accentGreen);

    drawWire("Audio In 1 (ADC)", routingResponseText, SoundIdTheme::accentAmber);

    rInner.removeFromTop(4.0f);
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::italic));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("Note: " + routingNotesText, rInner, juce::Justification::topLeft, true);'''
    )
])

# 4. HardwareDeviceDisplayCardComponent.cpp
replace_in_file("src/gui/HardwareDeviceDisplayCardComponent.cpp", [
    (
        '''    g.drawText(juce::String::fromUTF8(u8"Tipo: ") + currentHwCategory, typeArea, juce::Justification::centred, true);''',
        '''    g.drawText("Type: " + currentHwCategory, typeArea, juce::Justification::centred, true);'''
    )
])

# 5. SoundIdHardwareCatalogSelector.cpp
replace_in_file("src/gui/soundid/SoundIdHardwareCatalogSelector.cpp", [
    (
        '''    btnAutoDetect.setButtonText(juce::String::fromUTF8(u8"Auto-Detect (MIDI / USB)"));''',
        '''    btnAutoDetect.setButtonText("Auto-Detect Device (MIDI / USB)");'''
    ),
    (
        '''    btnLibreMode.setButtonText(juce::String::fromUTF8(u8"Dispositivo No Listado (Modo Libre)"));''',
        '''    btnLibreMode.setButtonText("Unlisted Device (Free Mode)");'''
    )
])

# 6. MainContentComponent.cpp
replace_in_file("src/gui/MainContentComponent.cpp", [
    (
        '''        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr)
        {
            setupInfoTab.setTargetHardwareInfo(
                juce::String(contract->displayName),
                drawer.getActiveFunctionDisplayName(),
                "Direct Loopback / Audio Routing",
                drawer.getActiveModelRasterImage(),
                nullptr,
                juce::String(contract->deviceType)
            );
            drawer.getSetupTab().setTargetHardwareInfo(
                juce::String(contract->displayName),
                drawer.getActiveFunctionDisplayName(),
                "Direct Loopback / Audio Routing",
                drawer.getActiveModelRasterImage(),
                nullptr,
                juce::String(contract->deviceType)
            );
        }''',
        '''        const auto* contract = hardwareManager.findContractById(hwId.toStdString());
        if (contract != nullptr)
        {
            bool isConnected = false;
            auto discovered = hardwareManager.getHotplugMonitor().getDiscoveredDevices();
            for (const auto& dev : discovered)
            {
                if (dev.contractId == contract->id || dev.displayName.containsIgnoreCase(contract->displayName))
                {
                    isConnected = true;
                    break;
                }
            }

            juce::String status = isConnected ? "CONNECTED HARDWARE" : "SELECTED PROFILE";
            juce::Colour col = isConnected ? SoundIdTheme::accentGreen : SoundIdTheme::accentAmber;

            setupInfoTab.setTargetHardwareInfo(
                juce::String(contract->displayName),
                drawer.getActiveFunctionDisplayName(),
                "Direct Loopback / Audio Routing",
                drawer.getActiveModelRasterImage(),
                nullptr,
                juce::String(contract->deviceType),
                status,
                col
            );
            drawer.getSetupTab().setTargetHardwareInfo(
                juce::String(contract->displayName),
                drawer.getActiveFunctionDisplayName(),
                "Direct Loopback / Audio Routing",
                drawer.getActiveModelRasterImage(),
                nullptr,
                juce::String(contract->deviceType),
                status,
                col
            );
        }'''
    )
])

print("ENGLISH REPLACEMENTS AND HARDWARE CONNECTION LOGIC APPLIED.")
