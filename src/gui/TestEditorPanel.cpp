#include "TestEditorPanel.h"
#include "../core/AutoTestPresetEngine.h"
#include <algorithm>

namespace abdaudiolab::gui
{

TestEditorPanel::TestEditorPanel()
{
    // Section 1: Presets & Test Name
    lblPresetSelector.setText("1. PRESET CONFIGURATIONS (QUICK SETUP)", juce::dontSendNotification);
    lblPresetSelector.setFont(AppTheme::fontBold(10.5f));
    lblPresetSelector.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblPresetSelector);

    comboPresets.setTooltip("Preset Configurations - Select a standard measurement template for quick setup");
    comboPresets.onChange = [this] {
        int idx = comboPresets.getSelectedId() - 1;
        if (idx >= 0 && onPresetSelected) onPresetSelected(idx);
    };
    addAndMakeVisible(comboPresets);

    lblTestName.setText("Test Name:", juce::dontSendNotification);
    lblTestName.setFont(AppTheme::fontBold(10.5f));
    lblTestName.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblTestName);

    txtTestName.setTooltip("Test Name - Descriptive title for this measurement routine");
    txtTestName.onTextChange = [this] {
        currentConfig.testName = txtTestName.getText();
        if (onConfigChanged) onConfigChanged();
    };
    addAndMakeVisible(txtTestName);

    lblStimulusType.setText("Stimulus Type:", juce::dontSendNotification);
    lblStimulusType.setFont(AppTheme::fontBold(10.5f));
    lblStimulusType.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblStimulusType);

    comboStimulusType.setTooltip("Stimulus Type - Excitation signal fed into hardware (Log Sweep, Ramp, Sine, Pulses, Noise)");
    comboStimulusType.addItem("Exponential Log-Farina Sweep (Standard for Filters/EQ)", 1);
    comboStimulusType.addItem("Amplitude Ramp (Saturation & Non-Linear Knees)", 2);
    comboStimulusType.addItem("Sync Pulses (ADSR Gate & Timing)", 3);
    comboStimulusType.addItem("Step Impulse / Dirac Delta (Transient & Phase Calibration)", 4);
    comboStimulusType.addItem("1kHz Sine Wave (Saturator & Harmonic Distortion THD)", 5);
    comboStimulusType.addItem("White Noise Burst (Statistical Noise Floor & Broad Band)", 6);
    comboStimulusType.addItem("NAM / RTNeural Calibration (Sync, Chirp, Noise, Multitone)", 7);

    comboStimulusType.onChange = [this] {
        int sId = comboStimulusType.getSelectedId();
        switch (sId)
        {
            case 1:
                currentConfig.stimulusType = audio::StimulusType::LogFarinaSweep;
                lblStimulusDesc.setText("Log Sweep: Best SNR, precise 20Hz-20kHz response, extracts THD", juce::dontSendNotification);
                break;
            case 2:
                currentConfig.stimulusType = audio::StimulusType::AmplitudeRamp;
                lblStimulusDesc.setText("Amplitude Ramp: Non-linear saturation and compression knees", juce::dontSendNotification);
                break;
            case 3:
                currentConfig.stimulusType = audio::StimulusType::SyncPulses3;
                lblStimulusDesc.setText("Sync Pulses: Gate step pulses for ADSR envelope timings", juce::dontSendNotification);
                break;
            case 4:
                currentConfig.stimulusType = audio::StimulusType::DiracDelta;
                lblStimulusDesc.setText("Step Impulse (Dirac Delta): Single 0 dBfs sample for transient/phase IR", juce::dontSendNotification);
                break;
            case 5:
                currentConfig.stimulusType = audio::StimulusType::SineWave1kHz;
                lblStimulusDesc.setText("1kHz Sine Wave: Pure tone for harmonic distortion (THD) and saturation", juce::dontSendNotification);
                break;
            case 6:
                currentConfig.stimulusType = audio::StimulusType::WhiteNoise;
                lblStimulusDesc.setText("White Noise: Uniform energy density across all frequencies", juce::dontSendNotification);
                break;
            case 7:
                currentConfig.stimulusType = audio::StimulusType::NamCalibration;
                lblStimulusDesc.setText("NAM Calibration: Multi-stage neural stimulus with sample-accurate sync pulses", juce::dontSendNotification);
                break;
            default: break;
        }
        if (onConfigChanged) onConfigChanged();
    };
    addAndMakeVisible(comboStimulusType);

    lblStimulusDesc.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::italic));
    lblStimulusDesc.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblStimulusDesc);

    // Section 2: Burst Duration & Capture Mode
    lblDurationSection.setText("2. SIGNAL BURST DURATION & CAPTURE MODE", juce::dontSendNotification);
    lblDurationSection.setFont(AppTheme::fontBold(10.5f));
    lblDurationSection.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblDurationSection);

    comboDurationPreset.addItem("Ultra-Fast (0.25 sec per point)", 1);
    comboDurationPreset.addItem("Fast Standard (0.50 sec per point)", 2);
    comboDurationPreset.addItem("High Precision (1.00 sec per point)", 3);
    comboDurationPreset.addItem("Ultra High Precision (2.00 sec per point)", 4);
    comboDurationPreset.addItem("Long Tail (4.00 sec per point)", 5);
    comboDurationPreset.addItem("Custom Duration (Manual)...", 99);
    comboDurationPreset.setTooltip("Burst Duration & Capture Speed - Select stimulus duration per evaluation point");

    comboDurationPreset.onChange = [this] {
        int id = comboDurationPreset.getSelectedId();
        if (id == 1) { currentConfig.burstDurationSec = 0.25f; txtManualDuration.setEnabled(false); }
        else if (id == 2) { currentConfig.burstDurationSec = 0.50f; txtManualDuration.setEnabled(false); }
        else if (id == 3) { currentConfig.burstDurationSec = 1.00f; txtManualDuration.setEnabled(false); }
        else if (id == 4) { currentConfig.burstDurationSec = 2.00f; txtManualDuration.setEnabled(false); }
        else if (id == 5) { currentConfig.burstDurationSec = 4.00f; txtManualDuration.setEnabled(false); }
        else if (id == 99)
        {
            txtManualDuration.setEnabled(true);
            currentConfig.burstDurationSec = std::max(0.1f, txtManualDuration.getText().getFloatValue());
        }
        txtManualDuration.setText(juce::String(currentConfig.burstDurationSec, 2), juce::dontSendNotification);
        updateEstimatedTime();
        if (onConfigChanged) onConfigChanged();
    };
    addAndMakeVisible(comboDurationPreset);

    lblManualDuration.setText("Custom Duration:", juce::dontSendNotification);
    lblManualDuration.setFont(AppTheme::fontBold(10.0f));
    lblManualDuration.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblManualDuration);

    txtManualDuration.setInputRestrictions(5, "0123456789.");
    txtManualDuration.setTooltip("Custom Duration (seconds) - Set arbitrary stimulus evaluation time per point");
    txtManualDuration.setJustification(juce::Justification::centred);
    txtManualDuration.onTextChange = [this] {
        if (comboDurationPreset.getSelectedId() == 99)
        {
            currentConfig.burstDurationSec = std::max(0.1f, txtManualDuration.getText().getFloatValue());
            updateEstimatedTime();
            if (onConfigChanged) onConfigChanged();
        }
    };
    addAndMakeVisible(txtManualDuration);

    lblSecondsUnit.setText("sec", juce::dontSendNotification);
    lblSecondsUnit.setFont(AppTheme::fontRegular(10.5f));
    lblSecondsUnit.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblSecondsUnit);

    btnAdaptiveTail.setButtonText("Adaptive Auto-Tail Silence Cutoff (for ADSR / Reverb)");
    btnAdaptiveTail.setColour(juce::ToggleButton::textColourId, AppTheme::TextPrimary);
    btnAdaptiveTail.setColour(juce::ToggleButton::tickColourId, AppTheme::AccentActive);
    btnAdaptiveTail.setTooltip("Automatically stops capture when envelope or reverb tail drops below -60 dBfs");
    btnAdaptiveTail.onClick = [this] {
        currentConfig.captureMode = btnAdaptiveTail.getToggleState() ? "ADAPTIVE_ENVELOPE" : "FIXED_TIME";
        updateEstimatedTime();
        if (onConfigChanged) onConfigChanged();
    };
    addAndMakeVisible(btnAdaptiveTail);

    // Section 3: Matrix Resolution Table
    lblMatrixSection.setText("3. MEASUREMENT MATRIX RESOLUTION (SET STEPS PER CONTROL)", juce::dontSendNotification);
    lblMatrixSection.setFont(AppTheme::fontBold(10.5f));
    lblMatrixSection.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblMatrixSection);

    matrixTableComp.onControlsChanged = [this] {
        currentConfig.controls = matrixTableComp.getControls();
        updateEstimatedTime();
        if (onConfigChanged) onConfigChanged();
    };
    addAndMakeVisible(matrixTableComp);

    // Section 4: Estimation Card
    addAndMakeVisible(estimationCard);
}

