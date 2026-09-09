#include "TestPlanEstimationCardComponent.h"
#include "AppTheme.h"

namespace abdaudiolab::gui
{

TestPlanEstimationCardComponent::TestPlanEstimationCardComponent()
{
}

void TestPlanEstimationCardComponent::setEstimation(int totalPoints, float totalSeconds)
{
    points = totalPoints;
    seconds = totalSeconds;
    repaint();
}

void TestPlanEstimationCardComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(AppTheme::SurfaceSubtle);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(AppTheme::BorderSubtle);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto content = bounds.reduced(14.0f, 6.0f);

    // Green indicator pill badge
    auto badgeArea = content.removeFromLeft(90.0f);
    float badgeH = 18.0f;
    auto badgeRect = badgeArea.withSizeKeepingCentre(badgeArea.getWidth(), badgeH);
    g.setColour(AppTheme::AccentActive.withAlpha(0.15f));
    g.fillRoundedRectangle(badgeRect, badgeH * 0.5f);
    g.setFont(AppTheme::fontBold(9.5f));
    g.setColour(AppTheme::AccentActive);
    g.drawText("PLAN ESTIMATE", badgeRect, juce::Justification::centred, false);

    content.removeFromLeft(12.0f);

    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    juce::String durationStr = (mins > 0) ? (juce::String(mins) + "m " + juce::String(secs) + "s")
                                          : (juce::String(secs) + "s");

    juce::String mainText = juce::String(points) + " evaluation points total   |   Estimated Duration: ~" + durationStr;

    g.setFont(AppTheme::fontBold(11.0f));
    g.setColour(AppTheme::TextPrimary);
    g.drawText(mainText, content, juce::Justification::centredLeft, true);
}

} // namespace abdaudiolab::gui
