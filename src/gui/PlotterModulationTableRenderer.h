#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../math/ModulationMatrixProfile.h"

namespace abdaudiolab::gui
{

/**
 * @brief Autonomous specialized renderer component for the sparse Modulation Matrix table.
 *
 * Displays measured nodes (Source ID, Dest ID, Gain K, Offset c, Linearity R^2)
 * with color-coded badges, alternating row backgrounds, and fit quality bars.
 */
class PlotterModulationTableRenderer : public juce::Component
{
public:
    PlotterModulationTableRenderer();
    ~PlotterModulationTableRenderer() override = default;

    void paint(juce::Graphics& g) override;

    void setProfile(const math::ModulationMatrixProfile& profile);
    void updateNode(const math::ModulationNode& node);
    void clear();

    [[nodiscard]] const math::ModulationMatrixProfile& getProfile() const noexcept { return currentProfile; }

private:
    math::ModulationMatrixProfile currentProfile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlotterModulationTableRenderer)
};

} // namespace abdaudiolab::gui