void TestEditorPanel::updateTheme()
{
    matrixTableComp.updateTheme();
    lblPresetSelector.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblTestName.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblStimulusType.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblStimulusDesc.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    lblDurationSection.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblManualDuration.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblSecondsUnit.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblMatrixSection.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    txtTestName.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtTestName.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    txtManualDuration.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtManualDuration.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    estimationCard.repaint();
    repaint();
}

void TestEditorPanel::setPresetSelectorVisible(bool visible)
{
    lblPresetSelector.setVisible(visible);
    comboPresets.setVisible(visible);
    resized();
}

void TestEditorPanel::setPresetOptions(const std::vector<juce::String>& presetNames)
{
    comboPresets.clear(juce::dontSendNotification);
    for (size_t i = 0; i < presetNames.size(); ++i)
    {
        comboPresets.addItem(presetNames[i], static_cast<int>(i + 1));
    }
}

void TestEditorPanel::populateWithAutoTestPresets()
{
    auto allPresets = core::AutoTestPresetEngine::getAllPresets();
    std::vector<juce::String> names;
    names.reserve(allPresets.size());
    for (const auto& p : allPresets)
    {
        names.push_back("[" + juce::String(p.badgeText) + "] " + juce::String(p.displayName));
    }
    setPresetOptions(names);
    setPresetSelectorVisible(true);

    onPresetSelected = [this, allPresets](int idx) {
        if (idx >= 0 && idx < static_cast<int>(allPresets.size()))
        {
            const auto& rec = allPresets[static_cast<size_t>(idx)];
            currentConfig.testName = juce::String(rec.displayName);
            currentConfig.stimulusType = rec.stimulusType;
            currentConfig.burstDurationSec = rec.burstDurationSec;
            currentConfig.captureMode = juce::String(rec.captureMode);
            currentConfig.silenceThresholdDb = rec.silenceThresholdDb;

            // Update UI widgets
            setConfiguration(currentConfig);
            if (onConfigChanged) onConfigChanged();
        }
    };
}

