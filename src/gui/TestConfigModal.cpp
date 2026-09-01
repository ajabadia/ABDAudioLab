#include "TestConfigModal.h"
#include <cmath>

namespace abdaudiolab::gui
{

TestConfigModal::TestConfigModal()
{
    setAlwaysOnTop(true);
    setWantsKeyboardFocus(true);
    setVisible(false);

    addAndMakeVisible(panel);

    // Close button
    btnClose.setButtonText(juce::String::fromUTF8(u8"✕"));
    btnClose.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClose.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnClose.onClick = [this] { dismissDialog(); };
    panel.addAndMakeVisible(btnClose);

    // Section 1: Stimulus
    lblSectionStimulus.setText("1. TEST IDENTIFIER & STIMULUS SIGNAL", juce::dontSendNotification);
    lblSectionStimulus.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSectionStimulus.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    panel.addAndMakeVisible(lblSectionStimulus);

    lblTestName.setText("Test Name:", juce::dontSendNotification);
    lblTestName.setFont(juce::FontOptions(10.5f));
    lblTestName.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    panel.addAndMakeVisible(lblTestName);

    txtTestName.setText("Custom Acoustic / Hardware Profile");
    panel.addAndMakeVisible(txtTestName);

    lblStimulusType.setText("Stimulus Type:", juce::dontSendNotification);
    lblStimulusType.setFont(juce::FontOptions(10.5f));
    lblStimulusType.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    panel.addAndMakeVisible(lblStimulusType);

    comboStimulusType.addItem("Farina Log Sweep (20 Hz - 40 kHz)", 1);
    comboStimulusType.addItem("Amplitude Ramp (-40 dBfs to 0 dBfs)", 2);
    comboStimulusType.addItem("Triple Sync Pulses (Gate Step)", 3);
    comboStimulusType.addItem("Pure Sine Wave (1 kHz Continuous)", 4);
    comboStimulusType.addItem("LCG White Noise (Broadband)", 5);
    comboStimulusType.setSelectedId(1, juce::dontSendNotification);
    comboStimulusType.onChange = [this] {
        int id = comboStimulusType.getSelectedId();
        if (id == 1) lblStimulusDesc.setText("Logarithmic swept-sine for frequency response, THD% and H2-H5 harmonics.", juce::dontSendNotification);
        else if (id == 2) lblStimulusDesc.setText("Stepped amplitude ramps for non-linear saturation and compression knees.", juce::dontSendNotification);
        else if (id == 3) lblStimulusDesc.setText("Gate step pulses for attack, decay, sustain and release envelope timings.", juce::dontSendNotification);
        else if (id == 4) lblStimulusDesc.setText("Stable 1 kHz tone for LFO rate, chorus depth, wow & flutter, and thermal drift.", juce::dontSendNotification);
        else lblStimulusDesc.setText("Broadband Gaussian-distributed white noise for spectral density mapping.", juce::dontSendNotification);
    };
    panel.addAndMakeVisible(comboStimulusType);

    lblStimulusDesc.setText("Logarithmic swept-sine for frequency response, THD% and H2-H5 harmonics.", juce::dontSendNotification);
    lblStimulusDesc.setFont(juce::FontOptions(9.5f, juce::Font::italic));
    lblStimulusDesc.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    panel.addAndMakeVisible(lblStimulusDesc);

    // Section 2: Duration
    lblSectionDuration.setText("2. BURST DURATION & ADAPTIVE CAPTURE", juce::dontSendNotification);
    lblSectionDuration.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSectionDuration.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    panel.addAndMakeVisible(lblSectionDuration);

    comboDurationPreset.addItem("0.5s (Fast Check / Rapid Calibration)", 1);
    comboDurationPreset.addItem("1.0s (Standard Lab Precision - Recommended)", 2);
    comboDurationPreset.addItem("2.5s (High SNR & Harmonic Separation)", 3);
    comboDurationPreset.addItem("5.0s (Ultra Precision & Thermal Drift)", 4);
    comboDurationPreset.addItem("10.0s (Long Integration / Modulated Pads)", 5);
    comboDurationPreset.addItem("Custom Manual Duration...", 6);
    comboDurationPreset.setSelectedId(2, juce::dontSendNotification);
    comboDurationPreset.onChange = [this] {
        int id = comboDurationPreset.getSelectedId();
        isUpdatingFromPreset = true;
        if (id == 1) { txtManualDuration.setText("0.5", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 2) { txtManualDuration.setText("1.0", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 3) { txtManualDuration.setText("2.5", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 4) { txtManualDuration.setText("5.0", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 5) { txtManualDuration.setText("10.0", juce::dontSendNotification); txtManualDuration.setEnabled(false); }
        else if (id == 6) { txtManualDuration.setEnabled(true); txtManualDuration.grabKeyboardFocus(); }
        isUpdatingFromPreset = false;
        updateEstimatedTime();
    };
    panel.addAndMakeVisible(comboDurationPreset);

    lblManualDuration.setText("Custom Duration (s):", juce::dontSendNotification);
    lblManualDuration.setFont(juce::FontOptions(10.5f));
    lblManualDuration.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    panel.addAndMakeVisible(lblManualDuration);

    txtManualDuration.setText("1.0");
    txtManualDuration.setEnabled(false);
    txtManualDuration.onTextChange = [this] {
        if (!isUpdatingFromPreset)
        {
            if (comboDurationPreset.getSelectedId() != 6)
                comboDurationPreset.setSelectedId(6, juce::dontSendNotification);
        }
        updateEstimatedTime();
    };
    panel.addAndMakeVisible(txtManualDuration);

    lblSecondsUnit.setText("sec", juce::dontSendNotification);
    lblSecondsUnit.setFont(juce::FontOptions(10.5f));
    lblSecondsUnit.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    panel.addAndMakeVisible(lblSecondsUnit);

    btnAdaptiveTail.setTooltip("Automatically stops capture when envelope or reverb tail drops below -60 dBfs");
    btnAdaptiveTail.onClick = [this] { updateEstimatedTime(); };
    panel.addAndMakeVisible(btnAdaptiveTail);

    // Section 3: Matrix Grid Resolution
    lblSectionMatrix.setText("3. MEASUREMENT MATRIX RESOLUTION (SET STEPS PER PHYSICAL CONTROL)", juce::dontSendNotification);
    lblSectionMatrix.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSectionMatrix.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    panel.addAndMakeVisible(lblSectionMatrix);

    // Summary & Action Buttons
    lblSummaryBanner.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSummaryBanner.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    panel.addAndMakeVisible(lblSummaryBanner);

    btnCancel.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
    btnCancel.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnCancel.onClick = [this] { dismissDialog(); };
    panel.addAndMakeVisible(btnCancel);

    btnApply.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillBlackBg);
    btnApply.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnApply.onClick = [this] {
        currentConfig.testName = txtTestName.getText();
        
        int stimId = comboStimulusType.getSelectedId();
        if (stimId == 1) currentConfig.stimulusType = audio::StimulusType::LogFarinaSweep;
        else if (stimId == 2) currentConfig.stimulusType = audio::StimulusType::AmplitudeRamp;
        else if (stimId == 3) currentConfig.stimulusType = audio::StimulusType::SyncPulses3;
        else if (stimId == 4) currentConfig.stimulusType = audio::StimulusType::SineWave1kHz;
        else currentConfig.stimulusType = audio::StimulusType::Silence;

        float parsedDuration = txtManualDuration.getText().getFloatValue();
        currentConfig.burstDurationSec = std::clamp(parsedDuration, 0.05f, 300.0f);
        currentConfig.captureMode = btnAdaptiveTail.getToggleState() ? "ADAPTIVE_ENVELOPE" : "FIXED_TIME";

        for (size_t i = 0; i < controlRowWidgets.size() && i < currentConfig.controls.size(); ++i)
        {
            int sId = controlRowWidgets[i].combo->getSelectedId();
            if (sId == 99)
            {
                int customVal = controlRowWidgets[i].txtCustomSteps->getText().getIntValue();
                currentConfig.controls[i].steps = std::max(1, customVal);
            }
            else
            {
                currentConfig.controls[i].steps = (sId > 0) ? sId : 1;
            }
        }

        if (onConfigurationConfirmed)
            onConfigurationConfirmed(currentConfig);

        dismissDialog();
    };
    panel.addAndMakeVisible(btnApply);
}

void TestConfigModal::rebuildControlRows()
{
    controlRowWidgets.clear();

    for (size_t i = 0; i < currentConfig.controls.size(); ++i)
    {
        const auto& ctrl = currentConfig.controls[i];
        ControlRowWidgets row;
        row.label = std::make_unique<juce::Label>();
        row.label->setText(ctrl.name + " (" + ctrl.type + "):", juce::dontSendNotification);
        row.label->setFont(juce::FontOptions(10.0f));
        row.label->setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        panel.addAndMakeVisible(row.label.get());

        row.combo = std::make_unique<juce::ComboBox>();
        row.combo->addItem("Fixed (1 step - default)", 1);
        row.combo->addItem("3 Steps (0%, 50%, 100%)", 3);
        row.combo->addItem("5 Steps (Standard: 0, 25, 50, 75, 100%)", 5);
        row.combo->addItem("8 Steps (Detailed: 8 steps)", 8);
        row.combo->addItem("16 Steps (High-Res: 16 steps)", 16);
        row.combo->addItem("32 Steps (Ultra High-Res: 32 steps)", 32);
        row.combo->addItem("64 Steps (Extreme: 64 steps)", 64);
        row.combo->addItem("Custom Steps (Manual)...", 99);

        row.txtCustomSteps = std::make_unique<juce::TextEditor>();
        row.txtCustomSteps->setInputRestrictions(3, "0123456789");

        int initialStep = (ctrl.steps > 0) ? ctrl.steps : 1;
        bool isStandard = (initialStep == 1 || initialStep == 3 || initialStep == 5 || 
                           initialStep == 8 || initialStep == 16 || initialStep == 32 || initialStep == 64);

        if (isStandard)
        {
            row.combo->setSelectedId(initialStep, juce::dontSendNotification);
            row.txtCustomSteps->setText(juce::String(initialStep), juce::dontSendNotification);
            row.txtCustomSteps->setEnabled(false);
        }
        else
        {
            row.combo->setSelectedId(99, juce::dontSendNotification);
            row.txtCustomSteps->setText(juce::String(initialStep), juce::dontSendNotification);
            row.txtCustomSteps->setEnabled(true);
        }

        size_t rowIdx = i;
        row.combo->onChange = [this, rowIdx] {
            if (rowIdx < controlRowWidgets.size() && rowIdx < currentConfig.controls.size())
            {
                int sId = controlRowWidgets[rowIdx].combo->getSelectedId();
                if (sId == 99)
                {
                    controlRowWidgets[rowIdx].txtCustomSteps->setEnabled(true);
                    controlRowWidgets[rowIdx].txtCustomSteps->grabKeyboardFocus();
                    int cVal = controlRowWidgets[rowIdx].txtCustomSteps->getText().getIntValue();
                    currentConfig.controls[rowIdx].steps = std::max(1, cVal);
                }
                else
                {
                    controlRowWidgets[rowIdx].txtCustomSteps->setText(juce::String(sId), juce::dontSendNotification);
                    controlRowWidgets[rowIdx].txtCustomSteps->setEnabled(false);
                    currentConfig.controls[rowIdx].steps = sId;
                }
                updateEstimatedTime();
            }
        };

        row.txtCustomSteps->onTextChange = [this, rowIdx] {
            if (rowIdx < controlRowWidgets.size() && rowIdx < currentConfig.controls.size())
            {
                int cVal = controlRowWidgets[rowIdx].txtCustomSteps->getText().getIntValue();
                if (controlRowWidgets[rowIdx].combo->getSelectedId() != 99)
                {
                    controlRowWidgets[rowIdx].combo->setSelectedId(99, juce::dontSendNotification);
                }
                currentConfig.controls[rowIdx].steps = std::max(1, cVal);
                updateEstimatedTime();
            }
        };

        panel.addAndMakeVisible(row.combo.get());
        panel.addAndMakeVisible(row.txtCustomSteps.get());

        controlRowWidgets.push_back(std::move(row));
    }
}

void TestConfigModal::showDialog(juce::Component* parent, const TestConfiguration& initialConfig)
{
    if (parent != nullptr)
    {
        setBounds(parent->getLocalBounds());
        parent->addAndMakeVisible(this);
    }
    currentConfig = initialConfig;

    txtTestName.setText(currentConfig.testName);
    
    if (currentConfig.stimulusType == audio::StimulusType::LogFarinaSweep) comboStimulusType.setSelectedId(1, juce::dontSendNotification);
    else if (currentConfig.stimulusType == audio::StimulusType::AmplitudeRamp) comboStimulusType.setSelectedId(2, juce::dontSendNotification);
    else if (currentConfig.stimulusType == audio::StimulusType::SyncPulses3) comboStimulusType.setSelectedId(3, juce::dontSendNotification);
    else if (currentConfig.stimulusType == audio::StimulusType::SineWave1kHz) comboStimulusType.setSelectedId(4, juce::dontSendNotification);

    isUpdatingFromPreset = true;
    float dur = currentConfig.burstDurationSec;
    if (std::abs(dur - 0.5f) < 0.01f) comboDurationPreset.setSelectedId(1, juce::dontSendNotification);
    else if (std::abs(dur - 1.0f) < 0.01f) comboDurationPreset.setSelectedId(2, juce::dontSendNotification);
    else if (std::abs(dur - 2.5f) < 0.01f) comboDurationPreset.setSelectedId(3, juce::dontSendNotification);
    else if (std::abs(dur - 5.0f) < 0.01f) comboDurationPreset.setSelectedId(4, juce::dontSendNotification);
    else if (std::abs(dur - 10.0f) < 0.01f) comboDurationPreset.setSelectedId(5, juce::dontSendNotification);
    else comboDurationPreset.setSelectedId(6, juce::dontSendNotification);

    txtManualDuration.setText(juce::String(dur, 2), juce::dontSendNotification);
    txtManualDuration.setEnabled(comboDurationPreset.getSelectedId() == 6);
    isUpdatingFromPreset = false;

    btnAdaptiveTail.setToggleState(currentConfig.captureMode == "ADAPTIVE_ENVELOPE", juce::dontSendNotification);

    rebuildControlRows();
    updateEstimatedTime();

    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
    resized();
    repaint();
}

void TestConfigModal::dismissDialog()
{
    setVisible(false);
}

bool TestConfigModal::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        dismissDialog();
        return true;
    }
    return false;
}

void TestConfigModal::mouseDown(const juce::MouseEvent& e)
{
    if (!panel.getBounds().contains(e.getPosition()))
    {
        dismissDialog();
    }
}

void TestConfigModal::updateEstimatedTime()
{
    int total = 1;
    juce::String formulaStr;
    int activeCount = 0;

    for (const auto& ctrl : currentConfig.controls)
    {
        if (ctrl.steps > 1)
        {
            total *= ctrl.steps;
            if (formulaStr.isNotEmpty()) formulaStr += " x ";
            formulaStr += juce::String(ctrl.steps);
            activeCount++;
        }
    }

    if (activeCount == 0)
    {
        total = 1;
        formulaStr = "1 (Fixed Reference)";
    }

    float dur = txtManualDuration.getText().getFloatValue();
    if (dur <= 0.01f) dur = 1.0f;

    double estSec = total * (static_cast<double>(dur) + 0.35); // Duration + ADC settling time
    int estMin = static_cast<int>(estSec / 60.0);
    int estRemSec = static_cast<int>(estSec) % 60;

    juce::String timeStr;
    if (estMin > 0)
        timeStr = juce::String(estMin) + "m " + juce::String(estRemSec) + "s";
    else
        timeStr = juce::String(estRemSec) + "s";

    juce::String adaptiveStr = btnAdaptiveTail.getToggleState() ? " [Adaptive Auto-Tail]" : "";
    lblSummaryBanner.setText("Total: " + formulaStr + " = " + juce::String(total) + " measurements (~" + timeStr + ")" + adaptiveStr, juce::dontSendNotification);
}

void TestConfigModal::resized()
{
    auto area = getLocalBounds();
    int panelW = juce::jmin(700, area.getWidth() - 24);
    int panelH = juce::jmin(600, area.getHeight() - 24);
    panel.setBounds((area.getWidth() - panelW) / 2, (area.getHeight() - panelH) / 2, panelW, panelH);

    btnClose.setBounds(panelW - 36, 12, 24, 24);

    int padX = 24;
    int contentW = panelW - 48;
    int y = 20;

    // Header gap
    y += 24;

    // 1. Stimulus Section
    lblSectionStimulus.setBounds(padX, y, contentW, 16);
    y += 20;

    lblTestName.setBounds(padX, y, 90, 24);
    txtTestName.setBounds(padX + 95, y, contentW - 95, 24);
    y += 28;

    lblStimulusType.setBounds(padX, y, 90, 24);
    comboStimulusType.setBounds(padX + 95, y, contentW - 95, 24);
    y += 26;

    lblStimulusDesc.setBounds(padX + 95, y, contentW - 95, 14);
    y += 24;

    // 2. Duration Section
    lblSectionDuration.setBounds(padX, y, contentW, 16);
    y += 20;

    comboDurationPreset.setBounds(padX, y, contentW - 200, 26);
    lblManualDuration.setBounds(padX + contentW - 190, y, 105, 26);
    txtManualDuration.setBounds(padX + contentW - 80, y, 55, 26);
    lblSecondsUnit.setBounds(padX + contentW - 20, y, 20, 26);
    y += 30;

    btnAdaptiveTail.setBounds(padX, y, contentW, 20);
    y += 26;

    // 3. Matrix Section
    lblSectionMatrix.setBounds(padX, y, contentW, 16);
    y += 20;

    // Layout dynamic control rows (2 columns: with combo + custom input box)
    int colW = (contentW - 16) / 2;
    for (size_t i = 0; i < controlRowWidgets.size(); ++i)
    {
        int col = static_cast<int>(i % 2);
        int curX = padX + col * (colW + 16);
        
        controlRowWidgets[i].label->setBounds(curX, y, colW, 15);
        
        // Combo takes most of width, custom input box takes 42px on the right
        int comboW = colW - 46;
        controlRowWidgets[i].combo->setBounds(curX, y + 16, comboW, 25);
        controlRowWidgets[i].txtCustomSteps->setBounds(curX + comboW + 4, y + 16, 42, 25);

        if (col == 1 || i == controlRowWidgets.size() - 1)
        {
            y += 46;
        }
    }

    y += 6;

    // Summary banner
    lblSummaryBanner.setBounds(padX, y, contentW, 18);

    // Bottom action buttons
    int bottomY = panelH - 46;
    btnCancel.setBounds(padX, bottomY, 110, 32);
    btnApply.setBounds(panelW - 220, bottomY, 196, 32);
}

void TestConfigModal::paint(juce::Graphics& g)
{
    // Backdrop
    g.fillAll(juce::Colours::black.withAlpha(0.40f));

    // Panel Card
    auto bounds = panel.getBounds().toFloat();
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(bounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = bounds.reduced(24.0f, 16.0f);

    // Header Title
    auto headerRow = content.removeFromTop(26.0f);
    g.setFont(juce::FontOptions(16.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Configure Test Profile & Control Sweep Matrix", headerRow.removeFromLeft(390.0f), juce::Justification::centredLeft, true);

    auto badgeRect = headerRow.removeFromLeft(120.0f).reduced(0.0f, 3.0f);
    g.setColour(juce::Colour(0xfff3f4f6));
    g.fillRoundedRectangle(badgeRect, 4.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff374151));
    g.drawText("CUSTOM PROFILE", badgeRect, juce::Justification::centred, true);

    // Thin separator
    content.removeFromTop(4.0f);
    g.setColour(juce::Colour(0xffe5e7eb));
    g.fillRect(bounds.getX() + 24.0f, bounds.getY() + 48.0f, bounds.getWidth() - 48.0f, 1.0f);
}

} // namespace abdaudiolab::gui
