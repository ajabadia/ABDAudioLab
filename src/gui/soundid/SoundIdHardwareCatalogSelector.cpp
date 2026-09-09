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

    // Libre Mode button
    btnLibreMode.setTooltip("Caracterizar cualquier hardware no perfilado previamente o pruebas libres");
    btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnLibreMode.onClick = [this] {
        isLibreMode = !isLibreMode;
        if (isLibreMode)
        {
            btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentAmber.withAlpha(0.2f));
            btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
        }
        else
        {
            btnLibreMode.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
            btnLibreMode.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        }
        syncVisualCards();
        if (onSelectionChanged)
            onSelectionChanged(getSelectedHardwareId(), getSelectedFunctionId());
    };
    addAndMakeVisible(btnLibreMode);

    // Cascading combos
    comboDeviceType.setTextWhenNothingSelected("1. Tipo de Dispositivo (Sintetizador, Pedal, Eurorack...)");
    comboDeviceType.onChange = [this] {
        rebuildBrandsForCurrentType();
    };
    addAndMakeVisible(comboDeviceType);

    comboBrand.setTextWhenNothingSelected("2. Marca / Fabricante...");
    comboBrand.onChange = [this] {
        rebuildModelsForCurrentBrand();
    };
    addAndMakeVisible(comboBrand);

    comboModel.setTextWhenNothingSelected("3. Modelo...");
    comboModel.onChange = [this] {
        rebuildObjectivesForCurrentModel();
    };
    addAndMakeVisible(comboModel);

    comboObjective.setTextWhenNothingSelected(juce::String::fromUTF8(u8"4. Objetivo / Bloque de Medición..."));
    comboObjective.onChange = [this] {
        syncVisualCards();
        if (onSelectionChanged)
            onSelectionChanged(getSelectedHardwareId(), getSelectedFunctionId());
    };
    addAndMakeVisible(comboObjective);

    // Visual cards
    addAndMakeVisible(deviceDisplayCard);
    addAndMakeVisible(wiringDiagram);

    // Navigation buttons
    btnContinue.setButtonText(juce::String::fromUTF8(u8"Continuar a Calibración (Paso 2) ➔"));
    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnContinue.onClick = [this] {
        if (onContinueRequested)
            onContinueRequested();
    };
    addAndMakeVisible(btnContinue);

    lblLockedBanner.setText("Perfil de hardware fijado para la sesión activa.", juce::dontSendNotification);
    lblLockedBanner.setFont(juce::FontOptions(11.0f, juce::Font::italic));
    lblLockedBanner.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblLockedBanner.setVisible(false);
    addChildComponent(lblLockedBanner);

    btnUnlock.setButtonText("Cambiar Hardware / Desbloquear");
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
}

void SoundIdHardwareCatalogSelector::setContracts(const std::vector<core::HardwareContract>& contracts)
{
    contractsList = contracts;
    rebuildDeviceTypes();
}

void SoundIdHardwareCatalogSelector::setHardwareLocked(bool locked)
{
    isHardwareLocked = locked;
    comboDeviceType.setEnabled(!locked);
    comboBrand.setEnabled(!locked);
    comboModel.setEnabled(!locked);
    comboObjective.setEnabled(!locked);
    btnAutoDetect.setVisible(!locked);
    btnLibreMode.setEnabled(!locked);

    lblLockedBanner.setVisible(locked);
    btnUnlock.setVisible(locked);

    btnContinue.setButtonText(locked ? juce::String::fromUTF8(u8"Ir a Ejecución de Sesión (Paso 3) ➔")
                                    : juce::String::fromUTF8(u8"Continuar a Calibración (Paso 2) ➔"));
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
        if (dt.isNotEmpty())
            types.insert(dt);
    }

    int id = 1;
    for (const auto& t : types)
    {
        comboDeviceType.addItem(t, id++);
    }

    if (comboDeviceType.getNumItems() > 0)
    {
        comboDeviceType.setSelectedId(1, juce::dontSendNotification);
        rebuildBrandsForCurrentType();
    }
}

void SoundIdHardwareCatalogSelector::rebuildBrandsForCurrentType()
{
    comboBrand.clear(juce::dontSendNotification);
    juce::String selectedType = comboDeviceType.getText();
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

    if (comboBrand.getNumItems() > 0)
    {
        comboBrand.setSelectedId(1, juce::dontSendNotification);
        rebuildModelsForCurrentBrand();
    }
    else
    {
        rebuildModelsForCurrentBrand();
    }
}