void TestEditorPanel::setConfiguration(const TestConfiguration& config)
{
    currentConfig = config;

    txtTestName.setText(currentConfig.testName, juce::dontSendNotification);

    switch (currentConfig.stimulusType)
    {
        case audio::StimulusType::LogFarinaSweep: comboStimulusType.setSelectedId(1, juce::dontSendNotification); break;
        case audio::StimulusType::AmplitudeRamp:  comboStimulusType.setSelectedId(2, juce::dontSendNotification); break;
        case audio::StimulusType::SyncPulses3:    comboStimulusType.setSelectedId(3, juce::dontSendNotification); break;
        case audio::StimulusType::DiracDelta:     comboStimulusType.setSelectedId(4, juce::dontSendNotification); break;
        case audio::StimulusType::SineWave1kHz:   comboStimulusType.setSelectedId(5, juce::dontSendNotification); break;
        case audio::StimulusType::WhiteNoise:     comboStimulusType.setSelectedId(6, juce::dontSendNotification); break;
        case audio::StimulusType::NamCalibration: comboStimulusType.setSelectedId(7, juce::dontSendNotification); break;
        default: comboStimulusType.setSelectedId(1, juce::dontSendNotification); break;
    }

    float dur = currentConfig.burstDurationSec;
    if (std::abs(dur - 0.25f) < 0.01f) comboDurationPreset.setSelectedId(1, juce::dontSendNotification);
    else if (std::abs(dur - 0.50f) < 0.01f) comboDurationPreset.setSelectedId(2, juce::dontSendNotification);
    else if (std::abs(dur - 1.00f) < 0.01f) comboDurationPreset.setSelectedId(3, juce::dontSendNotification);
    else if (std::abs(dur - 2.00f) < 0.01f) comboDurationPreset.setSelectedId(4, juce::dontSendNotification);
    else if (std::abs(dur - 4.00f) < 0.01f) comboDurationPreset.setSelectedId(5, juce::dontSendNotification);
    else comboDurationPreset.setSelectedId(99, juce::dontSendNotification);

    txtManualDuration.setText(juce::String(dur, 2), juce::dontSendNotification);
    txtManualDuration.setEnabled(comboDurationPreset.getSelectedId() == 99);

    btnAdaptiveTail.setToggleState(currentConfig.captureMode == "ADAPTIVE_ENVELOPE", juce::dontSendNotification);

    matrixTableComp.setControls(currentConfig.controls);
    updateEstimatedTime();
    resized();
}

