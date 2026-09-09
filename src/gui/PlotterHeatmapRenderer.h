#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "../export/LutExporter.h"

namespace abdaudiolab::gui
{

/**
 * @brief Autonomous 2D parameter excitation grid renderer with Viridis / Plasma colormap.
 */
class PlotterHeatmapRenderer : public juce::Component
{
public:
    PlotterHeatmapRenderer();
    ~PlotterHeatmapRenderer() override = default;

    void paint(juce::Graphics& g) override;

    void setPoints(const std::vector<exporting::MeasuredPoint>& newPoints);
    void clear();

    static juce::Colour viridisColor(float normalized0to1) noexcept;

private:
    std::vector<exporting::MeasuredPoint> points;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlotterHeatmapRenderer)
};

} // namespace abdaudiolab::gui
