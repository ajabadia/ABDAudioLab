/**
 * @file ExportReportPanel.h
 * @brief Step 4 (Export & Report) Certification & 1-Click Production Package Panel.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"
#include <functional>
#include <cmath>

namespace abdaudiolab::gui
{

class ExportReportPanel : public juce::Component, private juce::Timer
{
public:
    ExportReportPanel()
    {
        // Header texts with Sonarworks SoundID Reference style
        titleLabel.setText("HARDWARE CERTIFICATION COMPLETED", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        subtitleLabel.setText("The device has been successfully characterized via adaptive closed-loop measurement.", juce::dontSendNotification);
        subtitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
        subtitleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(subtitleLabel);

        // Hero 1-Click Export Button
        exportButton.setButtonText(juce::String::fromUTF8(u8"\u26a1 EXPORT PRODUCTION PACKAGE (1-CLICK)"));
        exportButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        exportButton.onClick = [this] { if (onExportRequested) onExportRequested(); };
        addAndMakeVisible(exportButton);

        // Cloud Publish Button
        publishCloudButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x98\x81 PUBLISH TO CLOUD (API)")));
        publishCloudButton.setTooltip("Validate with JSON Schema and publish the bundle .tar.gz to the central community server");
        publishCloudButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        publishCloudButton.onClick = [this] { if (onPublishCloudRequested) onPublishCloudRequested(); };
        addAndMakeVisible(publishCloudButton);

        // Secondary quick action buttons
        openFolderButton.setButtonText("Open Export Folder");
        openFolderButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        openFolderButton.onClick = [this] { if (onOpenFolderRequested) onOpenFolderRequested(); };
        addAndMakeVisible(openFolderButton);

        viewHtmlButton.setButtonText("View HTML Report");
        viewHtmlButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        viewHtmlButton.onClick = [this] { if (onViewHtmlRequested) onViewHtmlRequested(); };
        addAndMakeVisible(viewHtmlButton);

        // Real-Time Audition DSP Preview Toggle ("Comprobar cómo sonaría")
        auditionToggleButton.setButtonText("PREVIEW AUDIO CORRECTION (DSP AUDITION)");
        auditionToggleButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        auditionToggleButton.onClick = [this] {
            isAuditioning = !isAuditioning;
            updateAuditionUi();
            if (onAuditionToggled) onAuditionToggled(isAuditioning);
        };
        addAndMakeVisible(auditionToggleButton);

        // A/B Verification Button (Cross-Validation against physical hardware)
        btnVerifyAb.setButtonText("A/B VERIFICATION (DSP vs HARDWARE)");
        btnVerifyAb.setTooltip("Compare physical hardware audio against emulated DSP model via cross-correlation and FFT spectral analysis");
        btnVerifyAb.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        btnVerifyAb.onClick = [this] { if (onVerifyAbRequested) onVerifyAbRequested(); };
        addAndMakeVisible(btnVerifyAb);

        // Audition DSP sliders & combo
        lblCutoff.setText("Cutoff:", juce::dontSendNotification);
        lblCutoff.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        lblCutoff.setJustificationType(juce::Justification::centredRight);
        addChildComponent(lblCutoff);

        sliderCutoff.setRange(0.0, 1.0, 0.005);
        sliderCutoff.setValue(0.5);
        sliderCutoff.setSliderStyle(juce::Slider::LinearHorizontal);
        sliderCutoff.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        sliderCutoff.onValueChange = [this] {
            if (onAuditionParamsChanged)
                onAuditionParamsChanged(static_cast<float>(sliderCutoff.getValue()), static_cast<float>(sliderResonance.getValue()));
        };
        addChildComponent(sliderCutoff);

        lblResonance.setText("Reso:", juce::dontSendNotification);
        lblResonance.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        lblResonance.setJustificationType(juce::Justification::centredRight);
        addChildComponent(lblResonance);

        sliderResonance.setRange(0.0, 1.0, 0.005);
        sliderResonance.setValue(0.5);
        sliderResonance.setSliderStyle(juce::Slider::LinearHorizontal);
        sliderResonance.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        sliderResonance.onValueChange = [this] {
            if (onAuditionParamsChanged)
                onAuditionParamsChanged(static_cast<float>(sliderCutoff.getValue()), static_cast<float>(sliderResonance.getValue()));
        };
        addChildComponent(sliderResonance);

        comboWaveform.addItem("Sawtooth (130 Hz)", 1);
        comboWaveform.addItem("Square (130 Hz)", 2);
        comboWaveform.addItem("White Noise", 3);
        comboWaveform.setSelectedId(1, juce::dontSendNotification);
        comboWaveform.onChange = [this] {
            if (onAuditionWaveformChanged)
                onAuditionWaveformChanged(comboWaveform.getSelectedId() - 1);
        };
        addChildComponent(comboWaveform);

        // Interactive status message below buttons
        statusLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00a86b));
        statusLabel.setVisible(false);
        addAndMakeVisible(statusLabel);

        // Default base metrics
        updateMetrics(98.4f, -92.1f, 0.012f, 12, 24.5f);
        applyButtonStyling();
    }

    ~ExportReportPanel() override
    {
        stopTimer();
    }

    /**
     * @brief Shows visual confirmation of export success.
     */
    void showExportSuccess(const juce::String& targetFolder, const juce::String& baseName)
    {
        juce::ignoreUnused(targetFolder, baseName);
        exportButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93 PACKAGE EXPORTED!")));
        exportButton.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::primaryBg());
        statusLabel.setText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93 Package exported: C++ Header (alignas 16), Telemetry JSON, HTML Report, and Manifest")), juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
        statusLabel.setVisible(true);
        startTimer(4000);
        repaint();
    }

    /**
     * @brief Displays an informational message in the status banner.
     */
    void showStatusMessage(const juce::String& message, bool isError = false)
    {
        statusLabel.setText(message, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, isError ? juce::Colour(0xffef4444) : juce::Colour(0xff00a86b));
        statusLabel.setVisible(true);
        startTimer(5000);
        repaint();
    }

    /**
     * @brief Updates real session telemetry for the certification card.
     */
    void updateMetrics(float avgSnr, float noiseFloor, float avgThd, int totalTakes, float durationSec)
    {
        metricsText = "ACOUSTIC QUALITY METRICS\n\n"
            + juce::String::fromUTF8(u8"• Signal-to-Noise Ratio (Mean SNR): ") + juce::String(avgSnr, 1) + " dB\n"
            + juce::String::fromUTF8(u8"• Residual Noise Floor: ") + juce::String(noiseFloor, 1) + " dBFS\n"
            + juce::String::fromUTF8(u8"• Harmonic Distortion (Mean THD): ") + juce::String(avgThd, 3) + "%\n"
            + juce::String::fromUTF8(u8"• Surgical Takes: ") + juce::String(totalTakes) + " (Catmull-Rom optimized)\n"
            + juce::String::fromUTF8(u8"• Acquisition Duration: ") + juce::String(durationSec, 1) + " seconds";

        repaint();
    }

    void updateAuditionUi()
    {
        auditionToggleButton.setButtonText(isAuditioning ? "STOP DSP AUDITION"
                                                         : "PREVIEW AUDIO CORRECTION (DSP AUDITION)");
        applyButtonStyling();

        lblCutoff.setVisible(isAuditioning);
        sliderCutoff.setVisible(isAuditioning);
        lblResonance.setVisible(isAuditioning);
        sliderResonance.setVisible(isAuditioning);
        comboWaveform.setVisible(isAuditioning);
        resized();
        repaint();
    }

    void applyButtonStyling()
    {
        const bool isDark = (AppTheme::currentMode == AppTheme::ThemeMode::Dark);

        // 1. PRIMARY: Dominant 1-Click Export Package
        exportButton.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::primaryBg());
        exportButton.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::primaryText());
        exportButton.setColour(juce::TextButton::textColourOnId, SoundIdTheme::ButtonTokens::primaryText());

        // 2. REVIEW: Preview Audio Correction & A/B Verification (Neutral surfaces)
        const auto revBg = isAuditioning ? (isDark ? juce::Colour(0xff1e293b) : juce::Colour(0xffe2e8f0))
                                         : SoundIdTheme::ButtonTokens::secondaryBg(isDark);
        const auto revText = isAuditioning ? SoundIdTheme::accentGreen
                                           : SoundIdTheme::ButtonTokens::secondaryText(isDark);
        auditionToggleButton.setColour(juce::TextButton::buttonColourId, revBg);
        auditionToggleButton.setColour(juce::TextButton::textColourOffId, revText);

        btnVerifyAb.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
        btnVerifyAb.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDark));

        // 3. UTILITIES: Open Export Folder & View HTML Report (Neutral support surfaces)
        openFolderButton.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
        openFolderButton.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDark));

        viewHtmlButton.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::secondaryBg(isDark));
        viewHtmlButton.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::secondaryText(isDark));

        // 4. EXTERNAL: Publish to Cloud (Restrained secondary blue, clearly distinct from local export)
        const auto cloudBg = isDark ? juce::Colour(0xff172554) : juce::Colour(0xffeff6ff);
        const auto cloudText = isDark ? juce::Colour(0xff60a5fa) : juce::Colour(0xff1d4ed8);
        publishCloudButton.setColour(juce::TextButton::buttonColourId, cloudBg);
        publishCloudButton.setColour(juce::TextButton::textColourOffId, cloudText);
        publishCloudButton.setColour(juce::TextButton::textColourOnId, cloudText);
    }

    std::function<void()> onExportRequested;
    std::function<void()> onOpenFolderRequested;
    std::function<void()> onViewHtmlRequested;
    std::function<void()> onPublishCloudRequested;
    std::function<void()> onVerifyAbRequested;
    std::function<void(bool active)> onAuditionToggled;
    std::function<void(float p1, float p2)> onAuditionParamsChanged;
    std::function<void(int waveform)> onAuditionWaveformChanged;