void TestEditorPanel::updateEstimatedTime()
{
    int totalPts = currentConfig.getTotalMeasurementPoints();
    float totalSec = static_cast<float>(totalPts) * currentConfig.burstDurationSec;
    estimationCard.setEstimation(totalPts, totalSec);
}

int TestEditorPanel::getPreferredHeight() const
{
    int y = 0;
    if (lblPresetSelector.isVisible()) y += 54;
    y += 50; // Test Name
    y += 80; // Stimulus type + description + margin
    y += 86; // Duration & Adaptive Tail
    y += 14; // Divider
    y += 24; // Matrix Header

    y += matrixTableComp.getPreferredHeight();
    y += 56; // Estimation Card
    y += 16; // Bottom margin
    return y;
}

void TestEditorPanel::paint(juce::Graphics& g)
{
    // Draw subtle divider line before Section 3
    if (lblMatrixSection.getY() > 20)
    {
        g.setColour(AppTheme::BorderSubtle);
        g.drawHorizontalLine(lblMatrixSection.getY() - 10, 0.0f, static_cast<float>(getWidth()));
    }
}

void TestEditorPanel::resized()
{
    int contentW = getWidth();
    int y = 0;

    if (lblPresetSelector.isVisible())
    {
        lblPresetSelector.setBounds(0, y, contentW, 16);
        y += 18;
        comboPresets.setBounds(0, y, contentW, 32);
        y += 36;
    }

    lblTestName.setBounds(0, y, contentW, 16);
    txtTestName.setBounds(0, y + 18, contentW, 30);
    y += 52;

    lblStimulusType.setBounds(0, y, contentW, 16);
    comboStimulusType.setBounds(0, y + 18, contentW, 30);
    lblStimulusDesc.setBounds(0, y + 52, contentW, 16);
    y += 82; // 14px clean vertical margin between stimulus description and section 2 header

    lblDurationSection.setBounds(0, y, contentW, 16);
    y += 20;

    int rightColW = 180;
    int leftColW = std::max(180, contentW - rightColW - 12);
    comboDurationPreset.setBounds(0, y, leftColW, 30);

    lblManualDuration.setBounds(leftColW + 12, y - 18, rightColW, 16);
    txtManualDuration.setBounds(leftColW + 12, y, rightColW - 40, 30);
    lblSecondsUnit.setBounds(leftColW + 12 + rightColW - 36, y, 32, 30);
    y += 36;

    btnAdaptiveTail.setBounds(0, y, contentW, 24);
    y += 36;

    lblMatrixSection.setBounds(0, y, contentW, 16);
    y += 22;

    int tableH = matrixTableComp.getPreferredHeight();
    matrixTableComp.setBounds(0, y, contentW, tableH);
    y += tableH + 12;

    estimationCard.setBounds(0, y, contentW, 46);
}

} // namespace abdaudiolab::gui
