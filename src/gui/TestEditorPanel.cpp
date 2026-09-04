/**
 * @file TestEditorPanel.cpp
 * @brief Reusable test parameter configuration panel (stimulus, duration, and matrix controls).
 * @author ABDSynths
 * @date 2026
 */

#include "TestEditorPanel.h"
#include <algorithm>

namespace abdaudiolab::gui
{

namespace
{
enum ColumnId
{
    colIcon = 1,
    colParam = 2,
    colResolution = 3,
    colSteps = 4,
    colMin = 5,
    colMax = 6,
    colOrder = 7
};

bool isStandardStep(int step) noexcept
{
    return step == 1 || step == 3 || step == 5 || step == 8 || step == 16 || step == 32 || step == 64;
}
} // namespace

// ============================================================================
// OrderButtonsComponent
// ============================================================================
TestEditorPanel::OrderButtonsComponent::OrderButtonsComponent()
{
    btnUp.setTooltip("Move parameter up in sweep execution order");
    btnUp.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnUp.setColour(juce::TextButton::textColourOffId, AppTheme::TextSecondary);
    addAndMakeVisible(btnUp);

    btnDown.setTooltip("Move parameter down in sweep execution order");
    btnDown.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnDown.setColour(juce::TextButton::textColourOffId, AppTheme::TextSecondary);
    addAndMakeVisible(btnDown);
}

void TestEditorPanel::OrderButtonsComponent::resized()
{
    auto b = getLocalBounds();
    int h = b.getHeight() / 2;
    btnUp.setBounds(b.removeFromTop(h));
    btnDown.setBounds(b);
}

// ============================================================================
// EstimationCardComponent
// ============================================================================
TestEditorPanel::EstimationCardComponent::EstimationCardComponent()
{
}

void TestEditorPanel::EstimationCardComponent::setEstimation(int totalPoints, float totalSeconds)
{
    points = totalPoints;
    seconds = totalSeconds;
    repaint();
}

void TestEditorPanel::EstimationCardComponent::paint(juce::Graphics& g)
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

// ============================================================================
// TestEditorPanel
// ============================================================================
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

    matrixTable.setModel(this);
    matrixTable.setRowHeight(34);
    matrixTable.setHeaderHeight(26);
    matrixTable.setColour(juce::ListBox::backgroundColourId, AppTheme::SurfaceCard);
    matrixTable.setColour(juce::ListBox::outlineColourId, AppTheme::BorderSubtle);
    matrixTable.setOutlineThickness(1);

