#include "SoundIdHardwareCatalogSelector.h"
#include "../SoundIdTheme.h"
#include <algorithm>

namespace abdaudiolab::gui
{

SoundIdHardwareCatalogSelector::SoundIdHardwareCatalogSelector()
{
    // Auto-detect button
    btnAutoDetect.setTooltip("Auto-detect connected synthesizer via MIDI SysEx / USB inquiry");
    btnAutoDetect.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnAutoDetect.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnAutoDetect.onClick = [this] {
        if (onAutoDetectRequested)
            onAutoDetectRequested();
    };
    addAndMakeVisible(btnAutoDetect);

    // Libre Mode button (Dispositivo no listado / pruebas libres)
    btnLibreMode.setButtonText("Unlisted Device (Free Mode)");
    btnLibreMode.setTooltip("Profile DIY synthesizers, analog pedals or modular gear not present in the fixed catalog");
    btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnLibreMode.onClick = [this] {
        isLibreMode = !isLibreMode;
        if (isLibreMode)
        {
            btnLibreMode.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93 Free Mode Active")));
            btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentAmber.withAlpha(0.25f));
            btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
        }
        else
        {
            btnLibreMode.setButtonText("Unlisted Device (Free Mode)");
            btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
            btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        }
        syncVisualCards();
        if (onSelectionChanged)
            onSelectionChanged(getSelectedHardwareId(), getSelectedFunctionId());
    };
    addAndMakeVisible(btnLibreMode);

