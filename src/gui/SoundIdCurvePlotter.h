#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <mutex>
#include "../export/LutExporter.h"
#include "SoundIdTheme.h"
#include "LiveSpectrumAnalyzer.h"

namespace abdaudiolab::gui
{

/**
 * @brief High-precision SoundID-style multi-mode visualizer.
 *
 * Tabs: Curve (μ ± σ) | 2D Heatmap | Spectrum FFT
 */
class SoundIdCurvePlotter : public juce::Component
{
public:
    enum class ViewMode
    {
        FrequencyCurve,
        Heatmap2D,
        SpectrumFFT
    };

    SoundIdCurvePlotter();
    ~SoundIdCurvePlotter() override = default;

    void clear();
    void addMeasuredPoint(const exporting::MeasuredPoint& point);
    void setPoints(const std::vector<exporting::MeasuredPoint>& newPoints);
    void removePoint(int index);
    void setHighlightedPointIndex(int index);
    void setViewMode(ViewMode mode);

    /** @brief Access the spectrum analyzer for external data feeding. */
    LiveSpectrumAnalyzer& getSpectrumAnalyzer() noexcept { return spectrumAnalyzer; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ViewMode currentView { ViewMode::FrequencyCurve };
    std::vector<exporting::MeasuredPoint> points;
    int highlightedPointIndex { -1 };
    std::mutex pointsMutex;

    juce::TextButton btnCurve { "Curve" };
    juce::TextButton btnHeatmap { "2D Heatmap" };
    juce::TextButton btnSpectrum { "Spectrum" };

    LiveSpectrumAnalyzer spectrumAnalyzer;

    void drawFrequencyPlot(juce::Graphics& g, juce::Rectangle<float> plotArea);
    void drawHeatmap2D(juce::Graphics& g, juce::Rectangle<float> plotArea);
    void drawLegend(juce::Graphics& g, juce::Rectangle<float> legendArea);

    // Viridis color map (256 entries, interpolated)
    static juce::Colour viridisColor(float normalized0to1) noexcept;
};

} // namespace abdaudiolab::gui
