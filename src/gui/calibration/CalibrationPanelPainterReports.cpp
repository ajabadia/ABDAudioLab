/**
 * @file CalibrationPanelPainterReports.cpp
 * @brief Right column of the Step 2 card: live level monitor plus the persistent
 *        Step 2A / Step 2B reports and the saved-profiles box.
 * @author ABDSynths
 * @date 2026
 */

#include "CalibrationPanelPainter.h"

#include "CalibrationPanelMetrics.h"

#include <cmath>

#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::calibrationpanel::painter
{

namespace
{

using calibrationpanel::kInnerCardCornerRadius;
using calibrationpanel::kMonitorHeight;
using calibrationpanel::kSavedProfilesMaxHeight;
using calibrationpanel::kSavedProfilesTailSpace;
using calibrationpanel::kSavedProfilesTopOffset;

/** @brief Title + right-aligned status badge shared by both report headers. */
void drawReportHeader(juce::Graphics& g,
                      juce::Rectangle<float>& meterArea,
                      const juce::String& title,
                      juce::Colour titleColour,
                      const juce::String& badgeLabel,
                      juce::Colour badgeColour,
                      float gapAfter)
{
    auto header = meterArea.removeFromTop(18.0f);

    g.setFont(font(13.0f, juce::Font::bold));
    g.setColour(titleColour);
    g.drawText(title, header.removeFromLeft(220.0f), juce::Justification::centredLeft, true);

    drawBadge(g, header.removeFromRight(110.0f), badgeLabel, juce::Colours::transparentBlack,
              badgeColour, 11.0f, juce::Justification::centredRight, 0.0f);

    meterArea.removeFromTop(gapAfter);
}

/** @brief Bold headline + plain remediation body, the shape used by every failure state. */
void drawFailureBody(juce::Graphics& g,
                     juce::Rectangle<float>& meterArea,
                     const juce::String& headline,
                     const juce::String& remediation,
                     juce::Colour headlineColour,
                     float headlineHeight)
{
    g.setFont(font(13.0f, juce::Font::bold));
    g.setColour(headlineColour);
    g.drawText(headline, meterArea.removeFromTop(headlineHeight),
               juce::Justification::centredLeft, true);

    g.setFont(font(11.5f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(remediation, meterArea, juce::Justification::topLeft, true);
}

/** @brief Plain multi-line status paragraph. */
void drawStatusBody(juce::Graphics& g,
                    juce::Rectangle<float>& meterArea,
                    const juce::String& body,
                    juce::Colour colour,
                    float bodyHeight,
                    float bodyFontHeight = 12.0f)
{
    g.setFont(font(bodyFontHeight));
    g.setColour(colour);
    g.drawText(body, meterArea.removeFromTop(bodyHeight), juce::Justification::topLeft, true);
}

struct LevelReading
{
    juce::String dBfsText;
    juce::String rangeText;
    juce::Colour rangeColour;
};

/** @brief Single place that maps a live peak to its text and its colour. */
LevelReading describeLevel(float livePeak)
{
    const auto liveDb = 20.0f * std::log10(std::max(livePeak, 1e-4f));

    LevelReading reading;
    reading.dBfsText = liveDb < -70.0f ? juce::String("-inf dBFS") : juce::String(liveDb, 1) + " dBFS";

    if (liveDb > -0.5f)
    {
        reading.rangeText = " [Clipping / Overload]";
        reading.rangeColour = SoundIdTheme::accentRed;
    }
    else if (liveDb >= -24.0f)
    {
        reading.rangeText = " [Optimal Level]";
        reading.rangeColour = SoundIdTheme::accentGreen;
    }
    else if (liveDb >= -40.0f)
    {
        reading.rangeText = " [Low Level - Turn Up]";
        reading.rangeColour = SoundIdTheme::accentAmber;
    }
    else
    {
        reading.rangeText = " [Idle / Silent]";
        reading.rangeColour = SoundIdTheme::textMuted;
    }

    return reading;
}

} // namespace

//==============================================================================
void paintLevelMonitor(juce::Graphics& g,
                       const ViewState& view,
                       juce::Rectangle<float> rightColumn,
                       juce::Rectangle<float>& outMeterArea)
{
    const auto card = rightColumn.withHeight(kMonitorHeight);
    drawCardShell(g, card, SoundIdTheme::bgCardHover, SoundIdTheme::borderSubtle,
                  kInnerCardCornerRadius, 1.0f);

    auto meterArea = card.reduced(14.0f, 12.0f);

    g.setFont(font(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("REAL-TIME SIGNAL MONITOR", meterArea.removeFromTop(16.0f),
               juce::Justification::centredLeft, true);
    meterArea.removeFromTop(6.0f);

    const bool isDark = (gui::AppTheme::currentMode == gui::AppTheme::ThemeMode::Dark);

    // Horizontal VU meter using shared TelemetryTokens
    const auto meterBar = meterArea.removeFromTop(SoundIdTheme::TelemetryTokens::trackHeight);
    g.setColour(SoundIdTheme::TelemetryTokens::trackBg(isDark));
    g.fillRoundedRectangle(meterBar, SoundIdTheme::TelemetryTokens::cornerRadius);
    g.setColour(SoundIdTheme::TelemetryTokens::trackBorder(isDark));
    g.drawRoundedRectangle(meterBar, SoundIdTheme::TelemetryTokens::cornerRadius, 1.0f);

    const float optimalStart = meterBar.getX() + meterBar.getWidth() * 0.15f;
    const float optimalEnd = meterBar.getX() + meterBar.getWidth() * 0.75f;
    g.setColour(SoundIdTheme::TelemetryTokens::safeColour().withAlpha(0.12f));
    g.fillRect(juce::Rectangle<float>(optimalStart, meterBar.getY(),
                                      optimalEnd - optimalStart, meterBar.getHeight()));

    const auto peak = juce::jlimit(0.0f, 1.0f, view.liveInputPeak);
    const auto reading = describeLevel(view.liveInputPeak);

    // The track colour only changes to red/amber past the optimal window.
    juce::Colour meterColour = SoundIdTheme::textMuted.withAlpha(0.4f);
    if (peak > 0.95f)
        meterColour = SoundIdTheme::TelemetryTokens::clippingColour();
    else if (peak >= 0.15f)
        meterColour = SoundIdTheme::TelemetryTokens::safeColour();
    else if (peak > 0.02f)
        meterColour = SoundIdTheme::TelemetryTokens::warningColour();

    g.setColour(meterColour);
    g.fillRoundedRectangle(meterBar.withWidth(meterBar.getWidth() * peak), SoundIdTheme::TelemetryTokens::cornerRadius);

    meterArea.removeFromTop(4.0f);

    auto levelRow = meterArea.removeFromTop(18.0f);
    g.setFont(font(12.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Input: " + reading.dBfsText, levelRow.removeFromLeft(125.0f),
               juce::Justification::centredLeft, true);

    g.setFont(font(11.5f, juce::Font::bold));
    g.setColour(reading.rangeColour);
    g.drawText(reading.rangeText, levelRow, juce::Justification::centredLeft, true);

    meterArea.removeFromTop(4.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(meterArea.removeFromTop(1.0f));
    meterArea.removeFromTop(6.0f);

    // Whatever is left inside the monitor card hosts the active sub-step report.
    outMeterArea = meterArea;
}

//==============================================================================
void paintNoiseBaselineReport(juce::Graphics& g, const ViewState& view, juce::Rectangle<float>& meterArea)
{
    const auto baselineState = view.noiseBaselineState;

    juce::String badgeLabel;
    juce::Colour badgeColour;
    if (baselineState == NoiseBaselineState::Passed)
    {
        badgeLabel = "[ PASS ]";
        badgeColour = SoundIdTheme::accentGreen;
    }
    else if (baselineState == NoiseBaselineState::Checking)
    {
        badgeLabel = "[ CHECKING ]";
        badgeColour = SoundIdTheme::accentBlue;
    }
    else if (baselineState == NoiseBaselineState::Contaminated)
    {
        badgeLabel = "[ CONTAMINATED ]";
        badgeColour = SoundIdTheme::accentRed;
    }
    else if (baselineState == NoiseBaselineState::SafetyAborted)
    {
        badgeLabel = "[ SAFETY ABORT ]";
        badgeColour = SoundIdTheme::accentRed;
    }
    else if (baselineState == NoiseBaselineState::DeviceStopped)
    {
        badgeLabel = "[ DEVICE STOPPED ]";
        badgeColour = SoundIdTheme::accentRed;
    }
    else
    {
        badgeLabel = "[ PENDING ]";
        badgeColour = SoundIdTheme::textMuted;
    }

    drawReportHeader(g, meterArea, "2A INPUT NOISE BASELINE REPORT",
                     baselineState == NoiseBaselineState::Passed ? SoundIdTheme::accentGreen
                                                                 : SoundIdTheme::textPrimary,
                     badgeLabel, badgeColour, 6.0f);

    switch (baselineState)
    {
        case NoiseBaselineState::Passed:
        {
            g.setFont(font(12.0f));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText("RMS Noise Floor: " + formatDbfs(view.noiseReport.rmsDbfs, 1) + " [SAFE < -45 dBFS]",
                       meterArea.removeFromTop(17.0f), juce::Justification::centredLeft, true);
            g.drawText("Peak Noise Floor: " + formatDbfs(view.noiseReport.peakDbfs, 1) + " [INFO]",
                       meterArea.removeFromTop(17.0f), juce::Justification::centredLeft, true);
            g.drawText("Channel: " + view.noiseReport.inputChannel + " | Device: " + view.noiseReport.deviceName,
                       meterArea.removeFromTop(17.0f), juce::Justification::centredLeft, true);
            if (view.noiseReport.timestampIso.isNotEmpty())
                g.drawText("Measured At: " + view.noiseReport.timestampIso,
                           meterArea.removeFromTop(17.0f), juce::Justification::centredLeft, true);

            meterArea.removeFromTop(6.0f);
            drawStatusBody(g, meterArea,
                           "Input verified quiet and safe. Ready to switch to 2B for loopback calibration.",
                           SoundIdTheme::accentGreen, 28.0f, 12.0f);
            break;
        }

        case NoiseBaselineState::Checking:
            drawStatusBody(g, meterArea, "Measuring 400 ms input baseline under confirmed digital silence...",
                           SoundIdTheme::accentBlue, 20.0f, 12.0f);
            break;

        case NoiseBaselineState::Contaminated:
            drawFailureBody(g, meterArea,
                            "Noise Floor Exceeds -45 dBFS (" + juce::String(view.noiseReport.rmsDbfs, 1) + " dBFS)",
                            "Cause: Instrument or microphone connected, or input preamp gain too high.\n"
                            "Action: Disconnect inputs, lower preamp gain, and retry.",
                            SoundIdTheme::accentRed, 18.0f);
            break;

        case NoiseBaselineState::SafetyAborted:
            drawFailureBody(g, meterArea, "Safety Abort: Signal Overload with Muted Output",
                            "Cause: Dangerous signal or feedback loop detected (> -6 dBFS).\n"
                            "Action: Turn Direct Monitor OFF, set Mix toward Playback, and lower gain.",
                            SoundIdTheme::accentRed, 18.0f);
            break;

        case NoiseBaselineState::DeviceStopped:
            drawFailureBody(g, meterArea, "Audio Device Stopped During Baseline",
                            "Cause: the audio interface was closed or disconnected mid-measurement.\n"
                            "Action: Reopen the device, confirm sample rate and buffer size, and retry 2A.",
                            SoundIdTheme::accentRed, 18.0f);
            break;

        default:
            drawStatusBody(g, meterArea,
                           "Input noise floor has not been measured yet.\n"
                           "Click [Check Input Noise Baseline] below to start.",
                           SoundIdTheme::textSecondary, 32.0f);
            break;
    }
}

//==============================================================================
void paintLoopbackReport(juce::Graphics& g, const ViewState& view, juce::Rectangle<float>& meterArea)
{
    const auto loopback = view.loopbackState;

    juce::String badgeLabel;
    juce::Colour badgeColour;
    switch (loopback)
    {
        case LoopbackState::Passed:
            badgeLabel = "[ PASS ]";
            badgeColour = SoundIdTheme::accentGreen;
            break;
        case LoopbackState::Stale:
            badgeLabel = "[ STALE ]";
            badgeColour = SoundIdTheme::accentAmber;
            break;
        case LoopbackState::MeasuringPreflight:
            badgeLabel = "[ PREFLIGHT ]";
            badgeColour = SoundIdTheme::accentBlue;
            break;
        case LoopbackState::MeasuringSweep:
            badgeLabel = "[ SWEEPING ]";
            badgeColour = SoundIdTheme::accentBlue;
            break;
        case LoopbackState::Ready:
            badgeLabel = "[ READY ]";
            badgeColour = SoundIdTheme::accentGreen;
            break;
        case LoopbackState::Clipped:
            badgeLabel = "[ CLIPPED ]";
            badgeColour = SoundIdTheme::accentRed;
            break;
        case LoopbackState::SignalTooLow:
            badgeLabel = "[ LOW LEVEL ]";
            badgeColour = SoundIdTheme::accentAmber;
            break;
        case LoopbackState::Locked:
            badgeLabel = "[ LOCKED ]";
            badgeColour = SoundIdTheme::textMuted;
            break;
        case LoopbackState::DeviceStopped:
            badgeLabel = "[ DEVICE STOPPED ]";
            badgeColour = SoundIdTheme::accentRed;
            break;
        default:
            badgeLabel = "[ FAIL ]";
            badgeColour = SoundIdTheme::accentRed;
            break;
    }

    drawReportHeader(g, meterArea, "2B PHYSICAL LOOPBACK REPORT",
                     loopback == LoopbackState::Passed ? SoundIdTheme::accentGreen : SoundIdTheme::textPrimary,
                     badgeLabel, badgeColour, 4.0f);

    meterArea.removeFromTop(4.0f);

    const auto baselineRef = view.noiseBaselineState == NoiseBaselineState::Passed
                                 ? juce::String(view.noiseReport.rmsDbfs, 1) + " dBFS [PASS]"
                                 : juce::String("[NOT CHECKED]");
    g.setFont(font(11.5f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("Baseline (2A): " + baselineRef, meterArea.removeFromTop(16.0f),
               juce::Justification::centredLeft, true);

    meterArea.removeFromTop(4.0f);

    const auto& report = view.loopbackReport;

    switch (loopback)
    {
        case LoopbackState::Passed:
        {
            g.setFont(font(12.0f));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText("Roundtrip Latency: " + juce::String(report.roundTripLatencyMs, 2) + " ms ("
                           + juce::String(report.latencySamples) + " samples)",
                       meterArea.removeFromTop(16.5f), juce::Justification::centredLeft, true);
            g.drawText("Recommended Level Trim: " + formatTrimDb(report.recommendedTrimGain, 2),
                       meterArea.removeFromTop(16.5f), juce::Justification::centredLeft, true);
            g.drawText("Flatness Delta: " + juce::String(report.frequencyFlatnessDb, 1) + " dB [PASS < 6.0 dB]",
                       meterArea.removeFromTop(16.5f), juce::Justification::centredLeft, true);
            juce::String snrTag = (report.snrMethod == math::SnrMeasurementMethod::PhysicalNoiseBaseline)
                                      ? " [PHYSICAL]"
                                      : (report.snrMethod == math::SnrMeasurementMethod::LegacyAssumedNoiseFloor
                                             ? " [ESTIMATED]"
                                             : "");
            g.drawText("SNR: " + juce::String(report.snrDb, 1) + " dB" + snrTag + " | THD+N: " + juce::String(report.thdPercent, 3) + "%",
                       meterArea.removeFromTop(16.5f), juce::Justification::centredLeft, true);
            g.drawText(juce::String("Polarity: ") + (report.phaseInverted ? "Inverted (180 deg)" : "Normal"),
                       meterArea.removeFromTop(16.5f), juce::Justification::centredLeft, true);

            if (!view.saveFeedbackText.isEmpty())
            {
                g.setFont(font(11.5f, juce::Font::bold));
                g.setColour(SoundIdTheme::accentGreen);
                g.drawText(view.saveFeedbackText, meterArea.removeFromTop(20.0f),
                           juce::Justification::topLeft, true);
            }
            break;
        }

        case LoopbackState::Stale:
            drawFailureBody(g, meterArea, "Calibration Invalidate: State is STALE",
                            "A new baseline check (2A) was run or audio settings changed.\n"
                            "Active latency and trim compensation is neutralized.\n"
                            "Run loopback calibration to establish a new active calibration.",
                            SoundIdTheme::accentAmber, 16.0f);
            break;

        case LoopbackState::DeviceStopped:
            drawFailureBody(g, meterArea, "Audio Device Stopped During Sweep",
                            "Cause: the audio interface was closed or disconnected mid-measurement.\n"
                            "Action: Reopen the device, confirm sample rate and buffer size, and re-run 2B.",
                            SoundIdTheme::accentRed, 18.0f);
            break;

        case LoopbackState::Locked:
            drawStatusBody(g, meterArea,
                           "Physical loopback measurement is locked.\n"
                           "Please complete Step 2A (Input Noise Baseline) first.",
                           SoundIdTheme::textMuted, 32.0f);
            break;

        case LoopbackState::MeasuringPreflight:
            drawStatusBody(g, meterArea,
                           "Running 200 ms Loopback Safety Check (output muted)...\n"
                           "Checking for feedback loops or overload.",
                           SoundIdTheme::accentBlue, 32.0f);
            break;

        case LoopbackState::MeasuringSweep:
            drawStatusBody(g, meterArea,
                           "Emitting logarithmic Farina sweep (1300 ms)...\n"
                           "Capturing loopback impulse response.",
                           SoundIdTheme::accentBlue, 32.0f);
            break;

        case LoopbackState::Ready:
        case LoopbackState::Cancelled:
            drawStatusBody(g, meterArea,
                           "Step 2A verified. Connect patch cable " + view.outputChannelName + " -> "
                               + view.inputChannelName + ".\n"
                               "Click [Run Physical Loopback Calibration] below.",
                           SoundIdTheme::textSecondary, 32.0f);
            break;

        default:
        {
            const auto reason = report.failureReason.isNotEmpty() ? report.failureReason
                                                                  : view.diagnostics.failureReason;
            juce::String extraDiagnostics;
            if (view.diagnostics.latencySamples > 0)
            {
                extraDiagnostics += "\nRTL: " + juce::String(view.diagnostics.roundTripLatencyMs, 2)
                                    + " ms (" + juce::String(view.diagnostics.latencySamples) + " smp) [DIAGNOSTIC ONLY — CALIBRATION FAILED]";
            }
            if (view.diagnostics.snrDb != 0.0f || view.noiseBaselineState == NoiseBaselineState::Passed)
            {
                extraDiagnostics += "\nSNR: " + juce::String(view.diagnostics.snrDb, 1) + " dB [PHYSICAL BASELINE, INVALID LOOPBACK]";
            }
            drawFailureBody(g, meterArea, "Failed: " + reason,
                            "Peak In: " + juce::String(view.diagnostics.peakInDbfs, 1) + " dBFS\n"
                            "Captured: " + juce::String(view.diagnostics.samplesCaptured) + " / "
                                + juce::String(view.diagnostics.samplesRequired)
                                + extraDiagnostics + "\n"
                            "Verify physical patch cable connection and retry.",
                            SoundIdTheme::accentRed, 18.0f);
            break;
        }
    }
}

//==============================================================================
void paintSavedProfiles(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> rightColumn)
{
    if (!view.showSavedProfiles)
        return;

    auto savedArea = rightColumn;
    savedArea.removeFromTop(kSavedProfilesTopOffset);
    const auto savedBox = savedArea.removeFromTop(
        juce::jmin(kSavedProfilesMaxHeight, savedArea.getHeight() - kSavedProfilesTailSpace));

    drawCardShell(g, savedBox, SoundIdTheme::bgCardHover, SoundIdTheme::borderSubtle,
                  kInnerCardCornerRadius, 1.0f);

    auto inner = savedBox.reduced(10.0f, 8.0f);

    const auto* profiles = view.savedProfiles;
    const auto totalProfiles = profiles != nullptr ? static_cast<int>(profiles->size()) : 0;
    const auto profileIndex = juce::jmax(0, view.selectedProfileIndex);
    const auto hasProfile = totalProfiles > 0 && profileIndex < totalProfiles;

    auto titleRow = inner.removeFromTop(18.0f);
    g.setFont(font(11.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("SAVED PROFILES IN APPDATA", titleRow, juce::Justification::centredLeft, true);

    if (hasProfile)
    {
        // Position of the profile currently being inspected, matching the [<] [>] buttons.
        g.setColour(SoundIdTheme::accentBlue);
        g.drawText(juce::String(profileIndex + 1) + " / " + juce::String(totalProfiles),
                   titleRow, juce::Justification::centredRight, true);
    }

    inner.removeFromTop(4.0f);

    if (!hasProfile)
    {
        g.setFont(font(12.0f));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText("No saved calibration profiles found on disk.", inner,
                   juce::Justification::centredLeft, true);
        return;
    }

    const auto& profile = (*profiles)[static_cast<size_t>(profileIndex)];

    g.setFont(font(12.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String(profile.deviceSnapshot.deviceName) + " ("
                   + juce::String(profile.deviceSnapshot.driverType) + ")",
               inner.removeFromTop(16.0f), juce::Justification::centredLeft, true);

    g.setFont(font(11.5f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(juce::String(profile.deviceSnapshot.sampleRate / 1000.0, 1) + " kHz | Buffer "
                   + juce::String(profile.deviceSnapshot.bufferSizeSamples) + " | "
                   + juce::String(profile.routingSnapshot.outputChannelLabel) + " -> "
                   + juce::String(profile.routingSnapshot.inputChannelLabel),
               inner.removeFromTop(15.0f), juce::Justification::centredLeft, true);

    g.setFont(font(11.0f));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("Date: " + juce::String(profile.createdAt), inner.removeFromTop(14.0f),
               juce::Justification::centredLeft, true);

    if (view.showProfileDetails)
    {
        const auto& result = profile.calibrationResult;
        g.setFont(font(11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText("Lat: " + juce::String(result.roundTripLatencyMs, 1) + " ms ("
                       + juce::String(result.latencySamples) + " spls) | SNR: "
                       + juce::String(result.snrDb, 1) + " dB | Flat: "
                       + juce::String(result.frequencyFlatnessDb, 1) + " dB",
                   inner.removeFromTop(15.0f), juce::Justification::centredLeft, true);
    }
}

} // namespace abdaudiolab::gui::calibrationpanel::painter