    // Load Plugin button (hidden by default, visible when Plugin Virtual type selected)
    btnLoadPlugin.setButtonText("Load Plugin from file...");
    btnLoadPlugin.setTooltip("Select a .vst3 plugin file from disk to profile");
    btnLoadPlugin.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.2f));
    btnLoadPlugin.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    btnLoadPlugin.onClick = [this] {
        juce::Logger::writeToLog("[CatalogSelector] 'Cargar Plugin desde archivo...' button clicked.");
        if (onLoadPluginFromFileRequested)
            onLoadPluginFromFileRequested();
    };
    btnLoadPlugin.setVisible(false);
    addChildComponent(btnLoadPlugin);

    // Show Plugin GUI button (hidden by default)
    btnShowPluginGui.setButtonText("Open Plugin GUI");
    btnShowPluginGui.setTooltip("Display native graphical interface of the loaded plugin");
    btnShowPluginGui.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentPurple.withAlpha(0.2f));
    btnShowPluginGui.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentPurple);
    btnShowPluginGui.onClick = [this] {
        juce::Logger::writeToLog("[CatalogSelector] 'Abrir GUI del Plugin' button clicked.");
        if (onShowPluginGuiRequested)
            onShowPluginGuiRequested();
    };
    btnShowPluginGui.setVisible(false);
    addChildComponent(btnShowPluginGui);

    // Virtual Keyboard button (visible only in plugin mode)
    btnVirtualKeyboard.setButtonText("Teclado Virtual");
    btnVirtualKeyboard.setTooltip("Abrir teclado virtual interactivo para interpretar este plugin (sin emojis)");
    btnVirtualKeyboard.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.2f));
    btnVirtualKeyboard.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnVirtualKeyboard.onClick = [this] {
        juce::Logger::writeToLog("[CatalogSelector] 'Teclado Virtual' button clicked.");
        if (onOpenKeyboardRequested)
            onOpenKeyboardRequested();
    };
    btnVirtualKeyboard.setVisible(false);
    addChildComponent(btnVirtualKeyboard);

    // Cascading combos
    comboDeviceType.setTextWhenNothingSelected("1. Device Type (Synthesizer, Pedal, Eurorack...)");
    comboDeviceType.onChange = [this] {
        juce::Logger::writeToLog("[CatalogSelector] comboDeviceType changed: '" + comboDeviceType.getText() + "'");
        rebuildBrandsForCurrentType();
        resized(); // Re-layout buttons for plugin vs hardware mode
    };
    addAndMakeVisible(comboDeviceType);

    comboBrand.setTextWhenNothingSelected("2. Brand / Manufacturer...");
    comboBrand.onChange = [this] {
        juce::Logger::writeToLog("[CatalogSelector] comboBrand changed: '" + comboBrand.getText() + "'");
        rebuildModelsForCurrentBrand();
    };
    addAndMakeVisible(comboBrand);

    comboModel.setTextWhenNothingSelected("3. Model...");
    comboModel.onChange = [this] {
        juce::Logger::writeToLog("[CatalogSelector] comboModel changed: '" + comboModel.getText()
            + "' (Id: " + juce::String(comboModel.getSelectedId()) + ", isPluginMode: " + juce::String(isPluginMode ? "YES" : "NO") + ")");
        rebuildObjectivesForCurrentModel();
        if (isPluginMode)
        {
            if (auto* desc = getSelectedPluginDescription())
            {
                juce::Logger::writeToLog("[CatalogSelector] Triggering onPluginSelected: '" + desc->name + "'");
                if (onPluginSelected)
                    onPluginSelected(*desc);
            }
            else
            {
                juce::Logger::writeToLog("[CatalogSelector WARNING] getSelectedPluginDescription() returned nullptr");
            }
        }
    };
    addAndMakeVisible(comboModel);

    comboObjective.setTextWhenNothingSelected("4. Target / Measurement Block...");
    comboObjective.onChange = [this] {
        juce::Logger::writeToLog("[CatalogSelector] comboObjective changed: '" + comboObjective.getText()
            + "' (Id: " + juce::String(comboObjective.getSelectedId()) + ")");
        syncVisualCards();
        if (isPluginMode)
        {
            if (auto* desc = getSelectedPluginDescription())
            {
                juce::Logger::writeToLog("[CatalogSelector] comboObjective triggered onPluginSelected: '" + desc->name + "'");
                if (onPluginSelected)
                    onPluginSelected(*desc);
            }
        }
        else if (onSelectionChanged)
        {
            onSelectionChanged(getSelectedHardwareId(), getSelectedFunctionId());
        }
    };
    addAndMakeVisible(comboObjective);

    // Visual cards
    addAndMakeVisible(deviceDisplayCard);
    addAndMakeVisible(wiringDiagram);

    // Navigation buttons
    btnContinue.setButtonText(juce::String::fromUTF8("Proceed to Run Session (Step 3) \xE2\x86\x92"));
    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnContinue.onClick = [this] {
        if (onContinueRequested)
            onContinueRequested();
    };
    addAndMakeVisible(btnContinue);

    lblLockedBanner.setText("Hardware profile locked for the active session.", juce::dontSendNotification);
    lblLockedBanner.setFont(juce::FontOptions(11.0f, juce::Font::italic));
    lblLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblLockedBanner.setVisible(false);
    addChildComponent(lblLockedBanner);

    btnUnlock.setButtonText("Change Hardware / Unlock");
    btnUnlock.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnUnlock.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnUnlock.onClick = [this] {
        if (onResetOrUnlockRequested)
            onResetOrUnlockRequested();
        else
            setHardwareLocked(false);
    };
    btnUnlock.setVisible(false);
    addChildComponent(btnUnlock);

    updateTheme();
}

void SoundIdHardwareCatalogSelector::updateTheme()
{
    // Auto-detect and free mode buttons
    btnAutoDetect.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnAutoDetect.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);

    if (isLibreMode)
    {
        btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentAmber.withAlpha(0.25f));
        btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
    }
    else
    {
        btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
        btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    }

    btnLoadPlugin.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.2f));
    btnLoadPlugin.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);

    btnShowPluginGui.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentPurple.withAlpha(0.2f));
    btnShowPluginGui.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentPurple);

    btnVirtualKeyboard.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.2f));
    btnVirtualKeyboard.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);

    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    lblLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);

    btnUnlock.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnUnlock.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    for (auto* combo : { &comboDeviceType, &comboBrand, &comboModel, &comboObjective })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, SoundIdTheme::bgCard);
        combo->setColour(juce::ComboBox::textColourId, SoundIdTheme::textPrimary);
        combo->setColour(juce::ComboBox::outlineColourId, SoundIdTheme::borderSubtle);
        combo->setColour(juce::ComboBox::arrowColourId, SoundIdTheme::textSecondary);
    }

    deviceDisplayCard.repaint();
    wiringDiagram.repaint();
    repaint();
}

