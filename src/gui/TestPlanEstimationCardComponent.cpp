#include "TestPlanEstimationCardComponent.h"
#include "AppTheme.h"
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

TestPlanEstimationCardComponent::TestPlanEstimationCardComponent()
{
}

void TestPlanEstimationCardComponent::setEstimation(int totalPoints, float totalSeconds)
{
    currentEstimate.measurementStates = totalPoints;
    currentEstimate.audioCaptureSeconds = totalSeconds;
    currentEstimate.estimatedTotalSeconds = totalSeconds;
    currentEstimate.isManualProfiling = false;
    currentEstimate.manualControlAdjustmentEvents = 0;
    currentEstimate.dimensionalFormula = juce::String(totalPoints) + " states";
    repaint();
}

void TestPlanEstimationCardComponent::setEstimation(const ProfilingTimeEstimate& estimate)
{
    currentEstimate = estimate;
    repaint();
}

void TestPlanEstimationCardComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto content = bounds.reduced(14.0f, 6.0f);

    int mins = static_cast<int>(currentEstimate.estimatedTotalSeconds) / 60;
    int secs = static_cast<int>(currentEstimate.estimatedTotalSeconds) % 60;
    juce::String durationStr = (mins > 0) ? (juce::String(mins) + "m " + juce::String(secs) + "s")
                                          : (juce::String(secs) + "s");

    // Single-line vs Two-line layout based on component height
    if (getHeight() <= 48)
    {
        // Compact single-line view
        auto badgeArea = content.removeFromLeft(92.0f);
        float badgeH = 18.0f;
        auto badgeRect = badgeArea.withSizeKeepingCentre(badgeArea.getWidth(), badgeH);
        g.setColour(AppTheme::AccentActive.withAlpha(0.15f));
        g.fillRoundedRectangle(badgeRect, badgeH * 0.5f);
        g.setFont(AppTheme::fontBold(9.5f));
        g.setColour(AppTheme::AccentActive);
        g.drawText("PLAN ESTIMATE", badgeRect, juce::Justification::centred, false);

        content.removeFromLeft(10.0f);

        juce::String mainText = juce::String(currentEstimate.measurementStates) + " states ("
                              + currentEstimate.dimensionalFormula + ")   |   "
                              + (currentEstimate.isManualProfiling
                                 ? ("Manual \u2248 " + juce::String(currentEstimate.manualControlAdjustmentEvents) + " adjustments   |   ")
                                 : "Automated   |   ")
                              + "Estimated Time: ~" + durationStr;

        g.setFont(AppTheme::fontBold(10.5f));
        g.setColour(AppTheme::TextPrimary);
        g.drawFittedText(mainText, content.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    }
    else
    {
        // Rich two-tier view
        auto topRow = content.removeFromTop(content.getHeight() * 0.5f);
        auto bottomRow = content;

        // Top Row: Badge + States + Formula
        auto badgeArea = topRow.removeFromLeft(92.0f);
        float badgeH = 18.0f;
        auto badgeRect = badgeArea.withSizeKeepingCentre(badgeArea.getWidth(), badgeH);
        g.setColour(AppTheme::AccentActive.withAlpha(0.15f));
        g.fillRoundedRectangle(badgeRect, badgeH * 0.5f);
        g.setFont(AppTheme::fontBold(9.5f));
        g.setColour(AppTheme::AccentActive);
        g.drawText("PLAN ESTIMATE", badgeRect, juce::Justification::centred, false);

        topRow.removeFromLeft(10.0f);

        juce::String statesText = juce::String(currentEstimate.measurementStates) + " measurement states";
        if (currentEstimate.dimensionalFormula.isNotEmpty())
            statesText += " (" + currentEstimate.dimensionalFormula + ")";

        g.setFont(AppTheme::fontBold(11.5f));
        g.setColour(AppTheme::TextPrimary);
        g.drawFittedText(statesText, topRow.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);

        // Bottom Row: Mode & Adjustments + Estimated Duration
        juce::String modeText;
        if (currentEstimate.isManualProfiling)
        {
            modeText = juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93")) + " Manual profiling \u2022 "
                     + juce::String(currentEstimate.manualControlAdjustmentEvents) + " control adjustments";
        }
        else
        {
            modeText = juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93 Fully automated profiling. No manual adjustments required."));
        }

        auto modeArea = bottomRow.removeFromLeft(bottomRow.getWidth() - 170.0f);
        g.setFont(AppTheme::fontRegular(10.5f));
        g.setColour(currentEstimate.isManualProfiling ? SoundIdTheme::accentBlue : AppTheme::TextSecondary);
        g.drawText(modeText, modeArea, juce::Justification::centredLeft, true);

        g.setFont(AppTheme::fontBold(11.0f));
        g.setColour(AppTheme::TextPrimary);
        g.drawText("Estimated Time: ~" + durationStr, bottomRow, juce::Justification::centredRight, true);
    }
}

} // namespace abdaudiolab::gui
