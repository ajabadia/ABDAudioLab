#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"
#include <cmath>
#include <numbers>

namespace abdaudiolab::gui
{

/**
 * @brief Rhythmic visual progress wheel and metronome guide for manual sweeps.
 *
 * Displays:
 *  1. An animated circular cadence ring (default 10 seconds per full turn)
 *  2. Second markers / ticks (1..10)
 *  3. Live progress percentage and remaining time countdown
 *  4. High-contrast metronome flash on each second tick
 *  5. Operator prompt label ("Gira la perilla a ritmo constante de 10s")
 */
class RhythmicMetronomeComponent : public juce::Component,
                                   private juce::Timer
{
public:
    RhythmicMetronomeComponent()
    {
        setOpaque(false);

        btnClose.setButtonText(juce::String::fromUTF8(u8"✕"));
        btnClose.setTooltip(juce::String::fromUTF8(u8"Cerrar metrónomo y volver a controles [Esc]"));
        btnClose.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btnClose.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        btnClose.onClick = [this] {
            stopSweep();
            if (onCloseRequested) onCloseRequested();
        };
        addAndMakeVisible(btnClose);

        auto setupDurationBtn = [this](juce::TextButton& btn, const juce::String& text, double dur) {
            btn.setButtonText(text);
            btn.setTooltip("Configurar barrido a " + text);
            btn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
            btn.onClick = [this, dur] { setSweepDuration(dur); };
            addAndMakeVisible(btn);
        };
        setupDurationBtn(btn5s, "5s", 5.0);
        setupDurationBtn(btn10s, "10s", 10.0);
        setupDurationBtn(btn15s, "15s", 15.0);
        updateDurationButtons();
    }

    ~RhythmicMetronomeComponent() override
    {
        stopTimer();
    }

    void setSweepDuration(double sec)
    {
        if (isRunning) return;
        sweepDurationSec = (sec >= 2.0) ? sec : 10.0;
        updateDurationButtons();
        repaint();
    }

    [[nodiscard]] double getSweepDuration() const noexcept { return sweepDurationSec; }

    void startSweep(double totalDurationSec = -1.0)
    {
        if (totalDurationSec > 0.5)
            sweepDurationSec = totalDurationSec;

        elapsedSeconds = 0.0;
        isRunning = true;
        isFinished = false;
        lastEmittedSecond = -1;
        flashAlpha = 0.0f;
        startTimerHz(60); // 60 FPS smooth animation
        repaint();
    }

    void stopSweep()
    {
        stopTimer();
        isRunning = false;
        isFinished = false;
        elapsedSeconds = 0.0;
        flashAlpha = 0.0f;
        repaint();
    }

    void resetSweep()
    {
        stopSweep();
    }

    [[nodiscard]] bool getIsRunning() const noexcept { return isRunning; }
    [[nodiscard]] bool getIsFinished() const noexcept { return isFinished; }
    [[nodiscard]] float getProgressNormalized() const noexcept
    {
        if (sweepDurationSec <= 0.0) return 0.0f;
        return std::clamp(static_cast<float>(elapsedSeconds / sweepDurationSec), 0.0f, 1.0f);
    }

    std::function<void(int secondTick)> onSecondTick;
    std::function<void()> onSweepFinished;
    std::function<void()> onCloseRequested;

    void setInstructionText(const juce::String& text)
    {
        instructionText = text;
        repaint();
    }

    void setTargetControlName(const juce::String& name)
    {
        controlName = name;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty()) return;

        // Container card styling
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        auto content = bounds.reduced(12.0f, 10.0f);

        // Header with control name and prompt
        auto header = content.removeFromTop(20.0f);
        header.removeFromRight(30.0f); // Reserve space for close button
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        juce::String title = controlName.isNotEmpty() ? ("BARRIDO MANUAL: " + controlName.toUpperCase()) 
                                                      : "BARRIDO MANUAL CONTINUO";
        g.drawText(title, header, juce::Justification::centred, true);

        // Bottom instruction label
        auto footer = content.removeFromBottom(24.0f);
        footer.removeFromLeft(140.0f); // Space for duration buttons
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::italic));
        g.setColour(SoundIdTheme::textSecondary);
        juce::String prompt = instructionText.isNotEmpty() ? instructionText 
                                                           : juce::String::fromUTF8(u8"Gira la perilla siguiendo los clics hasta completar la vuelta");
        g.drawText(prompt, footer, juce::Justification::centredLeft, true);

        content.removeFromTop(4.0f);

        // Circular Wheel Geometry
        float diameter = std::clamp(std::min(content.getWidth(), content.getHeight()), 70.0f, 150.0f);
        auto dialArea = content.withSizeKeepingCentre(diameter, diameter);
        float cx = dialArea.getCentreX();
        float cy = dialArea.getCentreY();
        float radius = diameter * 0.5f;

        // Base Track (Muted Circle)
        float strokeW = std::clamp(diameter * 0.08f, 5.0f, 10.0f);
        float trackR = radius - strokeW * 0.5f - 2.0f;

        juce::Path bgTrack;
        bgTrack.addCentredArc(cx, cy, trackR, trackR, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        g.setColour(SoundIdTheme::borderCard);
        g.strokePath(bgTrack, juce::PathStrokeType(strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Metronome Tick Markers (dynamically drawn based on sweepDurationSec)
        int totalTicks = static_cast<int>(std::round(sweepDurationSec));
        if (totalTicks <= 0) totalTicks = 10;
        for (int i = 0; i < totalTicks; ++i)
        {
            float angle = (static_cast<float>(i) / static_cast<float>(totalTicks)) * juce::MathConstants<float>::twoPi;
            float t1 = trackR - strokeW * 0.8f;
            float t2 = trackR + strokeW * 0.8f;
            float x1 = cx + t1 * std::sin(angle);
            float y1 = cy - t1 * std::cos(angle);
            float x2 = cx + t2 * std::sin(angle);
            float y2 = cy - t2 * std::cos(angle);

            g.setColour(SoundIdTheme::borderSubtle);
            g.drawLine(x1, y1, x2, y2, 1.5f);
        }

        // Active Progress Arc
        float normProgress = getProgressNormalized();
        if (normProgress > 0.001f)
        {
            float progressAngle = normProgress * juce::MathConstants<float>::twoPi;
            juce::Path progressArc;
            // Arc starts at top (0.0 rad in addCentredArc starts at 3 o'clock, so offset by -pi/2)
            progressArc.addCentredArc(cx, cy, trackR, trackR, 0.0f, -juce::MathConstants<float>::halfPi, -juce::MathConstants<float>::halfPi + progressAngle, true);

            // Flash effect when on second tick
            juce::Colour arcColour = SoundIdTheme::accentGreen.interpolatedWith(juce::Colours::white, flashAlpha * 0.6f);
            g.setColour(arcColour);
            g.strokePath(progressArc, juce::PathStrokeType(strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Center Indicator (Time countdown and metronome pulse badge)
        float innerRadius = trackR - strokeW * 0.7f;
        auto innerCircle = dialArea.withSizeKeepingCentre(innerRadius * 1.7f, innerRadius * 1.7f);

        // Flash pulse background
        juce::Colour innerBg = SoundIdTheme::bgCard.interpolatedWith(SoundIdTheme::accentGreen.withAlpha(0.25f), flashAlpha);
        g.setColour(innerBg);
        g.fillEllipse(innerCircle);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawEllipse(innerCircle, 1.0f);

        // Center Text (e.g. "6.4s" or "100%")
        g.setFont(juce::FontOptions("Inter", std::clamp(diameter * 0.16f, 13.0f, 22.0f), juce::Font::bold));
        g.setColour(isRunning ? SoundIdTheme::accentGreen : SoundIdTheme::textPrimary);
        juce::String timeText = juce::String(elapsedSeconds, 1) + "s";
        g.drawText(timeText, innerCircle.reduced(2.0f), juce::Justification::centred, true);
    }

    void resized() override
    {
        btnClose.setBounds(getWidth() - 28, 6, 22, 22);

        int durBtnW = 38;
        int durBtnH = 20;
        int bottomY = getHeight() - 26;
        btn5s.setBounds(12, bottomY, durBtnW, durBtnH);
        btn10s.setBounds(12 + durBtnW + 4, bottomY, durBtnW, durBtnH);
        btn15s.setBounds(12 + (durBtnW + 4) * 2, bottomY, durBtnW, durBtnH);
    }

private:
    void updateDurationButtons()
    {
        auto styleBtn = [](juce::TextButton& btn, bool active) {
            btn.setColour(juce::TextButton::buttonColourId, active ? SoundIdTheme::accentGreen.withAlpha(0.22f) : SoundIdTheme::surfaceSubtle);
            btn.setColour(juce::TextButton::textColourOffId, active ? SoundIdTheme::accentGreen : SoundIdTheme::textSecondary);
        };
        styleBtn(btn5s, std::abs(sweepDurationSec - 5.0) < 0.1);
        styleBtn(btn10s, std::abs(sweepDurationSec - 10.0) < 0.1);
        styleBtn(btn15s, std::abs(sweepDurationSec - 15.0) < 0.1);
    }

    void timerCallback() override
    {
        if (!isRunning) return;

        elapsedSeconds += (1.0 / 60.0);

        // Decay flash
        if (flashAlpha > 0.01f)
        {
            flashAlpha = std::max(0.0f, flashAlpha - 0.08f);
        }

        // Check if crossed into a new second
        int currentSec = static_cast<int>(std::floor(elapsedSeconds));
        if (currentSec > lastEmittedSecond && currentSec <= static_cast<int>(sweepDurationSec))
        {
            lastEmittedSecond = currentSec;
            flashAlpha = 1.0f;
            if (onSecondTick)
                onSecondTick(currentSec);
        }

        if (elapsedSeconds >= sweepDurationSec)
        {
            elapsedSeconds = sweepDurationSec;
            isRunning = false;
            isFinished = true;
            stopTimer();
            if (onSweepFinished)
                onSweepFinished();
        }

        repaint();
    }

    juce::TextButton btnClose;
    juce::TextButton btn5s;
    juce::TextButton btn10s;
    juce::TextButton btn15s;

    double sweepDurationSec { 10.0 };
    double elapsedSeconds { 0.0 };
    bool isRunning { false };
    bool isFinished { false };
    int lastEmittedSecond { -1 };
    float flashAlpha { 0.0f };

    juce::String controlName;
    juce::String instructionText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmicMetronomeComponent)
};

} // namespace abdaudiolab::gui