void SoundIdHardwareCatalogSelector::setContracts(const std::vector<core::HardwareContract>& contracts)
{
    contractsList = contracts;
    if (isPluginMode)
        return; // Preserve active selection and UI state when in virtual plugin mode
    rebuildDeviceTypes();
}

void SoundIdHardwareCatalogSelector::setAvailablePlugins(const std::vector<juce::PluginDescription>& plugins)
{
    juce::Logger::writeToLog("[CatalogSelector] setAvailablePlugins received " + juce::String(plugins.size()) + " plugins.");
    availablePlugins = plugins;
    if (isPluginMode)
        rebuildBrandsForCurrentType();
}

void SoundIdHardwareCatalogSelector::setHardwareLocked(bool locked)
{
    isHardwareLocked = locked;
    comboDeviceType.setEnabled(!locked);
    comboBrand.setEnabled(!locked);
    comboModel.setEnabled(!locked);
    comboObjective.setEnabled(!locked);

    btnAutoDetect.setVisible(!locked && !isPluginMode);
    btnLibreMode.setVisible(!locked && !isPluginMode);
    btnLibreMode.setEnabled(!locked);

    btnLoadPlugin.setVisible(!locked && isPluginMode);
    btnShowPluginGui.setVisible(isPluginMode);

    lblLockedBanner.setVisible(locked);
    btnUnlock.setVisible(locked);

    if (locked)
    {
        if (isPluginMode)
            lblLockedBanner.setText("Active virtual plugin assigned for the session.", juce::dontSendNotification);
        else
            lblLockedBanner.setText("Hardware profile locked for the active session.", juce::dontSendNotification);
    }

    btnContinue.setButtonText(juce::String::fromUTF8("Proceed to Run Session (Step 3) \xE2\x86\x92"));
    resized();
    repaint();
}

void SoundIdHardwareCatalogSelector::rebuildDeviceTypes()
{
    comboDeviceType.clear(juce::dontSendNotification);
    std::set<juce::String> types;

    for (const auto& c : contractsList)
    {
        juce::String dt = juce::String(c.deviceType).trim();
        // Virtual plugins are accessed via the dedicated unified 'Plugin Virtual' entry
        if (dt.isNotEmpty() && !dt.equalsIgnoreCase("SOFTWARE_PLUGIN"))
            types.insert(dt);
    }

    int id = 1;
    for (const auto& t : types)
    {
        comboDeviceType.addItem(t, id++);
    }

    // Always append Plugin Virtual type
    comboDeviceType.addItem("Virtual Plugin (VST3 / AU / LV2)", id++);

    comboDeviceType.setSelectedId(0, juce::dontSendNotification);
    comboBrand.clear(juce::dontSendNotification);
    comboModel.clear(juce::dontSendNotification);
    comboObjective.clear(juce::dontSendNotification);
    deviceDisplayCard.clear();
    wiringDiagram.clear();
}

