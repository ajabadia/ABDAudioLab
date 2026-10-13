/**
 * @file CalibrationPanelViewState.h
 * @brief Immutable snapshot of everything the Step 2 painters need to render one frame.
 *        Keeping the painters free of the juce::Component makes them side-effect free,
 *        so a paint pass can never mutate calibration state by accident.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "CalibrationPanelTypes.h"
#include "../../calibration/CalibrationProfileStore.h"

#include <vector>

namespace abdaudiolab::gui::calibrationpanel
{

/** @brief Read-only projection of NativeCalibrationPanel used by every painter. */
struct ViewState
{
    State state { State::ReadyToMeasure };
    SubView subView { SubView::NoiseBaseline_2A };
    NoiseBaselineState noiseBaselineState { NoiseBaselineState::NotChecked };
    LoopbackState loopbackState { LoopbackState::Locked };
    NoiseBaselineReport noiseReport;
    LoopbackReport loopbackReport;
    CalibrationDiagnostics diagnostics;

    juce::String outputChannelName;
    juce::String inputChannelName;

    float liveInputPeak { 0.0f };

    bool digitalMode { false };
    bool digitalVerified { false };

    bool showSavedProfiles { false };
    bool showProfileDetails { false };
    int selectedProfileIndex { 0 };
    juce::String saveFeedbackText;
    const std::vector<calibration::CalibrationRecord>* savedProfiles { nullptr };
};

} // namespace abdaudiolab::gui::calibrationpanel
