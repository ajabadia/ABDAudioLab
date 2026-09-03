#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <algorithm>
#include "SoundIdTheme.h"
#include "../audio/LabAudioEngine.h"

namespace abdaudiolab::gui
{

/**
 * @brief Real-time logarithmic FFT Spectrum Analyzer with peak-hold and decay.
 *
 * Renders a 20 Hz – 20 kHz spectrum with logarithmic frequency axis,
 * smooth exponential decay, and peak-hold indicators.
 * Designed to match the SoundID Nordic light aesthetic.
 */
class LiveSpectrumAnalyzer : public juce::Component
{
public:
    LiveSpectrumAnalyzer() = default;
    ~LiveSpectrumAnalyzer() override = default;

    /**
     * @brief Feed new magnitude data from the audio engine.
     * Call from the GUI timer callback (~30 Hz).
     */
    void pushSpectrumData(const std::array<float, audio::LabAudioEngine::kSpectrumBins>& magnitudesDb,
                          double sampleRate)
    {
        currentSampleRate = sampleRate;

        for (int i = 0; i < audio::LabAudioEngine::kSpectrumBins; ++i)
        {
            float incoming = magnitudesDb[static_cast<size_t>(i)];

            // Smooth exponential decay (attack fast, release slow)
            if (incoming > displayMagnitudes[static_cast<size_t>(i)])
                displayMagnitudes[static_cast<size_t>(i)] = incoming; // instant attack
            else
                displayMagnitudes[static_cast<size_t>(i)] = displayMagnitudes[static_cast<size_t>(i)] * 0.88f + incoming * 0.12f; // ~30 dB/s decay

            // Peak-hold with slow decay
            if (incoming > peakHoldValues[static_cast<size_t>(i)])
            {
                peakHoldValues[static_cast<size_t>(i)] = incoming;
                peakHoldCounters[static_cast<size_t>(i)] = kPeakHoldFrames;
            }
            else if (peakHoldCounters[static_cast<size_t>(i)] > 0)
            {
                --peakHoldCounters[static_cast<size_t>(i)];
            }
            else
            {
                peakHoldValues[static_cast<size_t>(i)] -= 1.0f; // 30 dB/s fall at 30 fps
            }
        }

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

        auto plotArea = bounds.reduced(12.0f, 8.0f);
        auto gridArea = plotArea.withTrimmedRight(40.0f).withTrimmedBottom(22.0f).withTrimmedTop(4.0f);

        // Inner plot background
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(gridArea, 4.0f);

        // dB grid lines
        constexpr float topDb = 0.0f;
        constexpr float botDb = -96.0f;
        float dbMarkers[] = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f, -72.0f, -84.0f, -96.0f };

        g.setFont(juce::FontOptions(9.5f));
        for (float db : dbMarkers)
        {
            float normY = (topDb - db) / (topDb - botDb);
            float y = gridArea.getY() + normY * gridArea.getHeight();

            g.setColour(SoundIdTheme::borderSubtle.withAlpha(0.6f));
            g.drawHorizontalLine(static_cast<int>(y), gridArea.getX(), gridArea.getRight());

            g.setColour(SoundIdTheme::textMuted);
            juce::String txt = juce::String(static_cast<int>(db));
            g.drawText(txt, static_cast<int>(gridArea.getRight() + 4.0f), static_cast<int>(y - 5.0f), 32, 10, juce::Justification::centredLeft, false);
        }

        // Frequency markers (logarithmic)
        float freqMarkers[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        juce::String freqLabels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
        constexpr float minLogF = 1.301f; // log10(20)
        constexpr float maxLogF = 4.301f; // log10(20000)

        for (int fi = 0; fi < 9; ++fi)
        {
            float normX = (std::log10(freqMarkers[fi]) - minLogF) / (maxLogF - minLogF);
            float x = gridArea.getX() + normX * gridArea.getWidth();

            g.setColour(SoundIdTheme::borderSubtle.withAlpha(0.4f));
            g.drawVerticalLine(static_cast<int>(x), gridArea.getY(), gridArea.getBottom());

            g.setColour(SoundIdTheme::textMuted);
            g.drawText(freqLabels[fi], static_cast<int>(x - 16.0f), static_cast<int>(gridArea.getBottom() + 4.0f), 32, 14, juce::Justification::centred, false);
        }

        // Draw spectrum bars
        if (currentSampleRate < 100.0) return;

        float binResolution = static_cast<float>(currentSampleRate) / static_cast<float>(audio::LabAudioEngine::kFFTSize);
        float barWidth = std::max(1.0f, gridArea.getWidth() / 200.0f);

        juce::Path spectrumPath;
        juce::Path peakPath;
        bool pathStarted = false;

        for (int bin = 1; bin < audio::LabAudioEngine::kSpectrumBins; ++bin)
        {
            float freq = static_cast<float>(bin) * binResolution;
            if (freq < 20.0f || freq > 20000.0f) continue;

            float normX = (std::log10(freq) - minLogF) / (maxLogF - minLogF);
            float x = gridArea.getX() + normX * gridArea.getWidth();

            float dbVal = displayMagnitudes[static_cast<size_t>(bin)];
            float normY = std::clamp((topDb - dbVal) / (topDb - botDb), 0.0f, 1.0f);
            float y = gridArea.getY() + normY * gridArea.getHeight();

            if (!pathStarted)
            {
                spectrumPath.startNewSubPath(x, y);
                pathStarted = true;
            }
            else
            {
                spectrumPath.lineTo(x, y);
            }

            // Peak-hold line
            float peakDb = peakHoldValues[static_cast<size_t>(bin)];
            float peakNormY = std::clamp((topDb - peakDb) / (topDb - botDb), 0.0f, 1.0f);
            float peakY = gridArea.getY() + peakNormY * gridArea.getHeight();

            if (peakHoldCounters[static_cast<size_t>(bin)] > 0 || peakDb > -90.0f)
            {
                g.setColour(SoundIdTheme::accentAmber.withAlpha(0.5f));
                g.fillRect(x - barWidth * 0.5f, peakY, barWidth, 2.0f);
            }
        }

        // Fill area under spectrum curve
        if (pathStarted)
        {
            juce::Path fillPath(spectrumPath);
            fillPath.lineTo(gridArea.getRight(), gridArea.getBottom());
            fillPath.lineTo(gridArea.getX(), gridArea.getBottom());
            fillPath.closeSubPath();

            // Gradient fill: green -> fading to transparent
            g.setGradientFill(juce::ColourGradient(
                SoundIdTheme::accentGreen.withAlpha(0.35f), gridArea.getX(), gridArea.getY(),
                SoundIdTheme::accentGreen.withAlpha(0.05f), gridArea.getX(), gridArea.getBottom(),
                false));
            g.fillPath(fillPath);

            // Stroke the spectrum line
            g.setColour(SoundIdTheme::accentGreen);
            g.strokePath(spectrumPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void resized() override {}

private:
    double currentSampleRate { 48000.0 };
    static constexpr int kPeakHoldFrames = 45; // ~1.5 seconds at 30 fps

    std::array<float, audio::LabAudioEngine::kSpectrumBins> displayMagnitudes {};
    std::array<float, audio::LabAudioEngine::kSpectrumBins> peakHoldValues {};
    std::array<int, audio::LabAudioEngine::kSpectrumBins> peakHoldCounters {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveSpectrumAnalyzer)
};

} // namespace abdaudiolab::gui
