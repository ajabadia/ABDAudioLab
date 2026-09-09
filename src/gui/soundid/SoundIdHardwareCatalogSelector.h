#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
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
 * - Non-destructive: Co-exists seamlessly alongside HardwareRoutingPanel.
 */
class SoundIdHardwareCatalogSelector : public juce::Component
{
public:
    SoundIdHardwareCatalogSelector();
    ~SoundIdHardwareCatalogSelector() override = default;

    void setContracts(const std::vector<core::HardwareContract>& contracts);
    void setSelectedHardware(const juce::String& hwId, const juce::String& funcId = {});
    
    [[nodiscard]] juce::String getSelectedHardwareId() const;
    [[nodiscard]] juce::String getSelectedFunctionId() const;
    [[nodiscard]] juce::String getSelectedDeviceType() const;
    [[nodiscard]] juce::String getSelectedBrand() const;
    [[nodiscard]] bool isCustomOrLibre() const noexcept { return isLibreMode; }

    void setHardwareLocked(bool locked);
    [[nodiscard]] bool isLocked() const noexcept { return isHardwareLocked; }

    // Callbacks
    std::function<void(const juce::String& hwId, const juce::String& funcId)> onSelectionChanged;
    std::function<void()> onContinueRequested;
    std::function<void()> onAutoDetectRequested;
    std::function<void()> onResetOrUnlockRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    std::vector<core::HardwareContract> contractsList;
    bool isHardwareLocked { false };
    bool isLibreMode { false };

    // Cascading Dropdowns
    juce::ComboBox comboDeviceType;
    juce::ComboBox comboBrand;
    juce::ComboBox comboModel;
    juce::ComboBox comboObjective;

    // Header & Buttons
    juce::TextButton btnAutoDetect { "Auto-Detect Device (MIDI / USB)" };
    juce::TextButton btnLibreMode { "Custom / Modo Libre" };
    juce::TextButton btnContinue { "Continuar a Calibracion ➔" };
    juce::TextButton btnUnlock { "Cambiar Hardware / Desbloquear" };
    juce::Label lblLockedBanner;

    // Visual cards
    HardwareDeviceDisplayCardComponent deviceDisplayCard;
    HardwareWiringDiagramComponent wiringDiagram;

    void rebuildDeviceTypes();
    void rebuildBrandsForCurrentType();
    void rebuildModelsForCurrentBrand();
    void rebuildObjectivesForCurrentModel();
    void syncVisualCards();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdHardwareCatalogSelector)
};

} // namespace abdaudiolab::gui