void SoundIdHardwareCatalogSelector::rebuildBrandsForCurrentType()
{
    comboBrand.clear(juce::dontSendNotification);
    juce::String selectedType = comboDeviceType.getText();

    if (comboDeviceType.getSelectedId() <= 0)
    {
        comboModel.clear(juce::dontSendNotification);
        comboObjective.clear(juce::dontSendNotification);
        deviceDisplayCard.clear();
        wiringDiagram.clear();
        return;
    }

    // Detect if we're in plugin virtual mode
    isPluginMode = selectedType.containsIgnoreCase("Plugin") || selectedType.equalsIgnoreCase("SOFTWARE_PLUGIN");
    btnLoadPlugin.setVisible(isPluginMode && !isHardwareLocked);
    btnShowPluginGui.setVisible(isPluginMode);
    btnAutoDetect.setVisible(!isPluginMode && !isHardwareLocked);
    btnLibreMode.setVisible(!isPluginMode && !isHardwareLocked);

    if (isPluginMode)
    {
        // Populate brands from scanned plugins
        std::set<juce::String> pluginBrands;
        for (const auto& pd : availablePlugins)
        {
            juce::String mfr = pd.manufacturerName.trim();
            if (mfr.isNotEmpty())
                pluginBrands.insert(mfr);
        }

        int id = 1;
        for (const auto& b : pluginBrands)
            comboBrand.addItem(b, id++);

        // Always add a generic entry if no plugins scanned
        if (comboBrand.getNumItems() == 0)
            comboBrand.addItem("(No plugins scanned)", 1);

        comboBrand.setSelectedId(0, juce::dontSendNotification);
        comboModel.clear(juce::dontSendNotification);
        comboObjective.clear(juce::dontSendNotification);
        deviceDisplayCard.clear();
        wiringDiagram.clear();
        return;
    }

    std::set<juce::String> brands;

    for (const auto& c : contractsList)
    {
        if (juce::String(c.deviceType).trim().equalsIgnoreCase(selectedType))
        {
            juce::String b = juce::String(c.brand).trim();
            if (b.isEmpty()) b = juce::String(c.manufacturer).trim();
            if (b.isNotEmpty())
                brands.insert(b);
        }
    }

    int id = 1;
    for (const auto& b : brands)
    {
        comboBrand.addItem(b, id++);
    }

    comboBrand.setSelectedId(0, juce::dontSendNotification);
    comboModel.clear(juce::dontSendNotification);
    comboObjective.clear(juce::dontSendNotification);
    deviceDisplayCard.clear();
    wiringDiagram.clear();
}

void SoundIdHardwareCatalogSelector::rebuildModelsForCurrentBrand()
{
    comboModel.clear(juce::dontSendNotification);

    if (comboBrand.getSelectedId() <= 0)
    {
        comboObjective.clear(juce::dontSendNotification);
        deviceDisplayCard.clear();
        wiringDiagram.clear();
        return;
    }

    if (isPluginMode)
    {
        // Populate models from scanned plugins filtered by selected brand
        juce::String selectedBrand = comboBrand.getText();
        for (size_t i = 0; i < availablePlugins.size(); ++i)
        {
            const auto& pd = availablePlugins[i];
            if (pd.manufacturerName.trim().equalsIgnoreCase(selectedBrand))
            {
                juce::String label = pd.name;
                if (pd.isInstrument)
                    label += " [Instrument]";
                else
                    label += " [Effect]";
                comboModel.addItem(label, static_cast<int>(i + 1));
            }
        }

        comboModel.setSelectedId(0, juce::dontSendNotification);
        comboObjective.clear(juce::dontSendNotification);
        deviceDisplayCard.clear();
        wiringDiagram.clear();
        return;
    }

    juce::String selectedType = comboDeviceType.getText();
    juce::String selectedBrand = comboBrand.getText();

    for (size_t i = 0; i < contractsList.size(); ++i)
    {
        const auto& c = contractsList[i];
        bool matchType = juce::String(c.deviceType).trim().equalsIgnoreCase(selectedType);
        juce::String b = juce::String(c.brand).trim();
        if (b.isEmpty()) b = juce::String(c.manufacturer).trim();
        bool matchBrand = b.equalsIgnoreCase(selectedBrand);

        if (matchType && matchBrand)
        {
            comboModel.addItem(c.displayName, static_cast<int>(i + 1));
        }
    }

    comboModel.setSelectedId(0, juce::dontSendNotification);
    comboObjective.clear(juce::dontSendNotification);
    deviceDisplayCard.clear();
    wiringDiagram.clear();
}