    auto& hdr = matrixTable.getHeader();
    hdr.addColumn("ICON", colIcon, 32, 28, 40, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("PARAMETER", colParam, 170, 120, 260, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("STEP RESOLUTION", colResolution, 150, 130, 220, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("STEPS", colSteps, 55, 45, 75, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("MIN %", colMin, 52, 45, 75, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("MAX %", colMax, 52, 45, 75, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("ORDER", colOrder, 46, 40, 60, juce::TableHeaderComponent::notSortable);

    addAndMakeVisible(matrixTable);

    // Section 4: Estimation Card
    addAndMakeVisible(estimationCard);
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

    matrixTable.updateContent();
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
    y += 66; // Stimulus
    y += 86; // Duration & Adaptive Tail
    y += 14; // Divider
    y += 24; // Matrix Header

    int numRows = static_cast<int>(currentConfig.controls.size());
    int tableContentH = 26 + std::max(1, numRows) * 34 + 6;
    int tableH = std::clamp(tableContentH, 94, 260);
    y += tableH;

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
    lblStimulusDesc.setBounds(0, y + 50, contentW, 14);
    y += 68;

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

    int numRows = static_cast<int>(currentConfig.controls.size());
    int tableContentH = 26 + std::max(1, numRows) * 34 + 6;
    int tableH = std::clamp(tableContentH, 94, 260);
    matrixTable.setBounds(0, y, contentW, tableH);
    y += tableH + 12;

    estimationCard.setBounds(0, y, contentW, 46);
}

// ============================================================================
// TableListBoxModel Implementation
// ============================================================================
int TestEditorPanel::getNumRows()
{
    return static_cast<int>(currentConfig.controls.size());
}

void TestEditorPanel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(AppTheme::SurfaceHover);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(AppTheme::BackgroundApp.withAlpha(0.60f));
    }
    else
    {
        g.fillAll(AppTheme::SurfaceCard);
    }

    g.setColour(AppTheme::BorderSubtle);
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
}

void TestEditorPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(currentConfig.controls.size()))
        return;

    const auto& ctrl = currentConfig.controls[static_cast<size_t>(rowNumber)];

    if (columnId == colParam)
    {
        g.setFont(AppTheme::fontBold(11.0f));
        g.setColour(AppTheme::TextPrimary);
        g.drawText(ctrl.name, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
    }
}

juce::Component* TestEditorPanel::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/,
                                                           juce::Component* existingComponentToUpdate)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(currentConfig.controls.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    const auto& ctrl = currentConfig.controls[static_cast<size_t>(rowNumber)];

    // 1. Column Icon
    if (columnId == colIcon)
    {
        ControlIconComponent* iconComp = nullptr;
        if (existingComponentToUpdate != nullptr)
            iconComp = dynamic_cast<ControlIconComponent*>(existingComponentToUpdate);
        else
            iconComp = new ControlIconComponent(ctrl.type);

        if (iconComp != nullptr)
            iconComp->setControlType(ctrl.type);

        return iconComp;
    }

    // 2. Column Parameter Name: painted directly in paintCell for zero widget overhead
    if (columnId == colParam)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    // 3. Column Step Resolution (ComboBox)
    if (columnId == colResolution)
    {
        juce::ComboBox* combo = nullptr;
        if (existingComponentToUpdate != nullptr)
            combo = dynamic_cast<juce::ComboBox*>(existingComponentToUpdate);
        else
        {
            combo = new juce::ComboBox();
            combo->setTooltip("Step Resolution - Preset number of points across sweep range");
            combo->addItem("Fixed (1 step - single reference)", 1);
            combo->addItem("3 Steps (0%, 50%, 100%)", 3);
            combo->addItem("5 Steps (Standard: 0, 25, 50, 75, 100%)", 5);
            combo->addItem("8 Steps (Detailed: 8 steps)", 8);
            combo->addItem("16 Steps (High-Res: 16 steps)", 16);
            combo->addItem("32 Steps (Ultra High-Res: 32 steps)", 32);
            combo->addItem("64 Steps (Extreme: 64 steps)", 64);
            combo->addItem("Custom Steps (Manual)...", 99);
        }

        // CRITICAL JUCE 8: Clear callback before mutating state to avoid firing on recycled row
        combo->onChange = nullptr;

        int step = (ctrl.steps > 0) ? ctrl.steps : 1;
        combo->setSelectedId(isStandardStep(step) ? step : 99, juce::dontSendNotification);

        // Rebind callback capturing the EXACT rowNumber
        combo->onChange = [this, rowNumber, combo] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(currentConfig.controls.size()))
            {
                auto& c = currentConfig.controls[static_cast<size_t>(rowNumber)];
                int sId = combo->getSelectedId();
                if (sId != 99)
                {
                    c.steps = sId;
                    if (c.steps == 1) c.maxPct = c.minPct;
                }
                matrixTable.updateContent();
                updateEstimatedTime();
                if (onConfigChanged) onConfigChanged();
            }
        };

        return combo;
    }

