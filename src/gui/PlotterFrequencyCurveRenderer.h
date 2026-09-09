#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "../export/LutExporter.h"
#include "../math/LabAnalyticEngine.h"

namespace abdaudiolab::gui
{

/**
 * @brief Autonomous 2D frequency curve & statistical distribution renderer.
 *
 * Renders nominal mean response curve (μ), confidence band (±σ), measured THD% nodes,
 * ballistic sweep trace beam, adaptive roadmap markers, and interactive crosshair tooltip.
 */
class PlotterFrequencyCurveRenderer : public juce::Component
{
public:
    PlotterFrequencyCurveRenderer();
    ~PlotterFrequencyCurveRenderer() override = default;

    void paint(juce::Graphics& g) override;

    void setPoints(const std::vector<exporting::MeasuredPoint>& newPoints);
    void clear();
    void setHighlightedPointIndex(int index);

    // Layer toggles
    void setShowMeanCurve(bool show) noexcept;
    void setShowSigmaBand(bool show) noexcept;
    void setShowThdPoints(bool show) noexcept;

    // Measurement beam
    void setMeasuringProgress(bool measuring, float progress) noexcept;

    // PreScan ghost curve
    void setPreScanData(const std::vector<math::PreScanPoint>& trajectory,
                        const std::vector<int>& roadmapSteps,
                        bool showGhost) noexcept;
    void clearPreScanData() noexcept;

    // Mouse interaction for crosshair/tooltip
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void setHoverPosition(juce::Point<float> pos);
    void clearHover();

    [[nodiscard]] juce::Rectangle<float> getGridBounds() const noexcept { return lastGridBounds; }

private:
    std::vector<exporting::MeasuredPoint> points;
    int highlightedPointIndex { -1 };

    bool showMeanCurve { true };
    bool showSigmaBand { true };
    bool showThdPoints { true };

    bool isMeasuring { false };
    float measuringProgress { 0.0f };

    std::vector<math::PreScanPoint> preScanTrajectory;
    std::vector<int> preScanRoadmapSteps;
    bool hasPreScanData { false };
    bool showPreScanGhost { true };

    juce::Point<float> hoverMousePos { -1.0f, -1.0f };
    bool isHoveringPlot { false };
    int hoverPointIndex { -1 };
    juce::Rectangle<float> lastGridBounds;

    void drawCrosshairAndTooltip(juce::Graphics& g, juce::Rectangle<float> gridBounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlotterFrequencyCurveRenderer)
};

} // namespace abdaudiolab::gui
