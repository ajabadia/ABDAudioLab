#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../session/ProfilingSessionContracts.h"
#include "../OperatorCardsContainerComponent.h"

namespace abdaudiolab::gui::soundid
{

/**
 * @brief Adaptive excitation configuration panel for Step 2 (CalibrateLoopback / Setup).
 * Adapts reactively to target capabilities:
 * - Pure analog / non-digital targets: Forces ManualOperator (MANUAL_PROMPT) with settling time,
 *   repetition count, operator guidance instructions, and OperatorCards preview.
 * - Digital targets (MIDI/VST3): Allows switching between AutomatedMidi and ManualOperator,
 *   configuring note range, velocities, gateMs, settlingMs, and channel.
 *
 * Adheres to architectural boundary: Does NOT call ManualAnalogueController or ProfilingSequencer directly;
 * all commands flow through IProfilingSessionCommands.
 */
class SoundIdExcitationConfigPanel : public juce::Component
{
public:
    explicit SoundIdExcitationConfigPanel(session::IProfilingSessionCommands& commands);
    ~SoundIdExcitationConfigPanel() override;

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setupUiElements();
    void updateModeVisibility();
    void pushMidiRecipeUpdate();
    void pushManualRecipeUpdate();

    session::IProfilingSessionCommands& commands_;

    session::TargetControlMode currentControlMode_ { session::TargetControlMode::Vst3 };
    session::ExcitationMode currentExcitationMode_ { session::ExcitationMode::AutomatedMidi };
    bool isUpdatingFromSnapshot_ { false };

    // Header & Target Info
    juce::Label headerTitle_;
    juce::Label targetCapabilityBadge_;
    juce::Label targetSummaryLabel_;

    // Mode Selector (Automated vs Manual)
    juce::GroupComponent modeGroup_;
    juce::ToggleButton btnModeAutomatedMidi_ { "Automated MIDI Excitation" };
    juce::ToggleButton btnModeManualOperator_ { "Manual Operator Guidance" };

    // MIDI Parameters Card
    juce::GroupComponent midiGroup_;
    juce::Label lblFirstNote_ { {}, "First Note:" };
    juce::Slider sldrFirstNote_;
    juce::Label lblLastNote_ { {}, "Last Note:" };
    juce::Slider sldrLastNote_;
    juce::Label lblGateMs_ { {}, "Gate Duration (ms):" };
    juce::Slider sldrGateMs_;
    juce::Label lblSettlingMs_ { {}, "Settling Time (ms):" };
    juce::Slider sldrSettlingMs_;
    juce::Label lblMidiChannel_ { {}, "MIDI Channel:" };
    juce::Slider sldrMidiChannel_;
    juce::Label lblVelocities_ { {}, "Velocities:" };
    juce::ToggleButton btnVelSoft_ { "32 (Soft)" };
    juce::ToggleButton btnVelMed_ { "64 (Med)" };
    juce::ToggleButton btnVelFull_ { "127 (Full)" };
    juce::Label lblSequenceSummary_;

    // Manual Operator Parameters Card
    juce::GroupComponent manualGroup_;
    juce::Label lblInteractionKind_ { {}, "Interaction Kind:" };
    juce::ComboBox cmbInteractionKind_;
    juce::Label lblInstruction_ { {}, "Instruction Prompt:" };
    juce::TextEditor txtInstruction_;
    juce::Label lblExpectedSetting_ { {}, "Expected Setting:" };
    juce::TextEditor txtExpectedSetting_;
    juce::Label lblRepetitions_ { {}, "Repetitions:" };
    juce::Slider sldrRepetitions_;
    juce::Label lblManualSettlingMs_ { {}, "Operator Settling (ms):" };
    juce::Slider sldrManualSettlingMs_;

    // Visual Preview of Controls for Manual mode
    OperatorCardsContainerComponent cardsPreview_;

    // Validation Status
    juce::Label validationBanner_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdExcitationConfigPanel)
};

} // namespace abdaudiolab::gui::soundid
