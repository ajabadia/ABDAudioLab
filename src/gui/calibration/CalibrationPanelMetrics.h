/**
 * @file CalibrationPanelMetrics.h
 * @brief Single source of truth for the Step 2 card geometry.
 *        The paint pass, the layout pass and the painters MUST agree on these values,
 *        so they live here instead of being repeated as magic numbers per translation unit.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

namespace abdaudiolab::gui::calibrationpanel
{

// Main card frame.
inline constexpr float kCardMaxWidth { 860.0f };
inline constexpr float kCardMaxHeight { 580.0f };
inline constexpr float kCardMarginX { 40.0f };
inline constexpr float kCardMarginY { 30.0f };
inline constexpr float kCardCornerRadius { 12.0f };
inline constexpr float kInnerCardCornerRadius { 8.0f }; // step cards, monitor and profile boxes
inline constexpr float kCardPaddingX { 28.0f };
inline constexpr float kCardPaddingY { 24.0f };

// Header row, divider and 2A/2B stepper.
inline constexpr float kHeaderHeight { 32.0f };
inline constexpr float kHeaderGap { 10.0f };
inline constexpr float kDividerHeight { 1.0f };
inline constexpr float kStepperHeight { 62.0f };
inline constexpr float kStepperGapY { 12.0f };
inline constexpr float kStepperGapX { 14.0f };

// Body columns and bottom action row.
inline constexpr float kColumnSplitRatio { 0.52f };
inline constexpr float kColumnGap { 20.0f };
inline constexpr float kBottomRowOffset { 56.0f };

// Right column: live monitor card and the persistent 2A / 2B report inside it.
inline constexpr float kMonitorHeight { 320.0f };

// Saved profiles box (optional section below the monitor).
inline constexpr float kSavedProfilesTopOffset { 226.0f };
inline constexpr float kSavedProfilesMaxHeight { 140.0f };
inline constexpr float kSavedProfilesTailSpace { 65.0f };
inline constexpr float kSavedProfilesSideInset { 48.0f };

// Action row of the saved-profiles box. The vertical block is the tuned baseline that the
// delete / details buttons were placed against, so it is expressed once and kept verbatim.
inline constexpr float kSavedProfilesActionRowTop
{
    kCardPaddingY + kHeaderHeight + 44.0f + kHeaderGap + kSavedProfilesTopOffset + kSavedProfilesMaxHeight
};
inline constexpr float kSavedProfilesActionRowInset { 30.0f };

// Integer views of the same metrics, for the layout pass which stays on whole pixels.
inline constexpr int kCardPaddingXPx { static_cast<int>(kCardPaddingX) };
inline constexpr int kStepperHeightPx { static_cast<int>(kStepperHeight) };
inline constexpr int kStepperGapXPx { static_cast<int>(kStepperGapX) };
inline constexpr int kBottomRowOffsetPx { static_cast<int>(kBottomRowOffset) };
inline constexpr int kSavedProfilesSideInsetPx { static_cast<int>(kSavedProfilesSideInset) };
inline constexpr int kSavedProfilesActionRowInsetPx { static_cast<int>(kSavedProfilesActionRowInset) };
inline constexpr int kSavedProfilesActionRowTopPx { static_cast<int>(kSavedProfilesActionRowTop) };

} // namespace abdaudiolab::gui::calibrationpanel
