#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../core/HardwareContractRegistry.h"

namespace abdaudiolab::gui
{

class HardwareDeviceDisplayCardComponent : public juce::Component
{
public:
    HardwareDeviceDisplayCardComponent();
    ~HardwareDeviceDisplayCardComponent() override = default;

    void paint(juce::Graphics& g) override;

    void setDevice(const core::HardwareContract* contract);

    [[nodiscard]] const juce::String& getDisplayName() const noexcept { return currentHwDisplayName; }
    [[nodiscard]] const juce::String& getBrand() const noexcept { return currentHwBrand; }
    [[nodiscard]] const juce::String& getCategory() const noexcept { return currentHwCategory; }
    [[nodiscard]] bool hasModelImage() const noexcept { return modelSvgDrawable != nullptr || modelRasterImage.isValid(); }
    [[nodiscard]] bool hasBrandLogo() const noexcept { return brandLogoDrawable != nullptr; }

private:
    std::unique_ptr<juce::Drawable> modelSvgDrawable;
    juce::Image modelRasterImage;
    std::unique_ptr<juce::Drawable> brandLogoDrawable;
    juce::String currentHwBrand;
    juce::String currentHwDisplayName;
    juce::String currentHwCategory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwareDeviceDisplayCardComponent)
};

} // namespace abdaudiolab::gui
