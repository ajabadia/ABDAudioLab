#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

/**
 * @brief Compact horizontal measurement health status bar.
 *
 * Displays real-time SNR, confidence level, noise floor,
 * and measurement progress in a slim ~28px strip.
 */
class MeasurementHealthPanel : public juce::Component
{
public:
    MeasurementHealthPanel()
    {
        setInterceptsMouseClicks(false, false);
    }
    ~MeasurementHealthPanel() override = default;

    void setMeasurementHealth(float snrDb, float noiseFloorDb, int pointsDone, int pointsTotal)
    {
        currentSnrDb = snrDb;
        currentNoiseFloorDb = noiseFloorDb;
        currentPointsDone = pointsDone;
        currentPointsTotal = pointsTotal;
        repaint();
    }

    void setLatestTestId(const juce::String& testId)
    {
        latestTestId = testId;
        repaint();
    }

    void setCalibrationStatus(bool calibrated, bool firActive = false, float clockDriftPpm = 0.0f)
    {
        isCalibrated = calibrated;
        isFirActive = firActive;
        currentClockDriftPpm = clockDriftPpm;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Subtle background card
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 0.8f);

        auto area = bounds.reduced(10.0f, 3.0f);

        // --- 1. SNR Indicator ---
        auto snrArea = area.removeFromLeft(140.0f);
        juce::Colour snrColor;
        juce::String snrLabel;
        if (currentSnrDb >= 24.0f)
        {
            snrColor = SoundIdTheme::accentGreen;
            snrLabel = "EXCELLENT";
        }
        else if (currentSnrDb >= 18.0f)
        {
            snrColor = SoundIdTheme::accentAmber;
            snrLabel = "OK";
        }
        else
        {
            snrColor = SoundIdTheme::accentRed;
            snrLabel = "LOW";
        }

        // SNR dot
        float dotY = snrArea.getCentreY() - 4.0f;
        g.setColour(snrColor);
        g.fillEllipse(snrArea.getX(), dotY, 8.0f, 8.0f);

        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("SNR: " + juce::String(currentSnrDb, 1) + " dB",
                   snrArea.withTrimmedLeft(12.0f), juce::Justification::centredLeft, true);

        // --- 2. Confidence Badge ---
        area.removeFromLeft(4.0f);
        auto confArea = area.removeFromLeft(85.0f);
        auto badgeRect = confArea.withSizeKeepingCentre(confArea.getWidth(), 18.0f);
        g.setColour(snrColor.withAlpha(0.20f));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setColour(snrColor);
        g.drawRoundedRectangle(badgeRect, 6.0f, 1.2f);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(snrColor.darker(0.1f));
        g.drawText(snrLabel, badgeRect, juce::Justification::centred, true);

        // --- 3. Noise Floor ---
        area.removeFromLeft(12.0f);
        auto noiseArea = area.removeFromLeft(150.0f);
        g.setFont(juce::FontOptions(10.5f));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Noise Floor: ") + juce::String(currentNoiseFloorDb, 1) + " dBfs",
                   noiseArea, juce::Justification::centredLeft, true);

        // --- 4. Progress ---
        area.removeFromLeft(12.0f);
        auto progressArea = area.removeFromLeft(130.0f);
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("Points: " + juce::String(currentPointsDone) + " / " + juce::String(currentPointsTotal),
                   progressArea, juce::Justification::centredLeft, true);

        // Progress bar
        if (currentPointsTotal > 0)
        {
            auto barArea = juce::Rectangle<float>(progressArea.getX(), progressArea.getBottom() - 5.0f, progressArea.getWidth(), 3.0f);
            g.setColour(SoundIdTheme::bgCardHover);
            g.fillRoundedRectangle(barArea, 1.5f);
            float progressFrac = static_cast<float>(currentPointsDone) / static_cast<float>(currentPointsTotal);
            g.setColour(SoundIdTheme::accentGreen);
            g.fillRoundedRectangle(barArea.withWidth(barArea.getWidth() * progressFrac), 1.5f);
        }

        // --- 5. Calibration / FIR Badge ---
        area.removeFromLeft(10.0f);
        auto calArea = area.removeFromLeft(110.0f);
        auto calBadge = calArea.withSizeKeepingCentre(calArea.getWidth(), 18.0f);
        const juce::Colour calColor = isCalibrated
            ? (isFirActive ? SoundIdTheme::accentGreen : SoundIdTheme::accentCyan)
            : SoundIdTheme::textMuted;
        const juce::String calLabel = isCalibrated
            ? (isFirActive ? "CAL: FIR+TRIM" : "CAL: TRIM")
            : "UNCALIBRATED";
        g.setColour(calColor.withAlpha(0.16f));
        g.fillRoundedRectangle(calBadge, 6.0f);
        g.setColour(calColor.withAlpha(0.75f));
        g.drawRoundedRectangle(calBadge, 6.0f, 1.0f);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(calColor.darker(0.15f));
        g.drawText(calLabel, calBadge, juce::Justification::centred, true);

        if (std::abs(currentClockDriftPpm) >= 5.0f)
        {
            area.removeFromLeft(8.0f);
            auto driftArea = area.removeFromLeft(90.0f);
            const bool highDrift = std::abs(currentClockDriftPpm) > 50.0f;
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.setColour(highDrift ? SoundIdTheme::accentAmber : SoundIdTheme::textSecondary);
            const juce::String sign = currentClockDriftPpm >= 0.0f ? "+" : "";
            g.drawText("Drift: " + sign + juce::String(currentClockDriftPpm, 0) + " ppm",
                       driftArea, juce::Justification::centredLeft, true);
        }

        // --- 6. Active Test ID ---
        if (latestTestId.isNotEmpty())
        {
            g.setColour(SoundIdTheme::textMuted);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(latestTestId, area, juce::Justification::centredRight, true);
        }
    }

    void resized() override {}

private:
    float currentSnrDb { -96.0f };
    float currentNoiseFloorDb { -96.0f };
    int currentPointsDone { 0 };
    int currentPointsTotal { 0 };
    bool isCalibrated { false };
    bool isFirActive { false };
    float currentClockDriftPpm { 0.0f };
    juce::String latestTestId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementHealthPanel)
};

} // namespace abdaudiolab::gui
