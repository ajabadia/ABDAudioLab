#include "TestEditorPanel.h"
#include <algorithm>

namespace abdaudiolab::gui
{

TestEditorPanel::TestEditorPanel()
{
    lblPresetSelector.setText("1. PRESET CONFIGURATIONS (QUICK SETUP)", juce::dontSendNotification);
    lblPresetSelector.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblPresetSelector.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblPresetSelector);

    comboPresets.setColour(juce::ComboBox::backgroundColourId, SoundIdTheme::pillWhiteBg);
    comboPresets.setColour(juce::ComboBox::textColourId, SoundIdTheme::textPrimary);
    comboPresets.setColour(juce::ComboBox::outlineColourId, SoundIdTheme::borderCard);
    comboPresets.onChange = [this] {
        int idx = comboPresets.getSelectedId() - 1;
        if (idx >= 0 && onPresetSelected) onPresetSelected(idx);
    };
    addAndMakeVisible(comboPresets);

    lblTestName.setText("Test Name:", juce::dontSendNotification);
    lblTestName.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    lblTestName.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblTestName);

    txtTestName.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
    txtTestName.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    txtTestName.setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
    txtTestName.onTextChange = [this] {
        currentConfig.testName = txtTestName.getText();
        if (onConfigChanged) onConfigChanged();
    };
    addAndMakeVisible(txtTestName);

    lblStimulusType.setText("Stimulus Type:", juce::dontSendNotification);
    lblStimulusType.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    lblStimulusType.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblStimulusType);

    comboStimulusType.addItem("Exponential Log-Farina Sweep (Standard for Filters/EQ)", 1);
    comboStimulusType.addItem("Amplitude Ramp (Saturation & Non-Linear Knees)", 2);
    comboStimulusType.addItem("Sync Pulses (ADSR Gate & Timing)", 3);
    comboStimulusType.addItem("Step Impulse / Dirac Delta (Transient & Phase Calibration)", 4);
    comboStimulusType.addItem("1kHz Sine Wave (Saturator & Harmonic Distortion THD)", 5);
    comboStimulusType.addItem("White Noise Burst (Statistical Noise Floor & Broad Band)", 6);
    comboStimulusType.addItem("NAM / RTNeural Calibration (Sync, Chirp, Noise, Multitone)", 7);

    comboStimulusType.setColour(juce::ComboBox::backgroundColourId, SoundIdTheme::pillWhiteBg);
    comboStimulusType.setColour(juce::ComboBox::textColourId, SoundIdTheme::textPrimary);
    comboStimulusType.setColour(juce::ComboBox::outlineColourId, SoundIdTheme::borderCard);

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

    lblStimulusDesc.setFont(juce::FontOptions(9.5f, juce::Font::italic));
    lblStimulusDesc.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblStimulusDesc);

    lblDurationSection.setText("2. SIGNAL BURST DURATION & CAPTURE MODE", juce::dontSendNotification);
    lblDurationSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblDurationSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblDurationSection);

    comboDurationPreset.addItem("Ultra-Fast (0.25 sec per point)", 1);
    comboDurationPreset.addItem("Fast Standard (0.50 sec per point)", 2);
    comboDurationPreset.addItem("High Precision (1.00 sec per point)", 3);
    comboDurationPreset.addItem("Ultra High Precision (2.00 sec per point)", 4);
    comboDurationPreset.addItem("Long Tail (4.00 sec per point)", 5);
    comboDurationPreset.addItem("Custom Duration (Manual)...", 99);

    comboDurationPreset.setColour(juce::ComboBox::backgroundColourId, SoundIdTheme::pillWhiteBg);
    comboDurationPreset.setColour(juce::ComboBox::textColourId, SoundIdTheme::textPrimary);
    comboDurationPreset.setColour(juce::ComboBox::outlineColourId, SoundIdTheme::borderCard);

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

    lblManualDuration.setText("Custom Duration (s):", juce::dontSendNotification);
    lblManualDuration.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    lblManualDuration.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblManualDuration);

    txtManualDuration.setInputRestrictions(5, "0123456789.");
    txtManualDuration.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
    txtManualDuration.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    txtManualDuration.setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
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
    lblSecondsUnit.setFont(juce::FontOptions(10.5f));
    lblSecondsUnit.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblSecondsUnit);

    btnAdaptiveTail.setButtonText("Adaptive Auto-Tail Silence Cutoff (for ADSR / Reverb)");
    btnAdaptiveTail.setColour(juce::ToggleButton::textColourId, SoundIdTheme::textPrimary);
    btnAdaptiveTail.setTooltip("Automatically stops capture when envelope or reverb tail drops below -60 dBfs");
    btnAdaptiveTail.onClick = [this] {
        currentConfig.captureMode = btnAdaptiveTail.getToggleState() ? "ADAPTIVE_ENVELOPE" : "FIXED_TIME";
        updateEstimatedTime();
        if (onConfigChanged) onConfigChanged();
    };
    addAndMakeVisible(btnAdaptiveTail);

    lblMatrixSection.setText("3. MEASUREMENT MATRIX RESOLUTION (SET STEPS PER CONTROL)", juce::dontSendNotification);
    lblMatrixSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblMatrixSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblMatrixSection);

    lblHeaderParam.setText("CONTROL / PARAMETER", juce::dontSendNotification);
    lblHeaderParam.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    lblHeaderParam.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblHeaderParam);

    lblHeaderResolution.setText("STEP RESOLUTION", juce::dontSendNotification);
    lblHeaderResolution.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    lblHeaderResolution.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblHeaderResolution);

    lblHeaderCustom.setText("STEPS", juce::dontSendNotification);
    lblHeaderCustom.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    lblHeaderCustom.setJustificationType(juce::Justification::centred);
    lblHeaderCustom.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblHeaderCustom);

    lblHeaderMin.setText("MIN %", juce::dontSendNotification);
    lblHeaderMin.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    lblHeaderMin.setJustificationType(juce::Justification::centred);
    lblHeaderMin.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblHeaderMin);

    lblHeaderMax.setText("MAX %", juce::dontSendNotification);
    lblHeaderMax.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    lblHeaderMax.setJustificationType(juce::Justification::centred);
    lblHeaderMax.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblHeaderMax);

    lblHeaderOrder.setText("ORDER", juce::dontSendNotification);
    lblHeaderOrder.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    lblHeaderOrder.setJustificationType(juce::Justification::centred);
    lblHeaderOrder.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblHeaderOrder);

    lblTestSummary.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblTestSummary.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    addAndMakeVisible(lblTestSummary);
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

    rebuildControlRows();
}

