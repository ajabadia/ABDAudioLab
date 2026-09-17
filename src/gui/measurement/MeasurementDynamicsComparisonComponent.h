/**
 * @file MeasurementDynamicsComparisonComponent.h
 * @brief Multi-series accessible dynamics curve comparison component with geometric markers and line patterns.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MeasurementComparisonSession.h"

namespace abdaudiolab::gui::measurement
{

enum class DynamicsComparisonMode
{
    LevelDbfs,
    TimbreCentroidHz,
    TimbreRolloffHz
};

class MeasurementDynamicsComparisonComponent : public juce::Component,
                                              public MeasurementComparisonSession::Listener
{
public:
    explicit MeasurementDynamicsComparisonComponent(MeasurementComparisonSession& session);
    ~MeasurementDynamicsComparisonComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;

    // MeasurementComparisonSession::Listener interface
    void containerStateChanged(int containerId, ContainerLoadState newState) override;
    void containerListChanged() override;
    void domainFilterChanged() override;

    void setComparisonMode(DynamicsComparisonMode mode);
    [[nodiscard]] DynamicsComparisonMode getComparisonMode() const noexcept { return mode_; }

private:
    struct SeriesRenderData
    {
        LoadedContainerEntry entry;
        std::vector<std::pair<double, double>> points; // X (velocity 0..127), Y (dBFS or Hz)
        juce::Path path;
        bool isCompatible { true };
        juce::String incompatibilityReason;
    };

    void rebuildCurves();
    void renderGridAndAxes(juce::Graphics& g, juce::Rectangle<float> plotArea, double minY, double maxY);
    void renderSeries(juce::Graphics& g, const SeriesRenderData& series, juce::Rectangle<float> plotArea, double minY, double maxY);
    void renderLegend(juce::Graphics& g, juce::Rectangle<float> legendArea);
    void renderHoverTooltip(juce::Graphics& g);

    MeasurementComparisonSession& session_;
    DynamicsComparisonMode mode_ { DynamicsComparisonMode::LevelDbfs };
    std::vector<SeriesRenderData> seriesList_;

    // Mode Selector buttons
    juce::TextButton btnModeLevel_ { "Nivel (dBFS)" };
    juce::TextButton btnModeCentroid_ { "Centroide (Hz)" };
    juce::TextButton btnModeRolloff_ { "Rolloff (Hz)" };

    // Hover tooltip tracking
    juce::Point<float> hoverPos_;
    bool isHovering_ { false };
    juce::String hoverTooltipText_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementDynamicsComparisonComponent)
};

} // namespace abdaudiolab::gui::measurement