protected:
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        const juce::Colour accentGreen  = SoundIdTheme::accentGreen;
        const juce::Colour textPrimary  = getLookAndFeel().findColour(juce::Label::textColourId);
        const juce::Colour borderSubtle = textPrimary.withAlpha(0.12f);
        const juce::Colour cardBg       = textPrimary.withAlpha(0.02f);

        // 1. Central Certification Card
        auto cardBounds = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.88f, bounds.getHeight() * 0.80f);
        g.setColour(cardBg);
        g.fillRoundedRectangle(cardBounds, 8.0f);
        g.setColour(borderSubtle);
        g.drawRoundedRectangle(cardBounds, 8.0f, 1.5f);

        // 2. Green "SOUNDID VERIFIED" badge
        auto badgeBounds = cardBounds.removeFromTop(45).withSizeKeepingCentre(140.0f, 26.0f).translated(0, 18);
        g.setColour(accentGreen.withAlpha(0.08f));
        g.fillRoundedRectangle(badgeBounds, 4.0f);
        g.setColour(accentGreen);
        g.drawRoundedRectangle(badgeBounds, 4.0f, 1.2f);

        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("SOUNDID VERIFIED", badgeBounds, juce::Justification::centred);

        // 3. Acoustic Telemetry Text
        auto textBounds = cardBounds.reduced(24.0f).removeFromLeft(cardBounds.getWidth() * 0.52f).translated(10, 15);
        g.setColour(textPrimary);
        g.setFont(juce::FontOptions(13.0f, juce::Font::plain));
        g.drawFittedText(metricsText, textBounds.toNearestInt(), juce::Justification::topLeft, 10);

        // 4. "Visual DNA" Spectral Graph Preview on the right side
        auto graphBounds = cardBounds.reduced(24.0f).removeFromRight(cardBounds.getWidth() * 0.42f).withHeight(115).translated(-10, 25);
        g.setColour(textPrimary.withAlpha(0.04f));
        g.fillRoundedRectangle(graphBounds, 4.0f);
        g.setColour(borderSubtle);
        g.drawRoundedRectangle(graphBounds, 4.0f, 1.0f);

        // Simulated Catmull-Rom waveform / spectrum preview
        juce::Path path;
        path.startNewSubPath(graphBounds.getX(), graphBounds.getBottom() - 12.0f);

        const int numPoints = 24;
        const float dx = graphBounds.getWidth() / static_cast<float>(numPoints);
        for (int i = 1; i <= numPoints; ++i)
        {
            float x = graphBounds.getX() + (static_cast<float>(i) * dx);
            float coeff = std::sin(static_cast<float>(i) * 0.45f) * std::cos(static_cast<float>(i) * 0.22f);
            float y = graphBounds.getCentreY() + (coeff * 36.0f) + 8.0f;

            if (x > graphBounds.getRight()) x = graphBounds.getRight();
            path.lineTo(x, juce::jlimit(graphBounds.getY() + 6.0f, graphBounds.getBottom() - 6.0f, y));
        }

        g.setColour(accentGreen.withAlpha(0.75f));
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

        // Translucent gradient fill under curve
        path.lineTo(graphBounds.getRight(), graphBounds.getBottom() - 1.0f);
        path.lineTo(graphBounds.getX(), graphBounds.getBottom() - 1.0f);
        path.closeSubPath();
        g.setColour(accentGreen.withAlpha(0.06f));
        g.fillPath(path);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Header titles
        titleLabel.setBounds(bounds.removeFromTop(36).withSizeKeepingCentre(bounds.getWidth(), 25).translated(0, 20));
        subtitleLabel.setBounds(bounds.removeFromTop(26).withSizeKeepingCentre(bounds.getWidth(), 20).translated(0, 20));

        // Bottom action deck inside certification card (146px height)
        auto innerBottom = bounds.withSizeKeepingCentre(static_cast<int>(bounds.getWidth() * 0.84f), 146).translated(0, 105);

        // Row 1: PRIMARY ACTION - 1-Click Export (Dominant, 36px)
        auto buttonRow1 = innerBottom.removeFromTop(36);
        exportButton.setBounds(buttonRow1);

        innerBottom.removeFromTop(8);

        // Row 2: REVIEW ACTIONS - Preview & A/B Verification (32px)
        auto auditionRow = innerBottom.removeFromTop(32);
        if (!isAuditioning)
        {
            const int halfRev = (auditionRow.getWidth() - 8) / 2;
            auditionToggleButton.setBounds(auditionRow.removeFromLeft(halfRev));
            auditionRow.removeFromLeft(8);
            btnVerifyAb.setBounds(auditionRow.removeFromLeft(halfRev));
        }
        else
        {
            auditionToggleButton.setBounds(auditionRow.removeFromLeft(200));
            auditionRow.removeFromLeft(8);
            btnVerifyAb.setBounds(auditionRow.removeFromLeft(180));
            auditionRow.removeFromLeft(10);
            lblCutoff.setBounds(auditionRow.removeFromLeft(40));
            auditionRow.removeFromLeft(2);
            sliderCutoff.setBounds(auditionRow.removeFromLeft(65));
            auditionRow.removeFromLeft(6);
            lblResonance.setBounds(auditionRow.removeFromLeft(36));
            auditionRow.removeFromLeft(2);
            sliderResonance.setBounds(auditionRow.removeFromLeft(65));
            auditionRow.removeFromLeft(6);
            comboWaveform.setBounds(auditionRow);
        }

        innerBottom.removeFromTop(8);

        // Row 3: UTILITIES (Left) & EXTERNAL (Right) (30px)
        auto utilityRow = innerBottom.removeFromTop(30);
        openFolderButton.setBounds(utilityRow.removeFromLeft(140));
        utilityRow.removeFromLeft(8);
        viewHtmlButton.setBounds(utilityRow.removeFromLeft(140));

        publishCloudButton.setBounds(utilityRow.removeFromRight(200));

        innerBottom.removeFromTop(6);
        statusLabel.setBounds(innerBottom.removeFromTop(20));

        applyButtonStyling();
    }

private:
    void timerCallback() override
    {
        stopTimer();
        exportButton.setButtonText(juce::String::fromUTF8(u8"\u26a1 EXPORT PRODUCTION PACKAGE (1-CLICK)"));
        publishCloudButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x98\x81 PUBLISH TO CLOUD (API)")));
        applyButtonStyling();
        repaint();
    }

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton exportButton;
    juce::TextButton publishCloudButton;
    juce::TextButton openFolderButton;
    juce::TextButton viewHtmlButton;
    juce::TextButton auditionToggleButton;
    juce::TextButton btnVerifyAb;
    juce::Label lblCutoff;
    juce::Slider sliderCutoff;
    juce::Label lblResonance;
    juce::Slider sliderResonance;
    juce::ComboBox comboWaveform;
    bool isAuditioning { false };
    juce::Label statusLabel;
    juce::String metricsText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportReportPanel)
};

} // namespace abdaudiolab::gui
