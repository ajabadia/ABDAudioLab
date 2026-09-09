/**
 * @file DrawerHardwareTab.h
 * @brief Drawer tab for Hardware device inspection, submodule routing, and MIDI auto-detection.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "../SoundIdTheme.h"
#include "../../core/HardwareContractRegistry.h"
#include "AssetLocator.h"
#include "DrawerDataModels.h"
#include "HardwareMidiDetect/MidiHardwareBackend.h"

namespace abdaudiolab::gui
{

class DrawerHardwareTab : public juce::Component
{
public:
    DrawerHardwareTab();
    ~DrawerHardwareTab() override;

    void setHardwareList(const std::vector<HardwareItem>& list);
    void setContracts(std::vector<core::HardwareContract> contractsList);
    void setSelectedHardwareId(const juce::String& id);
    void clearSelectedHardware();
    void setHardwareLocked(bool locked);
    void triggerAutoDetect();
    void preWarmHardwarePicker();

    [[nodiscard]] bool getHardwareLocked() const noexcept { return isHardwareLocked; }
    [[nodiscard]] juce::String getSelectedHardwareId() const;
    [[nodiscard]] juce::String getSelectedFunctionId() const;
    [[nodiscard]] juce::String getActiveHardwareDisplayName() const;
    [[nodiscard]] juce::String getActiveFunctionDisplayName() const;
    [[nodiscard]] const juce::Image& getActiveModelRasterImage() const noexcept { return modelRasterImage; }
    [[nodiscard]] const juce::Drawable* getModelSvgDrawable() const noexcept { return modelSvgDrawable.get(); }
    [[nodiscard]] const juce::Drawable* getBrandLogoDrawable() const noexcept { return brandLogoDrawable.get(); }
    [[nodiscard]] const juce::String& getCurrentHwBrand() const noexcept { return currentHwBrand; }
    [[nodiscard]] float getBurstDurationSeconds() const;
    [[nodiscard]] bool isAdaptiveEnvelopeMode() const;
    [[nodiscard]] int getSelectedHardwareModeIndex() const { return hwModeCombo.getSelectedId(); }
    [[nodiscard]] int getPreferredHeight() const noexcept;

    void updateTheme();
    void updateBrandAndModelGraphics();
    void updateFunctionSelectionUI(const HardwareItem& item);

    // Callbacks
    std::function<void(const juce::String& hwId, const juce::String& funcId)> onHardwareSelected;
    std::function<void(const juce::String& displayName)> onDeviceDetected;
    std::function<void()> onNewFlowRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

    class ImageDisplayComponent : public juce::Component
    {
    public:
        explicit ImageDisplayComponent(DrawerHardwareTab& ownerRef) : owner(ownerRef) {}
        void paint(juce::Graphics& g) override;
    private:
        DrawerHardwareTab& owner;
    };

    class WiringGuideCard : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        juce::String stimulusText;
        juce::String responseText;
    };

private:
    void openHardwarePickerModal();

    std::vector<HardwareItem> hardwareList;
    std::vector<core::HardwareContract> availableContracts;

    std::unique_ptr<juce::Drawable> brandLogoDrawable;
    std::unique_ptr<juce::Drawable> modelSvgDrawable;
    juce::Image modelRasterImage;
    juce::String currentHwBrand;

    ImageDisplayComponent imgDisplay { *this };

    bool isHardwareLocked { false };
    juce::Label lblHardwareLockedBanner;
    juce::TextButton btnChangeHwOrNewFlow { "Cambiar Hardware / Iniciar Nuevo Flujo" };
    juce::Label lblHwTitle;
    juce::Label lblSelectHw;
    juce::ComboBox hwModeCombo;
    juce::Label lblSelectFunc;
    juce::ComboBox hwFunctionCombo;

    WiringGuideCard cardWiring;
    juce::Label lblAutoDetectSection;
    juce::TextButton btnAutoDetect { "Auto-Detect Device (MIDI / USB)" };
    juce::Label lblOrSeparator;

    std::unique_ptr<abd::hwid::MidiHardwareBackend> midiBackend;
    std::unique_ptr<juce::DocumentWindow> pickerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawerHardwareTab)
};

} // namespace abdaudiolab::gui
