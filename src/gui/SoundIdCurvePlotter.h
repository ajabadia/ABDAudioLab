#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <mutex>
#include <memory>
#include "../export/LutExporter.h"
#include "../math/ModulationMatrixProfile.h"
#include "SoundIdTheme.h"
#include "LiveSpectrumAnalyzer.h"

namespace abdaudiolab::gui
{

class Waterfall3DComponent;
class PlotterModulationTableRenderer;
class PlotterHeatmapRenderer;
class PlotterFrequencyCurveRenderer;

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
        SpectrumFFT,
        PhaseGroupDelay,
        Waterfall3D,
        ModulationMatrix
    };

    SoundIdCurvePlotter();
    ~SoundIdCurvePlotter() override;

    void clear();
    void addMeasuredPoint(const exporting::MeasuredPoint& point);
    void setPoints(const std::vector<exporting::MeasuredPoint>& newPoints);
    void patchPoint(int index, const exporting::MeasuredPoint& point);
    void removePoint(int index);
    void setHighlightedPointIndex(int index);
    void setViewMode(ViewMode mode);
    void setPhaseData(const std::vector<float>& freqsHz,
                      const std::vector<float>& phaseRad,
                      const std::vector<float>& groupDelaySamples);

    void setPreScanTrajectory(const math::PreScanResult& preScan);
    void clearPreScanData();

    void setModulationProfile(const math::ModulationMatrixProfile& profile);
    void updateModulationNode(const math::ModulationNode& node);
    [[nodiscard]] const math::ModulationMatrixProfile& getModulationProfile() const noexcept { return currentModProfile; }

    void set3DIsometricOffsets(float xOffset, float yOffset) noexcept;
    [[nodiscard]] float get3DXOffset() const noexcept;
    [[nodiscard]] float get3DYOffset() const noexcept;

    void set3DZoomFactor(float zoom) noexcept;
    [[nodiscard]] float get3DZoomFactor() const noexcept;

    void reset3DCamera() noexcept;

    void setCollapsed(bool collapsed);
    void setChevronGlyph(const juce::String& glyph);
    [[nodiscard]] bool getIsCollapsed() const noexcept { return isCollapsed; }
    void updateTheme();

    std::function<void()> onToggleCollapse;

    /** @brief Access the spectrum analyzer for external data feeding. */
    LiveSpectrumAnalyzer& getSpectrumAnalyzer() noexcept { return spectrumAnalyzer; }

    void setMeasuringState(bool measuring, float progress = 0.0f);

    void setShowMeanCurve(bool show) noexcept;
    [[nodiscard]] bool getShowMeanCurve() const noexcept { return showMeanCurve; }

    void setShowSigmaBand(bool show) noexcept;
    [[nodiscard]] bool getShowSigmaBand() const noexcept { return showSigmaBand; }

    void setShowThdPoints(bool show) noexcept;
    [[nodiscard]] bool getShowThdPoints() const noexcept { return showThdPoints; }

    void setShowPhaseCurve(bool show) noexcept;
    [[nodiscard]] bool getShowPhaseCurve() const noexcept { return showPhaseCurve; }

    void setShowGroupDelayCurve(bool show) noexcept;
    [[nodiscard]] bool getShowGroupDelayCurve() const noexcept { return showGroupDelayCurve; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    ViewMode currentView { ViewMode::FrequencyCurve };
    std::vector<exporting::MeasuredPoint> points;
    int highlightedPointIndex { -1 };
    std::mutex pointsMutex;

    math::ModulationMatrixProfile currentModProfile;
    bool useThermalPalette { false };

    // 3D Mountains view component (Frente 8.5-D)
    std::unique_ptr<Waterfall3DComponent> waterfall3DView;

    // Modulation Matrix Inspector view component (Frente 8.5-E)
    std::unique_ptr<PlotterModulationTableRenderer> modulationTableView;

    // 2D Heatmap view component (Frente 8.5-F)
    std::unique_ptr<PlotterHeatmapRenderer> heatmapView;

    // 2D Frequency Curve view component (Frente 8.5-F)
    std::unique_ptr<PlotterFrequencyCurveRenderer> frequencyCurveView;

    bool isCollapsed { false };
    bool isMeasuring { false };
    float measuringProgress { 0.0f };
    juce::TextButton btnToggleCollapse;

    // Conmutable layer toggles (1.7.10)
    bool showMeanCurve { true };
    bool showSigmaBand { true };
    bool showThdPoints { true };
    juce::TextButton toggleMean;
    juce::TextButton toggleSigma;
    juce::TextButton toggleThd;

    // Phase & Group Delay controls
    bool showPhaseCurve { true };
    bool showGroupDelayCurve { true };
    juce::TextButton togglePhase;
    juce::TextButton toggleGroupDelay;
    juce::TextButton btnPhaseDelay { "Phase / GD" };

    std::vector<float> phaseFreqs;
    std::vector<float> phaseDataRad;
    std::vector<float> groupDelayDataSamples;

    void updateLegendToggleStyles();

    juce::Point<float> hoverMousePos { -1.0f, -1.0f };
    bool isHoveringPlot { false };
    int hoverPointIndex { -1 };
    juce::Rectangle<float> lastGridBounds;

    juce::TextButton btnCurve { "Curve" };
    juce::TextButton btnHeatmap { "2D Heatmap" };
    juce::TextButton btnSpectrum { "Spectrum" };

    LiveSpectrumAnalyzer spectrumAnalyzer;

    // PreScan Ghost Curve and 3D Waterfall data
    std::vector<math::PreScanPoint> preScanTrajectory;
    std::vector<int> preScanRoadmapSteps;
    bool hasPreScanData { false };
    bool showPreScanGhost { true };
    juce::TextButton btnWaterfall3D { "3D Mountains" };

    // Modulation Matrix Inspector tab & controls
    juce::TextButton btnModMatrix { "Mod Matrix" };
    juce::TextButton btnPaletteToggle { "Retro Emerald" };

    void drawPhaseGroupDelayPlot(juce::Graphics& g, juce::Rectangle<float> plotArea);
    void drawLegend(juce::Graphics& g, juce::Rectangle<float> legendArea);
};

} // namespace abdaudiolab::gui

