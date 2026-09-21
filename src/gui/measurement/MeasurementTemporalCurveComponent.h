/**
 * @file MeasurementTemporalCurveComponent.h
 * @brief Autonomous vector renderer for temporal amplitude curves (ms vs dBFS).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../measurement/MeasurementContracts.h"

namespace abdaudiolab::gui::measurement
{

class MeasurementTemporalCurveComponent : public juce::Component
{
public:
    MeasurementTemporalCurveComponent();
    ~MeasurementTemporalCurveComponent() override = default;

    void setCurve(const abdaudiolab::measurement::MeasurementCurve& curve, bool isIntegrityVerified);
    void clear();
    void updateTheme();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    abdaudiolab::measurement::MeasurementCurve curve_;
    bool isIntegrityVerified_ { true };

    juce::Rectangle<float> plotBounds_;
    juce::Point<float> mousePos_ { -1.0f, -1.0f };
    bool isHovering_ { false };

    double minTimeMs_ { 0.0 };
    double maxTimeMs_ { 100.0 };
    double minDb_ { -96.0 };
    double maxDb_ { 0.0 };

    void recalculateBounds();
};

} // namespace abdaudiolab::gui::measurement