void SoundIdHardwareCatalogSelector::rebuildObjectivesForCurrentModel()
{
    comboObjective.clear(juce::dontSendNotification);

    if (comboModel.getSelectedId() <= 0)
    {
        deviceDisplayCard.clear();
        wiringDiagram.clear();
        return;
    }

    if (isPluginMode)
    {
        if (auto* desc = getSelectedPluginDescription())
        {
            juce::String mainLabel = desc->isInstrument 
                ? "Acoustic Profiling / MIDI Modulation [SynthOscillator]"
                : "Frequency Response / THD [SpectrumFilter]";
            comboObjective.addItem(mainLabel, 1);
        }
        else
        {
            comboObjective.addItem("Virtual DSP Processing [AudioProcessor]", 1);
        }
        comboObjective.addItem("Free / Manual Inspection", 9999);
        comboObjective.setSelectedId(1, juce::dontSendNotification);
        syncVisualCards();
        return;
    }

    int selIndex = comboModel.getSelectedId() - 1;

    if (selIndex >= 0 && selIndex < static_cast<int>(contractsList.size()))
    {
        const auto& c = contractsList[static_cast<size_t>(selIndex)];
        int id = 1;
        for (const auto& fn : c.functions)
        {
            juce::String label = fn.name;
            if (fn.measurementRecipe.recipeType.length() > 0)
                label += " [" + juce::String(fn.measurementRecipe.recipeType) + "]";
            comboObjective.addItem(label, id++);
        }
    }

    // Always append "Libre / Modo Manual" as an option
    comboObjective.addItem("Libre / Inspeccion Manual", 9999);

    if (comboObjective.getNumItems() > 0)
        comboObjective.setSelectedId(1, juce::sendNotification);
}

void SoundIdHardwareCatalogSelector::syncVisualCards()
{
    if (isPluginMode)
    {
        if (auto* desc = getSelectedPluginDescription())
        {
            deviceDisplayCard.setPluginInfo(desc->name, desc->manufacturerName, desc->pluginFormatName, desc->isInstrument);
            if (desc->isInstrument)
            {
                wiringDiagram.setPluginRouting(
                    "Internal MIDI Sequencer",
                    "Plugin (MIDI In)",
                    "Plugin (Audio Out)",
                    "ABDAudioLab Capture",
                    "Direct digital loop. Zero converter latency. Internal MIDI + Audio bus.");
            }
            else
            {
                wiringDiagram.setPluginRouting(
                    "Stimulus Generator",
                    "Plugin (Audio In)",
                    "Plugin (Audio Out)",
                    "ABDAudioLab Capture",
                    "Direct digital closed loop. Zero physical converter coloration.");
            }
        }
        else
        {
            deviceDisplayCard.setPluginInfo("Plugin Virtual", "Generic", "VST3 / AU", true);
            wiringDiagram.setPluginRouting(
                juce::String::fromUTF8(u8"Stimulus Generator"),
                juce::String::fromUTF8(u8"Plugin (In)"),
                juce::String::fromUTF8(u8"Plugin (Out)"),
                juce::String::fromUTF8(u8"ABDAudioLab Capture"),
                juce::String::fromUTF8(u8"Internal Direct Bus \u2013 Zero Converter Coloration"));
        }
        repaint();
        return;
    }

    int selIndex = comboModel.getSelectedId() - 1;
    if (selIndex >= 0 && selIndex < static_cast<int>(contractsList.size()) && !isLibreMode)
    {
        const auto& c = contractsList[static_cast<size_t>(selIndex)];
        deviceDisplayCard.setDevice(&c);

        int fnSel = comboObjective.getSelectedId() - 1;
        if (fnSel >= 0 && fnSel < static_cast<int>(c.functions.size()))
        {
            const auto& fn = c.functions[static_cast<size_t>(fnSel)];
            bool isAutonomous = (c.deviceType == "AUTOMATED_SYSEX" || c.deviceType == "AUTOMATED_MIDI_CC");
            wiringDiagram.setRouting(fn.routingGuide.stimulusOutput,
                                     fn.routingGuide.responseInput,
                                     fn.routingGuide.notes,
                                     isAutonomous);
        }
        else
        {
            wiringDiagram.setRouting("Audio Out 1 (L)", "Audio In 1 (L)", "Free Mode / Manual Routing", false);
        }
    }
    else
    {
        deviceDisplayCard.clear();
        wiringDiagram.setRouting("Audio Out 1 (L)", "Audio In 1 (L)", "Custom / Unlisted Setup", false);
    }
    repaint();
}

void SoundIdHardwareCatalogSelector::resetSelection()
{
    isHardwareLocked = false;
    isLibreMode = false;
    isPluginMode = false;
    btnLoadPlugin.setVisible(false);
    btnShowPluginGui.setVisible(false);
    btnAutoDetect.setVisible(true);
    btnLibreMode.setVisible(true);
    btnLibreMode.setButtonText("Unlisted Device (Free Mode)");
    comboDeviceType.setSelectedId(0, juce::dontSendNotification);
    comboBrand.clear(juce::dontSendNotification);
    comboModel.clear(juce::dontSendNotification);
    comboObjective.clear(juce::dontSendNotification);
    deviceDisplayCard.clear();
    wiringDiagram.clear();
    repaint();
}