void SoundIdHardwareCatalogSelector::rebuildModelsForCurrentBrand()
{
    comboModel.clear(juce::dontSendNotification);
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

    if (comboModel.getNumItems() > 0)
    {
        comboModel.setSelectedId(comboModel.getItemId(0), juce::dontSendNotification);
        rebuildObjectivesForCurrentModel();
    }
    else
    {
        rebuildObjectivesForCurrentModel();
    }
}

void SoundIdHardwareCatalogSelector::rebuildObjectivesForCurrentModel()
{
    comboObjective.clear(juce::dontSendNotification);
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

    syncVisualCards();
}

void SoundIdHardwareCatalogSelector::syncVisualCards()
{
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
            wiringDiagram.setRouting("Audio Out 1 (L)", "Audio In 1 (L)", juce::String::fromUTF8(u8"Modo Libre / Conexión Manual"), false);
        }
    }
    else
    {
        deviceDisplayCard.setDevice(nullptr);
        wiringDiagram.setRouting("Audio Out 1 (L)", "Audio In 1 (L)", juce::String::fromUTF8(u8"Configuración Personalizada / Libre"), false);
    }
    repaint();
}

juce::String SoundIdHardwareCatalogSelector::getSelectedHardwareId() const
{
    if (isLibreMode) return "LIBRE_CUSTOM";
    int sel = comboModel.getSelectedId() - 1;
    if (sel >= 0 && sel < static_cast<int>(contractsList.size()))
        return juce::String(contractsList[static_cast<size_t>(sel)].id);
    return {};
}

juce::String SoundIdHardwareCatalogSelector::getSelectedFunctionId() const
{
    if (isLibreMode) return "CUSTOM_FUNCTION";
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
    return comboDeviceType.getText();
}

juce::String SoundIdHardwareCatalogSelector::getSelectedBrand() const
{
    return comboBrand.getText();
}

void SoundIdHardwareCatalogSelector::setSelectedHardware(const juce::String& hwId, const juce::String& funcId)
{
    for (size_t i = 0; i < contractsList.size(); ++i)
    {
        if (contractsList[i].id == hwId.toStdString())
        {
            const auto& c = contractsList[i];
            comboDeviceType.setText(c.deviceType, juce::dontSendNotification);
            rebuildBrandsForCurrentType();

            juce::String b = juce::String(c.brand).trim();
            if (b.isEmpty()) b = juce::String(c.manufacturer).trim();
            comboBrand.setText(b, juce::dontSendNotification);
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

    // Top Row: Auto-Detect and Libre Mode
    auto topRow = b.removeFromTop(36);
    if (!isHardwareLocked)
    {
        btnAutoDetect.setBounds(topRow.removeFromLeft(260));
        topRow.removeFromLeft(12);
        btnLibreMode.setBounds(topRow.removeFromLeft(180));
    }
    else
    {
        lblLockedBanner.setBounds(topRow.removeFromLeft(360));
        btnUnlock.setBounds(topRow.removeFromRight(220));
    }

    b.removeFromTop(16);

    // Bottom Navigation Bar
    auto bottomBar = b.removeFromBottom(42);
    btnContinue.setBounds(bottomBar.removeFromRight(260));

    b.removeFromBottom(16);

    // Split Canvas: Left Column = 4-Step Cascading Dropdowns; Right Column = Visual Cards
    auto leftCol = b.removeFromLeft(juce::jmax(300, b.getWidth() / 2 - 12));
    b.removeFromLeft(24);
    auto rightCol = b;

    // Left Column: 4 Cascading Dropdowns
    const int comboHeight = 36;
    const int spacing = 16;

    comboDeviceType.setBounds(leftCol.removeFromTop(comboHeight));
    leftCol.removeFromTop(spacing);
    comboBrand.setBounds(leftCol.removeFromTop(comboHeight));
    leftCol.removeFromTop(spacing);
    comboModel.setBounds(leftCol.removeFromTop(comboHeight));
    leftCol.removeFromTop(spacing);
    comboObjective.setBounds(leftCol.removeFromTop(comboHeight));

    // Right Column: Display Card (top) + Wiring Diagram (bottom)
    int cardHeight = (rightCol.getHeight() - 16) / 2;
    deviceDisplayCard.setBounds(rightCol.removeFromTop(cardHeight));
    rightCol.removeFromTop(16);
    wiringDiagram.setBounds(rightCol);
}

} // namespace abdaudiolab::gui