void TestEditorPanel::rebuildControlRows()
{
    rowWidgets.clear();

    for (size_t rowIdx = 0; rowIdx < currentConfig.controls.size(); ++rowIdx)
    {
        const auto& ctrl = currentConfig.controls[rowIdx];
        RowWidgets row;

        row.icon = std::make_unique<ControlIconComponent>(ctrl.type);
        addAndMakeVisible(row.icon.get());

        row.label = std::make_unique<juce::Label>();
        row.label->setText(ctrl.name, juce::dontSendNotification);
        row.label->setFont(juce::FontOptions(10.5f, juce::Font::bold));
        row.label->setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
        addAndMakeVisible(row.label.get());

        row.btnUp = std::make_unique<juce::TextButton>(juce::String::fromUTF8(u8"▲"));
        row.btnUp->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        row.btnUp->setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        row.btnUp->setEnabled(rowIdx > 0);
        row.btnUp->onClick = [this, rowIdx] {
            if (rowIdx > 0)
            {
                std::swap(currentConfig.controls[rowIdx], currentConfig.controls[rowIdx - 1]);
                rebuildControlRows();
                if (onConfigChanged) onConfigChanged();
            }
        };
        addAndMakeVisible(row.btnUp.get());

        row.btnDown = std::make_unique<juce::TextButton>(juce::String::fromUTF8(u8"▼"));
        row.btnDown->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        row.btnDown->setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        row.btnDown->setEnabled(rowIdx + 1 < currentConfig.controls.size());
        row.btnDown->onClick = [this, rowIdx] {
            if (rowIdx + 1 < currentConfig.controls.size())
            {
                std::swap(currentConfig.controls[rowIdx], currentConfig.controls[rowIdx + 1]);
                rebuildControlRows();
                if (onConfigChanged) onConfigChanged();
            }
        };
        addAndMakeVisible(row.btnDown.get());

        row.combo = std::make_unique<juce::ComboBox>();
        row.combo->setColour(juce::ComboBox::backgroundColourId, SoundIdTheme::pillWhiteBg);
        row.combo->setColour(juce::ComboBox::textColourId, SoundIdTheme::textPrimary);
        row.combo->setColour(juce::ComboBox::outlineColourId, SoundIdTheme::borderCard);

        row.combo->addItem("Fixed (1 step - single reference)", 1);
        row.combo->addItem("3 Steps (0%, 50%, 100%)", 3);
        row.combo->addItem("5 Steps (Standard: 0, 25, 50, 75, 100%)", 5);
        row.combo->addItem("8 Steps (Detailed: 8 steps)", 8);
        row.combo->addItem("16 Steps (High-Res: 16 steps)", 16);
        row.combo->addItem("32 Steps (Ultra High-Res: 32 steps)", 32);
        row.combo->addItem("64 Steps (Extreme: 64 steps)", 64);
        row.combo->addItem("Custom Steps (Manual)...", 99);
        addAndMakeVisible(row.combo.get());

        row.txtCustomSteps = std::make_unique<juce::TextEditor>();
        row.txtCustomSteps->setInputRestrictions(3, "0123456789");
        row.txtCustomSteps->setJustification(juce::Justification::centred);
        row.txtCustomSteps->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
        row.txtCustomSteps->setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
        row.txtCustomSteps->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
        addAndMakeVisible(row.txtCustomSteps.get());

        int initialStep = (ctrl.steps > 0) ? ctrl.steps : 1;
        bool isStandard = (initialStep == 1 || initialStep == 3 || initialStep == 5 || 
                           initialStep == 8 || initialStep == 16 || initialStep == 32 || initialStep == 64);

        if (isStandard)
        {
            row.combo->setSelectedId(initialStep, juce::dontSendNotification);
            row.txtCustomSteps->setText(juce::String(initialStep), juce::dontSendNotification);
            row.txtCustomSteps->setEnabled(false);
            row.txtCustomSteps->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::bgCardHover);
            row.txtCustomSteps->setColour(juce::TextEditor::textColourId, SoundIdTheme::textSecondary);
            row.txtCustomSteps->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderSubtle);
        }
        else
        {
            row.combo->setSelectedId(99, juce::dontSendNotification);
            row.txtCustomSteps->setText(juce::String(initialStep), juce::dontSendNotification);
            row.txtCustomSteps->setEnabled(true);
            row.txtCustomSteps->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
            row.txtCustomSteps->setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
            row.txtCustomSteps->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
        }

        row.txtMinPct = std::make_unique<juce::TextEditor>();
        row.txtMinPct->setInputRestrictions(5, "0123456789.");
        row.txtMinPct->setText(juce::String(ctrl.minPct, 1), juce::dontSendNotification);
        row.txtMinPct->setJustification(juce::Justification::centred);
        row.txtMinPct->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
        row.txtMinPct->setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
        row.txtMinPct->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
        addAndMakeVisible(row.txtMinPct.get());

        row.txtMaxPct = std::make_unique<juce::TextEditor>();
        row.txtMaxPct->setInputRestrictions(5, "0123456789.");
        row.txtMaxPct->setText(juce::String(ctrl.maxPct, 1), juce::dontSendNotification);
        row.txtMaxPct->setJustification(juce::Justification::centred);
        row.txtMaxPct->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
        row.txtMaxPct->setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
        row.txtMaxPct->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
        row.txtMaxPct->setEnabled(initialStep > 1);
        addAndMakeVisible(row.txtMaxPct.get());

        auto updateStepAndBounds = [this, rowIdx] {
            if (rowIdx < rowWidgets.size() && rowIdx < currentConfig.controls.size())
            {
                auto& c = currentConfig.controls[rowIdx];
                auto& w = rowWidgets[rowIdx];

                int sId = w.combo->getSelectedId();
                if (sId == 99)
                {
                    w.txtCustomSteps->setEnabled(true);
                    w.txtCustomSteps->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
                    w.txtCustomSteps->setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
                    w.txtCustomSteps->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
                    int cVal = w.txtCustomSteps->getText().getIntValue();
                    c.steps = std::max(1, cVal);
                }
                else
                {
                    w.txtCustomSteps->setText(juce::String(sId), juce::dontSendNotification);
                    w.txtCustomSteps->setEnabled(false);
                    w.txtCustomSteps->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::bgCardHover);
                    w.txtCustomSteps->setColour(juce::TextEditor::textColourId, SoundIdTheme::textSecondary);
                    w.txtCustomSteps->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderSubtle);
                    c.steps = sId;
                }

                float minV = std::clamp(w.txtMinPct->getText().getFloatValue(), 0.0f, 100.0f);
                c.minPct = minV;

                if (c.steps == 1)
                {
                    c.maxPct = minV;
                    w.txtMaxPct->setText(juce::String(minV, 1), juce::dontSendNotification);
                    w.txtMaxPct->setEnabled(false);
                    w.txtMaxPct->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::bgCardHover);
                    w.txtMaxPct->setColour(juce::TextEditor::textColourId, SoundIdTheme::textSecondary);
                    w.txtMaxPct->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderSubtle);
                }
                else
                {
                    w.txtMaxPct->setEnabled(true);
                    w.txtMaxPct->setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::pillWhiteBg);
                    w.txtMaxPct->setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
                    w.txtMaxPct->setColour(juce::TextEditor::outlineColourId, SoundIdTheme::borderCard);
                    float maxV = std::clamp(w.txtMaxPct->getText().getFloatValue(), minV, 100.0f);
                    c.maxPct = maxV;
                }

                updateEstimatedTime();
                if (onConfigChanged) onConfigChanged();
            }
        };

        row.combo->onChange = updateStepAndBounds;
        row.txtCustomSteps->onTextChange = updateStepAndBounds;
        row.txtMinPct->onTextChange = updateStepAndBounds;
        row.txtMaxPct->onTextChange = updateStepAndBounds;

        rowWidgets.push_back(std::move(row));
    }

    updateEstimatedTime();
    resized();
}

