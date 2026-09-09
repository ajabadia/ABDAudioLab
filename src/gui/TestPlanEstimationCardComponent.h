#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @class TestPlanEstimationCardComponent
 * @brief Standalone UI card that computes and displays test plan evaluation points and estimated duration.
 */
class TestPlanEstimationCardComponent : public juce::Component
{
public:
    TestPlanEstimationCardComponent();
    ~TestPlanEstimationCardComponent() override = default;

    void setEstimation(int totalPoints, float totalSeconds);
    void paint(juce::Graphics& g) override;

    [[nodiscard]] int getPoints() const noexcept { return points; }
    [[nodiscard]] float getSeconds() const noexcept { return seconds; }

private:
    int points { 0 };
    float seconds { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestPlanEstimationCardComponent)
};

} // namespace abdaudiolab::gui
