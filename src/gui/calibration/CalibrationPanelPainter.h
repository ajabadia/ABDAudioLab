/**
 * @file CalibrationPanelPainter.h
 * @brief Side-effect free renderers for the Step 2 calibration card.
 *        Each painter receives a juce::Graphics plus an immutable ViewState and a
 *        rectangle cursor, so the whole paint pass is a pure function of panel state.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "CalibrationPanelViewState.h"

namespace abdaudiolab::gui::calibrationpanel::painter
{

//==============================================================================
// Shared primitives (DRY building blocks reused by every section).
//==============================================================================

/** @brief Inter font shorthand: one place to change the family/size conventions. */
inline juce::FontOptions font(float height, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return juce::FontOptions("Inter", height, style);
}

/** @brief Filled status pill / coloured caption used by headers, step cards and reports. */
void drawBadge(juce::Graphics& g,
               const juce::Rectangle<float>& target,
               const juce::String& label,
               const juce::Colour& background,
               const juce::Colour& foreground,
               float textHeight,
               juce::Justification justification,
               float cornerRadius);

/** @brief Filled + outlined rounded container shared by step cards, monitor and profile boxes. */
void drawCardShell(juce::Graphics& g,
                   const juce::Rectangle<float>& card,
                   juce::Colour background,
                   juce::Colour border,
                   float cornerRadius,
                   float borderThickness);

/** @brief Numbered instruction bullet; consumes the top of @p column. */
void drawInstructionStep(juce::Graphics& g,
                         juce::Rectangle<float>& column,
                         int number,
                         const juce::String& title,
                         const juce::String& description,
                         juce::Colour accent,
                         float rowHeight,
                         float descriptionFontHeight = 10.5f,
                         float gapAfter = 4.0f);

/** @brief Highlighted advice box; consumes the top of @p column. */
void drawTipBox(juce::Graphics& g,
                juce::Rectangle<float>& column,
                const juce::String& text,
                juce::Colour accent,
                juce::Font::FontStyleFlags style = juce::Font::bold,
                float borderAlpha = 0.35f);

//==============================================================================
// Section painters.
//==============================================================================

void paintHeaderRow(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> headerRow);
void paintStatusBadge(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> badgeRect);

void paintStepper(juce::Graphics& g,
                  const ViewState& view,
                  juce::Rectangle<float> stepperArea,
                  juce::Rectangle<int>& outCard2ABounds,
                  juce::Rectangle<int>& outCard2BBounds);

void paintDigitalIntro(juce::Graphics& g, juce::Rectangle<float>& content);
void paintDigitalInstructions(juce::Graphics& g, const ViewState& view, juce::Rectangle<float>& leftColumn);
void paintDigitalStatusCard(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> rightColumn);

void paintAnalogInstructions(juce::Graphics& g, const ViewState& view, juce::Rectangle<float>& leftColumn);

void paintLevelMonitor(juce::Graphics& g,
                       const ViewState& view,
                       juce::Rectangle<float> rightColumn,
                       juce::Rectangle<float>& outMeterArea);
void paintNoiseBaselineReport(juce::Graphics& g, const ViewState& view, juce::Rectangle<float>& meterArea);
void paintLoopbackReport(juce::Graphics& g, const ViewState& view, juce::Rectangle<float>& meterArea);
void paintSavedProfiles(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> rightColumn);

//==============================================================================
// Formatting helpers shared by several sections.
//==============================================================================

/** @brief Linear gain -> dB string with explicit sign, e.g. "+1.5 dB". */
juce::String formatTrimDb(float linearGain, int decimals);

/** @brief Level string that collapses digital silence into "-inf dBFS". */
juce::String formatDbfs(float dbfs, int decimals);

/** @brief True for the loopback states that mean "measurement failed". */
bool isLoopbackFailureState(LoopbackState state);

} // namespace abdaudiolab::gui::calibrationpanel::painter
