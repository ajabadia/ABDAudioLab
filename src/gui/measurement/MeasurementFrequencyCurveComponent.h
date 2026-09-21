/**
 * @file MeasurementFrequencyCurveComponent.h
 * @brief Autonomous vector renderer for frequency response curves (log Hz vs dB).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../measurement/MeasurementContracts.h"
#include <optional>

namespace abdaudiolab::gui::measurement
{

class MeasurementFrequencyCurveComponent : public juce::Component
{
public:
    MeasurementFrequencyCurveComponent();
    ~MeasurementFrequencyCurveComponent() override = default;

    void setCurve(const abdaudiolab::measurement::MeasurementCurve& curve,
                  const std::optional<abdaudiolab::measurement::SlopeFitMetadata>& slopeFit,
                  double cutoffHz,
                  bool isCutoffObservable,
                  bool isIntegrityVerified);

    void clear();
    void updateTheme();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    [[nodiscard]] bool isCutoffObservable() const noexcept { return isCutoffObservable_; }
    [[nodiscard]] double getCutoffHz() const noexcept { return cutoffHz_; }

private:
    abdaudiolab::measurement::MeasurementCurve curve_;
    std::optional<abdaudiolab::measurement::SlopeFitMetadata> slopeFit_;
    double cutoffHz_ { -1.0 };
    bool isCutoffObservable_ { false };
    bool isIntegrityVerified_ { true };

    juce::Rectangle<float> plotBounds_;
    juce::Point<float> mousePos_ { -1.0f, -1.0f };
    bool isHovering_ { false };

    const double minFreq_ { 20.0 };
    const double maxFreq_ { 20000.0 };
    const double minDb_ { -72.0 };
    const double maxDb_ { 12.0 };

    float freqToX(double f) const noexcept;
    float dbToY(double db) const noexcept;
    double xToFreq(float x) const noexcept;
    double yToDb(float y) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementFrequencyCurveComponent)
};

} // namespace abdaudiolab::gui::measurement
