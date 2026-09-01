#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <mutex>
#include "../export/LutExporter.h"
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

/**
 * @brief High-precision SoundID-style frequency response & parameter curve plotter.
 */
class SoundIdCurvePlotter : public juce::Component
{
public:
    enum class ViewMode
    {
        FrequencyCurve,
        Heatmap2D
    };

    SoundIdCurvePlotter();
    ~SoundIdCurvePlotter() override = default;

    void clear();
    void addMeasuredPoint(const exporting::MeasuredPoint& point);
    void setViewMode(ViewMode mode);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ViewMode currentView { ViewMode::FrequencyCurve };
    std::vector<exporting::MeasuredPoint> points;
    std::mutex pointsMutex;

    juce::TextButton btnCurve { "Curve (μ ± σ)" };
    juce::TextButton btnHeatmap { "2D Heatmap" };

    void drawFrequencyPlot(juce::Graphics& g, juce::Rectangle<float> plotArea);
    void drawHeatmap2D(juce::Graphics& g, juce::Rectangle<float> plotArea);
    void drawLegend(juce::Graphics& g, juce::Rectangle<float> legendArea);
};

} // namespace abdaudiolab::gui
