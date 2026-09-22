#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../core/HardwareContractRegistry.h"
#include "../HardwareWiringDiagramComponent.h"
#include "../HardwareDeviceDisplayCardComponent.h"
#include <vector>
#include <set>
#include <functional>

namespace abdaudiolab::gui
{

/**
 * @brief Cascading 4-step hardware selector:
 *        1. Tipo (Synthesizer, Pedal, Eurorack, Loopback/Virtual, Libre)
 *        2. Marca (Filtered by selected type)
 *        3. Modelo (Filtered by selected brand)
 *        4. Objetivo / Receta (Available functions/recipes or "Libre / Modo Manual")
 *
 * Design:
 * - Clean, spacious cards matching Sonarworks SoundID Reference / Measure aesthetics.
 * - Dynamic data driven purely from HardwareContract metadata (not hardcoded).
 * - "Libre" option always available for custom unprofiled hardware.
 * - Canonical authority: This selector is the single source of truth for hardware selection (HardwareRoutingPanel removed in HITO-07 Phase 4B).
 */
class SoundIdHardwareCatalogSelector : public juce::Component
{
public:
    SoundIdHardwareCatalogSelector();
    ~SoundIdHardwareCatalogSelector() override = default;

    void setContracts(const std::vector<core::HardwareContract>& contracts);
    void setSelectedHardware(const juce::String& hwId, const juce::String& funcId = {});
    void setAvailablePlugins(const std::vector<juce::PluginDescription>& plugins);
    [[nodiscard]] bool isPluginVirtualMode() const noexcept { return isPluginMode; }
    
    [[nodiscard]] juce::String getSelectedHardwareId() const;
    [[nodiscard]] juce::String getSelectedFunctionId() const;
    [[nodiscard]] juce::String getSelectedDeviceType() const;
    [[nodiscard]] juce::String getSelectedBrand() const;
    [[nodiscard]] bool isCustomOrLibre() const noexcept { return isLibreMode; }
    [[nodiscard]] const juce::PluginDescription* getSelectedPluginDescription() const;

    void setHardwareLocked(bool locked);
    [[nodiscard]] bool isLocked() const noexcept { return isHardwareLocked; }
    void resetSelection();
    void updateTheme();

    // Callbacks
    std::function<void(const juce::String& hwId, const juce::String& funcId)> onSelectionChanged;
    std::function<void(const juce::PluginDescription& pluginDesc)> onPluginSelected;
    std::function<void()> onContinueRequested;
    std::function<void()> onAutoDetectRequested;
    std::function<void()> onResetOrUnlockRequested;
    std::function<void()> onLoadPluginFromFileRequested;
    std::function<void()> onShowPluginGuiRequested;
    std::function<void()> onOpenKeyboardRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    std::vector<core::HardwareContract> contractsList;
    std::vector<juce::PluginDescription> availablePlugins;
    bool isHardwareLocked { false };
    bool isLibreMode { false };
    bool isPluginMode { false };

    // Cascading Dropdowns
    juce::ComboBox comboDeviceType;
    juce::ComboBox comboBrand;
    juce::ComboBox comboModel;
    juce::ComboBox comboObjective;

    // Header & Buttons
    juce::TextButton btnAutoDetect { "Auto-Detect Device (MIDI / USB)" };
    juce::TextButton btnLibreMode { "Unlisted Device (Free Mode)" };
    juce::TextButton btnLoadPlugin;
    juce::TextButton btnShowPluginGui;
    juce::TextButton btnVirtualKeyboard;
    juce::TextButton btnContinue { "Proceed to Run Session (Step 3) \u2192" };
    juce::TextButton btnUnlock { "Change Hardware / Unlock" };
    juce::Label lblLockedBanner;

    // Visual cards
    HardwareDeviceDisplayCardComponent deviceDisplayCard;
    HardwareWiringDiagramComponent wiringDiagram;
    juce::Label lblRecipeCard;

    void rebuildDeviceTypes();
    void rebuildBrandsForCurrentType();
    void rebuildModelsForCurrentBrand();
    void rebuildObjectivesForCurrentModel();
    void syncVisualCards();
    void updateRecipeCard();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdHardwareCatalogSelector)
};

} // namespace abdaudiolab::gui