    // 4. Column Custom Steps (TextEditor)
    if (columnId == colSteps)
    {
        juce::TextEditor* editor = nullptr;
        if (existingComponentToUpdate != nullptr)
            editor = dynamic_cast<juce::TextEditor*>(existingComponentToUpdate);
        else
        {
            editor = new juce::TextEditor();
            editor->setInputRestrictions(3, "0123456789");
            editor->setTooltip("Step Count - Number of evaluation points for this control");
            editor->setJustification(juce::Justification::centred);
        }

        // CRITICAL JUCE 8: Clear callback before setting text
        editor->onTextChange = nullptr;

        int step = (ctrl.steps > 0) ? ctrl.steps : 1;
        editor->setText(juce::String(step), juce::dontSendNotification);

        bool isCustom = !isStandardStep(step);
        editor->setEnabled(isCustom);
        editor->setColour(juce::TextEditor::backgroundColourId, isCustom ? AppTheme::SurfaceCard : AppTheme::SurfaceHover);
        editor->setColour(juce::TextEditor::textColourId, isCustom ? AppTheme::TextPrimary : AppTheme::TextSecondary);

        // Rebind callback capturing EXACT rowNumber
        editor->onTextChange = [this, rowNumber, editor] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(currentConfig.controls.size()))
            {
                int val = std::max(1, editor->getText().getIntValue());
                currentConfig.controls[static_cast<size_t>(rowNumber)].steps = val;
                updateEstimatedTime();
                if (onConfigChanged) onConfigChanged();
            }
        };

        return editor;
    }

    // 5. Column Min % (TextEditor)
    if (columnId == colMin)
    {
        juce::TextEditor* editor = nullptr;
        if (existingComponentToUpdate != nullptr)
            editor = dynamic_cast<juce::TextEditor*>(existingComponentToUpdate);
        else
        {
            editor = new juce::TextEditor();
            editor->setInputRestrictions(5, "0123456789.");
            editor->setTooltip("Minimum Value (%) - Sweep start point");
            editor->setJustification(juce::Justification::centred);
        }

        // CRITICAL JUCE 8: Clear callback before mutating text
        editor->onTextChange = nullptr;
        editor->setText(juce::String(ctrl.minPct, 1), juce::dontSendNotification);

        // Rebind callback capturing EXACT rowNumber
        editor->onTextChange = [this, rowNumber, editor] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(currentConfig.controls.size()))
            {
                float val = std::clamp(editor->getText().getFloatValue(), 0.0f, 100.0f);
                auto& c = currentConfig.controls[static_cast<size_t>(rowNumber)];
                c.minPct = val;
                if (c.steps == 1)
                {
                    c.maxPct = val;
                    matrixTable.updateContent();
                }
                updateEstimatedTime();
                if (onConfigChanged) onConfigChanged();
            }
        };

        return editor;
    }

    // 6. Column Max % (TextEditor)
    if (columnId == colMax)
    {
        juce::TextEditor* editor = nullptr;
        if (existingComponentToUpdate != nullptr)
            editor = dynamic_cast<juce::TextEditor*>(existingComponentToUpdate);
        else
        {
            editor = new juce::TextEditor();
            editor->setInputRestrictions(5, "0123456789.");
            editor->setTooltip("Maximum Value (%) - Sweep end point");
            editor->setJustification(juce::Justification::centred);
        }

        // CRITICAL JUCE 8: Clear callback before mutating text
        editor->onTextChange = nullptr;
        editor->setText(juce::String(ctrl.maxPct, 1), juce::dontSendNotification);

        bool enabled = (ctrl.steps > 1);
        editor->setEnabled(enabled);
        editor->setColour(juce::TextEditor::backgroundColourId, enabled ? AppTheme::SurfaceCard : AppTheme::SurfaceHover);
        editor->setColour(juce::TextEditor::textColourId, enabled ? AppTheme::TextPrimary : AppTheme::TextSecondary);

        // Rebind callback capturing EXACT rowNumber
        editor->onTextChange = [this, rowNumber, editor] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(currentConfig.controls.size()))
            {
                auto& c = currentConfig.controls[static_cast<size_t>(rowNumber)];
                float val = std::clamp(editor->getText().getFloatValue(), c.minPct, 100.0f);
                c.maxPct = val;
                updateEstimatedTime();
                if (onConfigChanged) onConfigChanged();
            }
        };

        return editor;
    }

    // 7. Column Order (OrderButtonsComponent)
    if (columnId == colOrder)
    {
        OrderButtonsComponent* order = nullptr;
        if (existingComponentToUpdate != nullptr)
            order = dynamic_cast<OrderButtonsComponent*>(existingComponentToUpdate);
        else
            order = new OrderButtonsComponent();

        // CRITICAL JUCE 8: Clear callbacks before mutating state
        order->btnUp.onClick = nullptr;
        order->btnDown.onClick = nullptr;

        order->btnUp.setEnabled(rowNumber > 0);
        order->btnDown.setEnabled(rowNumber + 1 < static_cast<int>(currentConfig.controls.size()));

        // Rebind callbacks capturing EXACT rowNumber
        order->btnUp.onClick = [this, rowNumber] {
            if (rowNumber > 0 && rowNumber < static_cast<int>(currentConfig.controls.size()))
            {
                std::swap(currentConfig.controls[static_cast<size_t>(rowNumber)],
                          currentConfig.controls[static_cast<size_t>(rowNumber - 1)]);
                matrixTable.updateContent();
                if (onConfigChanged) onConfigChanged();
            }
        };

        order->btnDown.onClick = [this, rowNumber] {
            if (rowNumber >= 0 && rowNumber + 1 < static_cast<int>(currentConfig.controls.size()))
            {
                std::swap(currentConfig.controls[static_cast<size_t>(rowNumber)],
                          currentConfig.controls[static_cast<size_t>(rowNumber + 1)]);
                matrixTable.updateContent();
                if (onConfigChanged) onConfigChanged();
            }
        };

        return order;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

} // namespace abdaudiolab::gui
