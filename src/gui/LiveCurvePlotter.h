#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <mutex>
#include "../export/LutExporter.h"

namespace abdaudiolab::gui
{

/**
 * @brief Real-time interactive curve plotter and multi-mode visualizer.
 * 
 * Implements Section 18.2 (RF-35, RF-36):
 * - Renders dynamic mean (mu) curve with shaded +/- sigma dispersion band.
 * - Displays 2D parameter heatmaps and spectral profiles.
 */
class LiveCurvePlotter : public juce::Component
{
public:
    enum class DisplayMode
    {
        TransferCurve,
        Heatmap2D,
        SpectrumOverlay
    };

    LiveCurvePlotter();
    ~LiveCurvePlotter() override = default;

    void clear();
    void addMeasuredPoint(const exporting::MeasuredPoint& point);
    void setDisplayMode(DisplayMode mode);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    DisplayMode currentMode { DisplayMode::TransferCurve };
    std::vector<exporting::MeasuredPoint> points;
    std::mutex pointsMutex;

    juce::TextButton btnCurve { "Transfer Curve (mu/sigma)" };
    juce::TextButton btnHeatmap { "2D Heatmap" };

    void drawTransferCurve(juce::Graphics& g, juce::Rectangle<float> plotArea);
    void drawHeatmap2D(juce::Graphics& g, juce::Rectangle<float> plotArea);
};

} // namespace abdaudiolab::gui