juce::String SoundIdHardwareCatalogSelector::getSelectedHardwareId() const
{
    if (isLibreMode) return "LIBRE_CUSTOM";
    if (isPluginMode)
    {
        if (auto* desc = getSelectedPluginDescription())
            return "plugin_" + juce::File(desc->fileOrIdentifier).getFileNameWithoutExtension();
        return "PLUGIN_VIRTUAL";
    }
    int sel = comboModel.getSelectedId() - 1;
    if (sel >= 0 && sel < static_cast<int>(contractsList.size()))
        return juce::String(contractsList[static_cast<size_t>(sel)].id);
    return {};
}

juce::String SoundIdHardwareCatalogSelector::getSelectedFunctionId() const
{
    if (isLibreMode) return "CUSTOM_FUNCTION";
    if (isPluginMode) return "vst3_audio_processor";
    int sel = comboModel.getSelectedId() - 1;
    int fnSel = comboObjective.getSelectedId() - 1;
    if (sel >= 0 && sel < static_cast<int>(contractsList.size()))
    {
        const auto& fns = contractsList[static_cast<size_t>(sel)].functions;
        if (fnSel >= 0 && fnSel < static_cast<int>(fns.size()))
            return juce::String(fns[static_cast<size_t>(fnSel)].id);
    }
    return {};
}

juce::String SoundIdHardwareCatalogSelector::getSelectedDeviceType() const
{
    return comboDeviceType.getSelectedId() > 0 ? comboDeviceType.getText() : juce::String();
}

juce::String SoundIdHardwareCatalogSelector::getSelectedBrand() const
{
    return comboBrand.getSelectedId() > 0 ? comboBrand.getText() : juce::String();
}

const juce::PluginDescription* SoundIdHardwareCatalogSelector::getSelectedPluginDescription() const
{
    if (!isPluginMode) return nullptr;
    int sel = comboModel.getSelectedId() - 1;
    if (sel >= 0 && sel < static_cast<int>(availablePlugins.size()))
        return &availablePlugins[static_cast<size_t>(sel)];
    return nullptr;
}

void SoundIdHardwareCatalogSelector::setSelectedHardware(const juce::String& hwId, const juce::String& funcId)
{
    for (size_t i = 0; i < contractsList.size(); ++i)
    {
        if (contractsList[i].id == hwId.toStdString())
        {
            const auto& c = contractsList[i];
            for (int t = 1; t <= comboDeviceType.getNumItems(); ++t)
            {
                if (comboDeviceType.getItemText(t - 1).equalsIgnoreCase(juce::String(c.deviceType)))
                {
                    comboDeviceType.setSelectedId(t, juce::dontSendNotification);
                    break;
                }
            }
            rebuildBrandsForCurrentType();

            juce::String b = juce::String(c.brand).trim();
            if (b.isEmpty()) b = juce::String(c.manufacturer).trim();
            for (int bIdx = 1; bIdx <= comboBrand.getNumItems(); ++bIdx)
            {
                if (comboBrand.getItemText(bIdx - 1).equalsIgnoreCase(b))
                {
                    comboBrand.setSelectedId(bIdx, juce::dontSendNotification);
                    break;
                }
            }
            rebuildModelsForCurrentBrand();

            comboModel.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
            rebuildObjectivesForCurrentModel();

            if (funcId.isNotEmpty())
            {
                for (size_t f = 0; f < c.functions.size(); ++f)
                {
                    if (c.functions[f].id == funcId.toStdString())
                    {
                        comboObjective.setSelectedId(static_cast<int>(f + 1), juce::dontSendNotification);
                        break;
                    }
                }
            }
            syncVisualCards();
            return;
        }
    }
}

void SoundIdHardwareCatalogSelector::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgLight);
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);
}

