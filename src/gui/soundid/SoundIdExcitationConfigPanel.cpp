#include "SoundIdExcitationConfigPanel.h"
#include "../SoundIdTheme.h"
#include <sstream>

namespace abdaudiolab::gui::soundid
{

SoundIdExcitationConfigPanel::SoundIdExcitationConfigPanel(session::IProfilingSessionCommands& commands)
    : commands_(commands)
{
    setupUiElements();
}

SoundIdExcitationConfigPanel::~SoundIdExcitationConfigPanel() = default;

void SoundIdExcitationConfigPanel::setupUiElements()
{
    // Header
    headerTitle_.setText("Stimulus Excitation & Operator Recipe", juce::dontSendNotification);
    headerTitle_.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    targetCapabilityBadge_.setText("Digital Control (MIDI)", juce::dontSendNotification);
    targetCapabilityBadge_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    targetCapabilityBadge_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    targetCapabilityBadge_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(targetCapabilityBadge_);

    targetSummaryLabel_.setText("Configure acoustic stimulus signals for profiling and frequency response mapping.", juce::dontSendNotification);
    targetSummaryLabel_.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    targetSummaryLabel_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(targetSummaryLabel_);

    // Mode Selector Group
    modeGroup_.setText("Excitation Mode");
    modeGroup_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    modeGroup_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(modeGroup_);

    btnModeAutomatedMidi_.setRadioGroupId(101);
    btnModeAutomatedMidi_.setToggleState(true, juce::dontSendNotification);
    btnModeAutomatedMidi_.onClick = [this] {
        if (!isUpdatingFromSnapshot_ && btnModeAutomatedMidi_.getToggleState())
        {
            commands_.setExcitationMode(session::ExcitationMode::AutomatedMidi);
            updateModeVisibility();
        }
    };
    addAndMakeVisible(btnModeAutomatedMidi_);

    btnModeManualOperator_.setRadioGroupId(101);
    btnModeManualOperator_.onClick = [this] {
        if (!isUpdatingFromSnapshot_ && btnModeManualOperator_.getToggleState())
        {
            commands_.setExcitationMode(session::ExcitationMode::ManualOperator);
            updateModeVisibility();
        }
    };
    addAndMakeVisible(btnModeManualOperator_);

    // --- MIDI Configuration Group ---
    midiGroup_.setText("Automated MIDI Note Parameters");
    midiGroup_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    midiGroup_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(midiGroup_);

    auto setupSlider = [this](juce::Slider& s, double min, double max, double def, double step, const juce::String& suffix) {
        s.setRange(min, max, step);
        s.setValue(def, juce::dontSendNotification);
        s.setSliderStyle(juce::Slider::LinearBar);
        s.setTextValueSuffix(suffix);
        s.onValueChange = [this] { pushMidiRecipeUpdate(); };
        addAndMakeVisible(s);
    };

    setupSlider(sldrFirstNote_, 0, 127, 36, 1, " (C2)");
    sldrFirstNote_.onValueChange = [this] {
        if (sldrFirstNote_.getValue() > sldrLastNote_.getValue())
            sldrLastNote_.setValue(sldrFirstNote_.getValue(), juce::dontSendNotification);
        sldrFirstNote_.setTextValueSuffix(" (MIDI " + juce::String(static_cast<int>(sldrFirstNote_.getValue())) + ")");
        pushMidiRecipeUpdate();
    };

    setupSlider(sldrLastNote_, 0, 127, 84, 1, " (C6)");
    sldrLastNote_.onValueChange = [this] {
        if (sldrLastNote_.getValue() < sldrFirstNote_.getValue())
            sldrFirstNote_.setValue(sldrLastNote_.getValue(), juce::dontSendNotification);
        sldrLastNote_.setTextValueSuffix(" (MIDI " + juce::String(static_cast<int>(sldrLastNote_.getValue())) + ")");
        pushMidiRecipeUpdate();
    };

    setupSlider(sldrGateMs_, 20.0, 2000.0, 250.0, 10.0, " ms");
    setupSlider(sldrSettlingMs_, 0.0, 1000.0, 50.0, 5.0, " ms");
    setupSlider(sldrMidiChannel_, 1, 16, 1, 1, " Ch");

    auto setupLabel = [this](juce::Label& lbl) {
        lbl.setFont(juce::FontOptions(12.0f, juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupLabel(lblFirstNote_);
    setupLabel(lblLastNote_);
    setupLabel(lblGateMs_);
    setupLabel(lblSettlingMs_);
    setupLabel(lblMidiChannel_);
    setupLabel(lblVelocities_);

    auto setupVelBtn = [this](juce::ToggleButton& b) {
        b.setToggleState(true, juce::dontSendNotification);
        b.onClick = [this] { pushMidiRecipeUpdate(); };
        addAndMakeVisible(b);
    };
    setupVelBtn(btnVelSoft_);
    setupVelBtn(btnVelMed_);
    setupVelBtn(btnVelFull_);

    lblSequenceSummary_.setText("Sequence: C2-C6 @ 3 velocities (147 trials)", juce::dontSendNotification);
    lblSequenceSummary_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblSequenceSummary_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    addAndMakeVisible(lblSequenceSummary_);

    // --- Manual Operator Group ---
    manualGroup_.setText("Manual Hardware / Operator Setup");
    manualGroup_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    manualGroup_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(manualGroup_);

    setupLabel(lblInteractionKind_);
    cmbInteractionKind_.addItem("Physical Control Adjustment", 1);
    cmbInteractionKind_.addItem("Manual Note Performance", 2);
    cmbInteractionKind_.addItem("Preset / Routing Confirmation", 3);
    cmbInteractionKind_.setSelectedId(1, juce::dontSendNotification);
    cmbInteractionKind_.onChange = [this] { pushManualRecipeUpdate(); };
    addAndMakeVisible(cmbInteractionKind_);

    setupLabel(lblInstruction_);
    txtInstruction_.setText("Ajustar controles segun la tarjeta y pulsar Listo [Espacio]");
    txtInstruction_.onTextChange = [this] { pushManualRecipeUpdate(); };
    addAndMakeVisible(txtInstruction_);

    setupLabel(lblExpectedSetting_);
    txtExpectedSetting_.setText("Default / Baseline");
    txtExpectedSetting_.onTextChange = [this] { pushManualRecipeUpdate(); };
    addAndMakeVisible(txtExpectedSetting_);

    setupLabel(lblRepetitions_);
    sldrRepetitions_.setRange(1, 5, 1);
    sldrRepetitions_.setValue(1, juce::dontSendNotification);
    sldrRepetitions_.setSliderStyle(juce::Slider::LinearBar);
    sldrRepetitions_.onValueChange = [this] { pushManualRecipeUpdate(); };
    addAndMakeVisible(sldrRepetitions_);

    setupLabel(lblManualSettlingMs_);
    sldrManualSettlingMs_.setRange(0.0, 3000.0, 50.0);
    sldrManualSettlingMs_.setValue(500.0, juce::dontSendNotification);
    sldrManualSettlingMs_.setTextValueSuffix(" ms");
    sldrManualSettlingMs_.setSliderStyle(juce::Slider::LinearBar);
    sldrManualSettlingMs_.onValueChange = [this] { pushManualRecipeUpdate(); };
    addAndMakeVisible(sldrManualSettlingMs_);

    // Cards Preview
    addAndMakeVisible(cardsPreview_);

    // Validation Banner
    validationBanner_.setText("Recipe Verified: Valid configuration", juce::dontSendNotification);
    validationBanner_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    validationBanner_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    addAndMakeVisible(validationBanner_);

    updateModeVisibility();
}

void SoundIdExcitationConfigPanel::updateModeVisibility()
{
    bool isManual = (currentExcitationMode_ == session::ExcitationMode::ManualOperator);

    midiGroup_.setVisible(!isManual);
    lblFirstNote_.setVisible(!isManual);
    sldrFirstNote_.setVisible(!isManual);
    lblLastNote_.setVisible(!isManual);
    sldrLastNote_.setVisible(!isManual);
    lblGateMs_.setVisible(!isManual);
    sldrGateMs_.setVisible(!isManual);
    lblSettlingMs_.setVisible(!isManual);
    sldrSettlingMs_.setVisible(!isManual);
    lblMidiChannel_.setVisible(!isManual);
    sldrMidiChannel_.setVisible(!isManual);
    lblVelocities_.setVisible(!isManual);
    btnVelSoft_.setVisible(!isManual);
    btnVelMed_.setVisible(!isManual);
    btnVelFull_.setVisible(!isManual);
    lblSequenceSummary_.setVisible(!isManual);

    manualGroup_.setVisible(isManual);
    lblInteractionKind_.setVisible(isManual);
    cmbInteractionKind_.setVisible(isManual);
    lblInstruction_.setVisible(isManual);
    txtInstruction_.setVisible(isManual);
    lblExpectedSetting_.setVisible(isManual);
    txtExpectedSetting_.setVisible(isManual);
    lblRepetitions_.setVisible(isManual);
    sldrRepetitions_.setVisible(isManual);
    lblManualSettlingMs_.setVisible(isManual);
    sldrManualSettlingMs_.setVisible(isManual);
    cardsPreview_.setVisible(isManual);

    resized();
}

void SoundIdExcitationConfigPanel::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    isUpdatingFromSnapshot_ = true;

    currentControlMode_ = snapshot.excitation.targetControlMode;
    currentExcitationMode_ = snapshot.excitation.excitationMode;

    bool isNoDigital = (currentControlMode_ == session::TargetControlMode::NoDigitalControl);

    if (isNoDigital)
    {
        targetCapabilityBadge_.setText("Pure Analog / No Digital Control", juce::dontSendNotification);
        targetCapabilityBadge_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
        targetSummaryLabel_.setText("Hardware without MIDI/VST3 control. Manual operator intervention is mandatory.", juce::dontSendNotification);

        btnModeAutomatedMidi_.setEnabled(false);
        btnModeManualOperator_.setEnabled(true);
        btnModeManualOperator_.setToggleState(true, juce::dontSendNotification);
    }
    else
    {
        bool supportsMidi = snapshot.target.supportsMidiInput || (currentControlMode_ == session::TargetControlMode::Midi);
        juce::String devType = (currentControlMode_ == session::TargetControlMode::Vst3) ? "Plugin VST3" : "MIDI Device";
        targetCapabilityBadge_.setText("Digital Target: " + devType, juce::dontSendNotification);
        targetCapabilityBadge_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);

        if (supportsMidi)
        {
            targetSummaryLabel_.setText("Device supports MIDI automation. You may select Automated MIDI or Manual Operator.", juce::dontSendNotification);
            btnModeAutomatedMidi_.setEnabled(true);
            btnModeManualOperator_.setEnabled(true);

            if (currentExcitationMode_ == session::ExcitationMode::AutomatedMidi)
                btnModeAutomatedMidi_.setToggleState(true, juce::dontSendNotification);
            else
                btnModeManualOperator_.setToggleState(true, juce::dontSendNotification);
        }
        else
        {
            targetSummaryLabel_.setText("Device does not accept MIDI. Parameter automation or manual guidance available.", juce::dontSendNotification);
            btnModeAutomatedMidi_.setEnabled(false);
            btnModeManualOperator_.setEnabled(true);
            btnModeManualOperator_.setToggleState(true, juce::dontSendNotification);
        }
    }

    if (snapshot.excitation.midi.has_value())
    {
        const auto& m = *snapshot.excitation.midi;
        sldrFirstNote_.setValue(m.firstNote, juce::dontSendNotification);
        sldrLastNote_.setValue(m.lastNote, juce::dontSendNotification);
        sldrGateMs_.setValue(m.gateMs, juce::dontSendNotification);
        sldrSettlingMs_.setValue(m.settlingMs, juce::dontSendNotification);
        sldrMidiChannel_.setValue(m.midiChannel, juce::dontSendNotification);

        bool hasSoft = std::find(m.velocities.begin(), m.velocities.end(), 32) != m.velocities.end();
        bool hasMed = std::find(m.velocities.begin(), m.velocities.end(), 64) != m.velocities.end();
        bool hasFull = std::find(m.velocities.begin(), m.velocities.end(), 127) != m.velocities.end();
        btnVelSoft_.setToggleState(hasSoft, juce::dontSendNotification);
        btnVelMed_.setToggleState(hasMed, juce::dontSendNotification);
        btnVelFull_.setToggleState(hasFull, juce::dontSendNotification);

        int noteCount = std::max(0, m.lastNote - m.firstNote + 1);
        int velCount = static_cast<int>(m.velocities.size());
        int totalTrials = noteCount * velCount * m.repetitions;
        juce::String hashStr = m.sequenceHash.empty() ? "pending" : juce::String(m.sequenceHash).substring(0, 8);
        lblSequenceSummary_.setText("Sequence: " + juce::String(totalTrials) + " trials | Hash: " + hashStr, juce::dontSendNotification);
    }

    if (snapshot.excitation.manual.has_value())
    {
        const auto& man = *snapshot.excitation.manual;
        int kindId = 1;
        if (man.interactionKind == session::ManualInteractionKind::ManualNotePerformance) kindId = 2;
        else if (man.interactionKind == session::ManualInteractionKind::PresetOrRoutingConfirmation) kindId = 3;
        cmbInteractionKind_.setSelectedId(kindId, juce::dontSendNotification);

        txtInstruction_.setText(man.instruction, juce::dontSendNotification);
        txtExpectedSetting_.setText(man.expectedSetting, juce::dontSendNotification);
        sldrRepetitions_.setValue(man.repetitions, juce::dontSendNotification);
        sldrManualSettlingMs_.setValue(man.settlingMs, juce::dontSendNotification);
    }

    // Update validation status
    if (snapshot.excitation.isValid)
    {
        validationBanner_.setText("Recipe Verified: Ready for profiling", juce::dontSendNotification);
        validationBanner_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    }
    else
    {
        validationBanner_.setText("Recipe Error: " + juce::String(snapshot.excitation.validationError), juce::dontSendNotification);
        validationBanner_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
    }

    updateModeVisibility();
    isUpdatingFromSnapshot_ = false;
}

void SoundIdExcitationConfigPanel::pushMidiRecipeUpdate()
{
    if (isUpdatingFromSnapshot_)
        return;

    session::MidiRecipe r;
    r.firstNote = static_cast<int>(sldrFirstNote_.getValue());
    r.lastNote = static_cast<int>(sldrLastNote_.getValue());
    r.gateMs = sldrGateMs_.getValue();
    r.settlingMs = sldrSettlingMs_.getValue();
    r.midiChannel = static_cast<int>(sldrMidiChannel_.getValue());

    r.velocities.clear();
    if (btnVelSoft_.getToggleState()) r.velocities.push_back(32);
    if (btnVelMed_.getToggleState())  r.velocities.push_back(64);
    if (btnVelFull_.getToggleState()) r.velocities.push_back(127);

    commands_.updateMidiRecipe(r);
}

void SoundIdExcitationConfigPanel::pushManualRecipeUpdate()
{
    if (isUpdatingFromSnapshot_)
        return;

    session::ManualOperatorRecipe r;
    int kId = cmbInteractionKind_.getSelectedId();
    if (kId == 2)
        r.interactionKind = session::ManualInteractionKind::ManualNotePerformance;
    else if (kId == 3)
        r.interactionKind = session::ManualInteractionKind::PresetOrRoutingConfirmation;
    else
        r.interactionKind = session::ManualInteractionKind::PhysicalControlAdjustment;

    r.instruction = txtInstruction_.getText();
    r.expectedSetting = txtExpectedSetting_.getText();
    r.repetitions = static_cast<int>(sldrRepetitions_.getValue());
    r.settlingMs = sldrManualSettlingMs_.getValue();
    r.requireOperatorConfirmation = true;

    commands_.updateManualRecipe(r);
}

void SoundIdExcitationConfigPanel::paint(juce::Graphics& g)
{
    g.fillAll(SoundIdTheme::bgLight);
}

void SoundIdExcitationConfigPanel::resized()
{
    auto area = getLocalBounds().reduced(16);

    // Header
    auto headerArea = area.removeFromTop(24);
    targetCapabilityBadge_.setBounds(headerArea.removeFromRight(200));
    headerTitle_.setBounds(headerArea);
    targetSummaryLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(10);

    // Mode Selector
    modeGroup_.setBounds(area.removeFromTop(60));
    auto modeInner = modeGroup_.getBounds().reduced(12, 18);
    btnModeAutomatedMidi_.setBounds(modeInner.removeFromLeft(modeInner.getWidth() / 2 - 8));
    modeInner.removeFromLeft(16);
    btnModeManualOperator_.setBounds(modeInner);
    area.removeFromTop(12);

    // Dynamic Group: MIDI or Manual
    bool isManual = (currentExcitationMode_ == session::ExcitationMode::ManualOperator);
    if (!isManual)
    {
        auto midiArea = area.removeFromTop(200);
        midiGroup_.setBounds(midiArea);
        auto inner = midiArea.reduced(14, 20);

        // Row 1: Notes
        auto row1 = inner.removeFromTop(24);
        lblFirstNote_.setBounds(row1.removeFromLeft(80));
        sldrFirstNote_.setBounds(row1.removeFromLeft(120));
        row1.removeFromLeft(20);
        lblLastNote_.setBounds(row1.removeFromLeft(80));
        sldrLastNote_.setBounds(row1.removeFromLeft(120));
        inner.removeFromTop(8);

        // Row 2: Gate & Settling
        auto row2 = inner.removeFromTop(24);
        lblGateMs_.setBounds(row2.removeFromLeft(130));
        sldrGateMs_.setBounds(row2.removeFromLeft(100));
        row2.removeFromLeft(20);
        lblSettlingMs_.setBounds(row2.removeFromLeft(130));
        sldrSettlingMs_.setBounds(row2.removeFromLeft(100));
        inner.removeFromTop(8);

        // Row 3: Channel & Velocities
        auto row3 = inner.removeFromTop(24);
        lblMidiChannel_.setBounds(row3.removeFromLeft(100));
        sldrMidiChannel_.setBounds(row3.removeFromLeft(80));
        row3.removeFromLeft(20);
        lblVelocities_.setBounds(row3.removeFromLeft(70));
        btnVelSoft_.setBounds(row3.removeFromLeft(80));
        btnVelMed_.setBounds(row3.removeFromLeft(80));
        btnVelFull_.setBounds(row3.removeFromLeft(80));
        inner.removeFromTop(10);

        // Summary
        lblSequenceSummary_.setBounds(inner.removeFromTop(20));
    }
    else
    {
        auto manualArea = area.removeFromTop(220);
        manualGroup_.setBounds(manualArea);
        auto inner = manualArea.reduced(14, 20);

        // Row 1: Interaction Kind & Repetitions
        auto row1 = inner.removeFromTop(24);
        lblInteractionKind_.setBounds(row1.removeFromLeft(110));
        cmbInteractionKind_.setBounds(row1.removeFromLeft(200));
        row1.removeFromLeft(20);
        lblRepetitions_.setBounds(row1.removeFromLeft(80));
        sldrRepetitions_.setBounds(row1.removeFromLeft(80));
        inner.removeFromTop(8);

        // Row 2: Settling
        auto row2 = inner.removeFromTop(24);
        lblManualSettlingMs_.setBounds(row2.removeFromLeft(140));
        sldrManualSettlingMs_.setBounds(row2.removeFromLeft(100));
        inner.removeFromTop(8);

        // Row 3: Instruction Prompt
        auto row3 = inner.removeFromTop(24);
        lblInstruction_.setBounds(row3.removeFromLeft(120));
        txtInstruction_.setBounds(row3);
        inner.removeFromTop(8);

        // Row 4: Expected Setting
        auto row4 = inner.removeFromTop(24);
        lblExpectedSetting_.setBounds(row4.removeFromLeft(120));
        txtExpectedSetting_.setBounds(row4);
        inner.removeFromTop(8);

        // Preview of control cards if height permits
        if (area.getHeight() > 100)
        {
            cardsPreview_.setBounds(area.removeFromTop(std::min(140, area.getHeight() - 30)));
        }
    }

    area.removeFromTop(8);
    validationBanner_.setBounds(area.removeFromTop(22));
}

} // namespace abdaudiolab::gui::soundid
