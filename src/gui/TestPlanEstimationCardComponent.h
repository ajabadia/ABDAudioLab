#pragma once

#include "TestConfiguration.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @class TestPlanEstimationCardComponent
 * @brief Standalone UI card that computes and displays test plan evaluation points,
 * dimensional matrix breakdown, physical control adjustments, and estimated duration.
 */
class TestPlanEstimationCardComponent : public juce::Component
{
public:
    TestPlanEstimationCardComponent();
    ~TestPlanEstimationCardComponent() override = default;

    void setEstimation(int totalPoints, float totalSeconds);
    void setEstimation(const ProfilingTimeEstimate& estimate);
    void paint(juce::Graphics& g) override;

    [[nodiscard]] int getPoints() const noexcept { return currentEstimate.measurementStates; }
    [[nodiscard]] float getSeconds() const noexcept { return currentEstimate.estimatedTotalSeconds; }
    [[nodiscard]] const ProfilingTimeEstimate& getEstimate() const noexcept { return currentEstimate; }

private:
    ProfilingTimeEstimate currentEstimate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestPlanEstimationCardComponent)
};

} // namespace abdaudiolab::gui
