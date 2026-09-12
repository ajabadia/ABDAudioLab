#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/HardwareContractRegistry.h"
#include "../hardware/MidiDeviceHotplugMonitor.h"
#include "HardwareWiringDiagramComponent.h"
#include "HardwareDeviceDisplayCardComponent.h"

namespace abdaudiolab::gui
{

class HardwareRoutingPanel : public juce::Component
{
public:
    HardwareRoutingPanel();
    ~HardwareRoutingPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    void setContracts(const std::vector<core::HardwareContract>& contracts);
    void setSelectedHardware(const juce::String& hwId, const juce::String& funcId = {});
    void setAutoDetectButtonText(const juce::String& text);

    [[nodiscard]] juce::String getSelectedHardwareId() const;
    [[nodiscard]] juce::String getSelectedFunctionId() const;

    void setHardwareLocked(bool locked);
    [[nodiscard]] bool isHardwareLockedActive() const noexcept { return isHardwareLocked; }
    void resetSelection();
    void setPluginVirtualRouting(const juce::String& pluginName, const juce::String& format, bool isInstrument);

    [[nodiscard]] const HardwareWiringDiagramComponent& getWiringDiagram() const noexcept { return wiringDiagram; }
    [[nodiscard]] const HardwareDeviceDisplayCardComponent& getDeviceDisplayCard() const noexcept { return deviceDisplayCard; }

    std::function<void(const juce::String& hwId, const juce::String& funcId)> onHardwareSelected;
    std::function<void()> onContinueToCalibration;
    std::function<void()> onOpenAdvancedSettings;
    std::function<void()> onOpenTopologyModal;
    std::function<void()> onAutoDetectRequested;
    std::function<void()> onNewFlowRequested;

private:
    void updateFunctionsCombo();
    void updateRoutingDisplay();
    void updateBrandAndModelGraphics();

    std::vector<core::HardwareContract> contractsList;

    bool isHardwareLocked { false };
    juce::Label lblHardwareLockedBanner;
    juce::TextButton btnChangeHwOrNewFlow;

    juce::TextButton btnAutoDetect { "Auto-Detect Device (MIDI / USB)" };
    juce::ComboBox hwDeviceCombo;
    juce::ComboBox hwFunctionCombo;

    juce::TextButton btnContinue;
    juce::TextButton btnAdvanced;
    juce::TextButton btnOpenTopology { "Studio Connection Map" };

    // Componentes gráficos desacoplados
    HardwareWiringDiagramComponent wiringDiagram;
    HardwareDeviceDisplayCardComponent deviceDisplayCard;

    hardware::MidiDeviceHotplugMonitor hotplugMonitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwareRoutingPanel)
};

} // namespace abdaudiolab::gui