void SoundIdHardwareCatalogSelector::resized()
{
    auto b = getLocalBounds().reduced(16);

    // 1. Fila Superior: Auto-Detect y Dispositivo No Listado (Modo Libre) / Plugin buttons
    auto topRow = b.removeFromTop(34);
    if (isHardwareLocked)
    {
        btnAutoDetect.setBounds({});
        btnLibreMode.setBounds({});
        btnLoadPlugin.setBounds({});

        int unlockW = 240;
        btnUnlock.setBounds(topRow.removeFromRight(unlockW));
        topRow.removeFromRight(10);

        if (isPluginMode)
        {
            int kbdW = 150;
            btnVirtualKeyboard.setBounds(topRow.removeFromRight(kbdW));
            topRow.removeFromRight(10);
            int guiW = 170;
            btnShowPluginGui.setBounds(topRow.removeFromRight(guiW));
            topRow.removeFromRight(10);
        }
        else
        {
            btnShowPluginGui.setBounds({});
            btnVirtualKeyboard.setBounds({});
        }

        lblLockedBanner.setBounds(topRow);
    }
    else
    {
        lblLockedBanner.setBounds({});
        btnUnlock.setBounds({});

        if (isPluginMode)
        {
            btnAutoDetect.setBounds({});
            btnLibreMode.setBounds({});
            int loadBtnW = juce::jmin(210, topRow.getWidth() / 3 - 6);
            btnLoadPlugin.setBounds(topRow.removeFromLeft(loadBtnW));
            topRow.removeFromLeft(10);
            int guiBtnW = juce::jmin(170, topRow.getWidth() / 3 - 6);
            btnShowPluginGui.setBounds(topRow.removeFromLeft(guiBtnW));
            topRow.removeFromLeft(10);
            int kbdBtnW = juce::jmin(150, topRow.getWidth());
            btnVirtualKeyboard.setBounds(topRow.removeFromLeft(kbdBtnW));
        }
        else
        {
            btnLoadPlugin.setBounds({});
            btnShowPluginGui.setBounds({});
            btnVirtualKeyboard.setBounds({});
            int autoBtnW = juce::jmin(250, topRow.getWidth() / 2 - 6);
            btnAutoDetect.setBounds(topRow.removeFromLeft(autoBtnW));
            topRow.removeFromLeft(12);
            int libreBtnW = juce::jmin(250, topRow.getWidth());
            btnLibreMode.setBounds(topRow.removeFromLeft(libreBtnW));
        }
    }

    b.removeFromTop(12);

    // 2. Selectores en dos filas (2 combos por fila) para legibilidad óptima
    int comboGap = 12;
    int comboH = 32;

    // Fila 1: 1. Tipo de Dispositivo y 2. Marca / Fabricante
    auto combosRow1 = b.removeFromTop(comboH);
    int colW = (combosRow1.getWidth() - comboGap) / 2;
    comboDeviceType.setBounds(combosRow1.removeFromLeft(colW));
    combosRow1.removeFromLeft(comboGap);
    comboBrand.setBounds(combosRow1);

    b.removeFromTop(8);

    // Fila 2: 3. Modelo y 4. Objetivo / Bloque
    auto combosRow2 = b.removeFromTop(comboH);
    comboModel.setBounds(combosRow2.removeFromLeft(colW));
    combosRow2.removeFromLeft(comboGap);
    comboObjective.setBounds(combosRow2);

    b.removeFromTop(14);

    // 3. Barra Inferior de Navegación (Continuar a Ejecución de Sesión)
    auto bottomBar = b.removeFromBottom(38);
    btnContinue.setBounds(bottomBar.removeFromRight(280));

    b.removeFromBottom(12);

    // 4. Zona Central Inferior: Columna Izquierda (Tarjeta de Hardware más ancha ~60%) y Derecha (Diagrama de Conexiones ~40%)
    int colGap = 16;
    int leftColW = static_cast<int>((b.getWidth() - colGap) * 0.60f);

    auto leftCol = b.removeFromLeft(leftColW);
    b.removeFromLeft(colGap);
    auto rightCol = b;

    deviceDisplayCard.setBounds(leftCol);
    wiringDiagram.setBounds(rightCol);
}

} // namespace abdaudiolab::gui