void TestEditorPanel::updateEstimatedTime()
{
    int totalPts = currentConfig.getTotalMeasurementPoints();
    float totalSec = static_cast<float>(totalPts) * currentConfig.burstDurationSec;
    int mins = static_cast<int>(totalSec) / 60;
    int secs = static_cast<int>(totalSec) % 60;

    juce::String text = "ESTIMATED PLAN: " + juce::String(totalPts) + " points total (~" +
                        juce::String(mins) + "m " + juce::String(secs) + "s duration)";
    lblTestSummary.setText(text, juce::dontSendNotification);
}

int TestEditorPanel::getPreferredHeight() const
{
    int y = 0;
    if (lblPresetSelector.isVisible()) y += 52;
    y += 48; // Test Name
    y += 64; // Stimulus
    y += 96; // Duration & Adaptive Tail
    y += 40; // Matrix Header
    y += static_cast<int>(rowWidgets.size()) * 32;
    y += 50; // Summary text + bottom margin
    return y;
}

void TestEditorPanel::resized()
{
    int padX = 0;
    int contentW = getWidth();
    int y = 0;

    if (lblPresetSelector.isVisible())
    {
        lblPresetSelector.setBounds(padX, y, contentW, 16);
        y += 18;
        comboPresets.setBounds(padX, y, contentW, 28);
        y += 34;
    }

    lblTestName.setBounds(padX, y, contentW, 16);
    txtTestName.setBounds(padX, y + 18, contentW, 26);
    y += 48;

    lblStimulusType.setBounds(padX, y, contentW, 16);
    comboStimulusType.setBounds(padX, y + 18, contentW, 26);
    lblStimulusDesc.setBounds(padX, y + 46, contentW, 14);
    y += 64;

    lblDurationSection.setBounds(padX, y, contentW, 16);
    y += 20;

    comboDurationPreset.setBounds(padX, y + 18, contentW - 190, 26);
    lblManualDuration.setBounds(padX + contentW - 180, y, 180, 16);
    txtManualDuration.setBounds(padX + contentW - 180, y + 18, 110, 26);
    lblSecondsUnit.setBounds(padX + contentW - 64, y + 18, 30, 26);
    y += 48;

    btnAdaptiveTail.setBounds(padX, y, contentW, 22);
    y += 28;

    lblMatrixSection.setBounds(padX, y, contentW, 16);
    y += 20;

    int comboColW = juce::jmax(150, contentW - 450);
    lblHeaderParam.setBounds(padX, y, 220, 16);
    lblHeaderResolution.setBounds(padX + 225, y, comboColW, 16);
    lblHeaderCustom.setBounds(padX + contentW - 215, y, 48, 16);
    lblHeaderMin.setBounds(padX + contentW - 160, y, 54, 16);
    lblHeaderMax.setBounds(padX + contentW - 100, y, 54, 16);
    lblHeaderOrder.setBounds(padX + contentW - 42, y, 42, 16);
    y += 18;

    for (size_t i = 0; i < rowWidgets.size(); ++i)
    {
        auto& w = rowWidgets[i];

        if (w.icon) w.icon->setBounds(padX, y + 6, 16, 16);
        w.label->setBounds(padX + 20, y + 4, 200, 20);

        w.combo->setBounds(padX + 225, y + 2, comboColW, 26);
        w.txtCustomSteps->setBounds(padX + contentW - 215, y + 2, 48, 26);

        w.txtMinPct->setBounds(padX + contentW - 160, y + 2, 54, 26);
        w.txtMaxPct->setBounds(padX + contentW - 100, y + 2, 54, 26);

        if (w.btnUp) w.btnUp->setBounds(padX + contentW - 38, y + 1, 32, 13);
        if (w.btnDown) w.btnDown->setBounds(padX + contentW - 38, y + 15, 32, 13);

        y += 32;
    }
    y += 8;

    lblTestSummary.setBounds(padX, y, contentW, 18);
}

} // namespace abdaudiolab::gui
