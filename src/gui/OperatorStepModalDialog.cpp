/**
 * @file OperatorStepModalDialog.cpp
 * @brief Implementation of manual operator step alignment modal and control rendering.
 * @author ABDSynths
 * @date 2026
 */

#include "OperatorStepModalDialog.h"
#include "OperatorCardsContainerComponent.h"
#include "SoundIdTheme.h"
#include "HardwareControlRenderer.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::gui
{

OperatorStepModalDialog::OperatorStepModalDialog()
{
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    cardsContainer = std::make_unique<OperatorCardsContainerComponent>();

    btnAccept.setButtonText(juce::String::fromUTF8(u8"Accept Step [Space]"));
    btnAccept.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnAccept.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    addAndMakeVisible(btnAccept);

    btnRepeat.setButtonText("Repeat Step");
    btnRepeat.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillWhiteBg);
    btnRepeat.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    addAndMakeVisible(btnRepeat);

    btnStepBack.setButtonText("Step Back");
    btnStepBack.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillWhiteBg);
    btnStepBack.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    addAndMakeVisible(btnStepBack);

    btnToggleMetronome.setButtonText("Metronome (10s)");
    btnToggleMetronome.setTooltip("Enable 10s visual and audible rhythmic guide for continuous manual sweeps");
    btnToggleMetronome.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnToggleMetronome.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnToggleMetronome.onClick = [this] {
        setMetronomeMode(!isMetronomeMode);
    };
    addAndMakeVisible(btnToggleMetronome);

    metronomeWidget.onSecondTick = [this](int tick) {
        if (onMetronomeTick) onMetronomeTick(tick);
    };
    metronomeWidget.onSweepFinished = [this] {
        setMeasuringState(true);
        btnAccept.setButtonText(juce::String::fromUTF8("Sweep Finished \xE2\x9C\x93"));
        if (onAccept) onAccept();
    };
    metronomeWidget.onCloseRequested = [this] {
        setMetronomeMode(false);
    };
    addChildComponent(metronomeWidget);

    btnCancel.setButtonText("Cancel Session");
    btnCancel.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnCancel.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
    addAndMakeVisible(btnCancel);

    lblAutoStatus.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    lblAutoStatus.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
    lblAutoStatus.setJustificationType(juce::Justification::centredLeft);
    lblAutoStatus.setVisible(false);
    addAndMakeVisible(lblAutoStatus);

    btnCloseInspector.setButtonText("Close Inspector");
    btnCloseInspector.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnCloseInspector.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnCloseInspector.onClick = [this] {
        isInspectorMode = false;
        dismiss();
        if (onCloseInspector) onCloseInspector();
    };
    addChildComponent(btnCloseInspector);

    btnToggleCollapse.setButtonText(juce::String::fromUTF8(u8"▼"));
    btnToggleCollapse.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnToggleCollapse.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnToggleCollapse.onClick = [this] {
        isCollapsed = !isCollapsed;
        btnToggleCollapse.setButtonText(isCollapsed ? juce::String::fromUTF8(u8"▲") : juce::String::fromUTF8(u8"▼"));
        cardsViewport.setVisible(!isCollapsed && !isMetronomeMode);
        metronomeWidget.setVisible(!isCollapsed && isMetronomeMode);
        if (isInspectorMode)
        {
            btnCloseInspector.setVisible(!isCollapsed);
            lblAutoStatus.setVisible(!isCollapsed);
        }
        else if (isAutomatedMode)
        {
            btnCancel.setVisible(!isCollapsed);
        }
        else
        {
            btnAccept.setVisible(!isCollapsed);
            btnRepeat.setVisible(!isCollapsed);
            btnStepBack.setVisible(!isCollapsed);
            btnToggleMetronome.setVisible(!isCollapsed);
            btnCancel.setVisible(!isCollapsed);
        }
        if (onCollapseToggled) onCollapseToggled(isCollapsed);
        resized();
        repaint();
    };
    addAndMakeVisible(btnToggleCollapse);

    btnAccept.onClick = [this] {
        if (isMetronomeMode)
        {
            if (!metronomeWidget.getIsRunning())
            {
                metronomeWidget.startSweep(10.0);
                btnAccept.setButtonText("Pause Sweep [Space]");
            }
            else
            {
                metronomeWidget.stopSweep();
                btnAccept.setButtonText("Resume Sweep [Space]");
            }
        }
        else
        {
            setMeasuringState(true);
            if (onAccept) onAccept();
        }
    };
    btnRepeat.onClick = [this] {
        metronomeWidget.resetSweep();
        setMeasuringState(true);
        if (onRepeat) onRepeat();
    };
    btnStepBack.onClick = [this] {
        metronomeWidget.resetSweep();
        setMeasuringState(true);
        if (onStepBack) onStepBack();
    };
    btnCancel.onClick = [this] {
        metronomeWidget.stopSweep();
        dismiss();
        if (onCancel) onCancel();
    };

    addAndMakeVisible(cardsViewport);
    cardsViewport.setScrollBarsShown(false, true);
    cardsViewport.setViewedComponent(cardsContainer.get(), false);

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    setVisible(false);
}

OperatorStepModalDialog::~OperatorStepModalDialog()
{
    removeKeyListener(this);
}

void OperatorStepModalDialog::setMetronomeMode(bool active)
{
    isMetronomeMode = active;
    if (!isMetronomeMode)
    {
        metronomeWidget.stopSweep();
    }
    else
    {
        juce::String target = parameterSteps.empty() ? "" : parameterSteps[0].paramName;
        metronomeWidget.setTargetControlName(target);
        metronomeWidget.setInstructionText(juce::String::fromUTF8(u8"Gira la perilla a ritmo constante siguiendo los 10 pulsos sonoros"));
    }
    updateTheme();
    resized();
    repaint();
}

void OperatorStepModalDialog::updateTheme()
{
    btnAccept.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnAccept.setColour(juce::TextButton::textColourOffId, juce::Colours::black);

    btnRepeat.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnRepeat.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);

    btnStepBack.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnStepBack.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);

    btnCancel.setColour(juce::TextButton::buttonColourId, isAutomatedMode ? SoundIdTheme::surfaceSubtle : juce::Colours::transparentBlack);
    btnCancel.setColour(juce::TextButton::textColourOffId, isAutomatedMode ? SoundIdTheme::textPrimary : SoundIdTheme::textMuted);

    btnCloseInspector.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnCloseInspector.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);

    btnToggleCollapse.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnToggleCollapse.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);

    if (isInspectorMode)
        lblAutoStatus.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    else
        lblAutoStatus.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);

    if (isMetronomeMode)
    {
        btnToggleMetronome.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.2f));
        btnToggleMetronome.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
        btnAccept.setButtonText(metronomeWidget.getIsRunning() ? juce::String::fromUTF8(u8"Pausar Barrido [Space]")
                                                               : juce::String::fromUTF8(u8"Iniciar Barrido (10s) [Space]"));
    }
    else
    {
        btnToggleMetronome.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
        btnToggleMetronome.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
        if (!isMeasuring)
            btnAccept.setButtonText(juce::String::fromUTF8(u8"Accept Step [Space]"));
    }

    if (cardsContainer != nullptr)
        cardsContainer->repaint();
    repaint();
}

void OperatorStepModalDialog::setAutomatedMode(bool autoMode)
{
    isAutomatedMode = autoMode;
    isInspectorMode = false;
    updateTheme();
    btnCloseInspector.setVisible(false);
    btnAccept.setVisible(!autoMode && !isCollapsed);
    btnRepeat.setVisible(!autoMode && !isCollapsed);
    btnStepBack.setVisible(!autoMode && !isCollapsed);
    btnToggleMetronome.setVisible(!autoMode && !isCollapsed);
    lblAutoStatus.setVisible(autoMode);
    btnCancel.setVisible(!isCollapsed);
    btnCancel.setButtonText(autoMode ? "Stop Session" : "Cancel Session");
    resized();
    repaint();
}

void OperatorStepModalDialog::setMeasuringState(bool measuring)
{
    isMeasuring = measuring;
    if (isAutomatedMode)
    {
        lblAutoStatus.setText(measuring ? juce::String::fromUTF8(u8"● MEASURING AUDIO RESPONSE...")
                                        : juce::String::fromUTF8(u8"● STEPPING HARDWARE PARAMETERS VIA AUTOMATED SYSEX/MIDI"),
                              juce::dontSendNotification);
        lblAutoStatus.setVisible(true);
    }
    else
    {
        lblAutoStatus.setVisible(false);
        btnAccept.setEnabled(!measuring);
        if (isMetronomeMode)
        {
            btnAccept.setButtonText(measuring ? "Measuring Audio..."
                                              : (metronomeWidget.getIsRunning() ? juce::String::fromUTF8(u8"Pausar Barrido [Space]")
                                                                                : juce::String::fromUTF8(u8"Iniciar Barrido (10s) [Space]")));
        }
        else
        {
            btnAccept.setButtonText(measuring ? "Measuring Audio..." : juce::String::fromUTF8(u8"Accept Step [Space]"));
        }
        btnRepeat.setEnabled(!measuring);
        btnStepBack.setEnabled(!measuring && stepIndex > 1);
    }
    if (cardsContainer != nullptr)
        cardsContainer->repaint();
    repaint();
}

void OperatorStepModalDialog::dismiss()
{
    metronomeWidget.stopSweep();
    setVisible(false);
}

void OperatorStepModalDialog::setStepInfo(const juce::String& sessionTitle,
                                         int currentStep,
                                         int totalSteps,
                                         const std::vector<core::ParameterStep>& steps,
                                         const juce::String& message)
{
    updateTheme();
    testTitle = sessionTitle.isNotEmpty() ? sessionTitle : (isAutomatedMode ? "Automated Hardware Sweep" : "Manual Alignment Step");
    stepIndex = currentStep;
    stepTotal = totalSteps;
    parameterSteps = steps;
    promptMessage = message;
    isMeasuring = false;
    if (cardsContainer != nullptr)
        cardsContainer->setStepData(parameterSteps, promptMessage);

    if (!isAutomatedMode)
    {
        btnAccept.setEnabled(true);
        if (isMetronomeMode)
        {
            metronomeWidget.resetSweep();
            juce::String target = parameterSteps.empty() ? "" : parameterSteps[0].paramName;
            metronomeWidget.setTargetControlName(target);
            btnAccept.setButtonText(juce::String::fromUTF8(u8"Iniciar Barrido (10s) [Space]"));
        }
        else
        {
            btnAccept.setButtonText(juce::String::fromUTF8(u8"Accept Step [Space]"));
        }
        btnRepeat.setEnabled(true);
        btnStepBack.setEnabled(stepIndex > 1);
    }

    setVisible(true);
    resized();
    repaint();
}

void OperatorStepModalDialog::showInspector(const juce::String& sessionTitle,
                                           int currentStep,
                                           int totalSteps,
                                           const std::vector<core::ParameterStep>& steps,
                                           const juce::String& metricsInfo)
{
    isInspectorMode = true;
    isAutomatedMode = false;
    updateTheme();
    btnAccept.setVisible(false);
    btnRepeat.setVisible(false);
    btnStepBack.setVisible(false);
    btnCancel.setVisible(false);

    lblAutoStatus.setText(metricsInfo, juce::dontSendNotification);
    lblAutoStatus.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblAutoStatus.setVisible(!isCollapsed);

    btnCloseInspector.setVisible(!isCollapsed);

    testTitle = sessionTitle.isNotEmpty() ? ("INSPECTOR: " + sessionTitle) : "Point Controls Inspector";
    stepIndex = currentStep;
    stepTotal = totalSteps;
    parameterSteps = steps;
    promptMessage = metricsInfo;
    isMeasuring = false;
    if (cardsContainer != nullptr)
        cardsContainer->setStepData(parameterSteps, promptMessage);

    setVisible(true);
    resized();
    repaint();
}

void OperatorStepModalDialog::showStepPrompt(juce::Component* parent,
                                            const juce::String& sessionTitle,
                                            int currentStep,
                                            int totalSteps,
                                            const std::vector<core::ParameterStep>& steps,
                                            const juce::String& message)
{
    testTitle = sessionTitle.isNotEmpty() ? sessionTitle : (isAutomatedMode ? "Automated Hardware Sweep" : "Manual Alignment Step");
    stepIndex = currentStep;
    stepTotal = totalSteps;
    parameterSteps = steps;
    promptMessage = message;
    isMeasuring = false;
    if (cardsContainer != nullptr)
        cardsContainer->setStepData(parameterSteps, promptMessage);

    if (parent != nullptr && getParentComponent() == nullptr)
    {
        parent->addChildComponent(this);
    }

    setVisible(true);
    resized();
    repaint();
    grabKeyboardFocus();
}

bool OperatorStepModalDialog::keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/)
{
    if (!isVisible() || isMeasuring) return false;

    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
    {
        if (!isAutomatedMode && !isInspectorMode && btnAccept.isVisible() && btnAccept.isEnabled())
        {
            if (btnAccept.onClick != nullptr)
                btnAccept.onClick();
            else
                btnAccept.triggerClick();
            return true;
        }
    }
    else if (key == juce::KeyPress::escapeKey)
    {
        if (isMetronomeMode)
        {
            setMetronomeMode(false);
            return true;
        }
        if (btnCancel.isVisible() && btnCancel.isEnabled())
        {
            if (btnCancel.onClick != nullptr)
                btnCancel.onClick();
            else
                btnCancel.triggerClick();
            return true;
        }
    }
    else if (key == juce::KeyPress::backspaceKey)
    {
        if (btnStepBack.isVisible() && btnStepBack.isEnabled())
        {
            btnStepBack.triggerClick();
            return true;
        }
    }
    else if (key == juce::KeyPress('r', juce::ModifierKeys::noModifiers, 0))
    {
        if (btnRepeat.isVisible() && btnRepeat.isEnabled())
        {
            btnRepeat.triggerClick();
            return true;
        }
    }
    return false;
}

void OperatorStepModalDialog::paint(juce::Graphics& g)
{
    if (!isVisible()) return;

    auto card = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(card, 8.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(card, 8.0f, 1.0f);

    auto header = card.removeFromTop(30.0f).reduced(14.0f, 4.0f);
    header.removeFromRight(30.0f); // Reserve space for collapse button

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentGreen);

    juce::String stepTag = isInspectorMode ? ("POINT " + juce::String(stepIndex) + " / " + juce::String(stepTotal))
                                          : ("STEP " + juce::String(stepIndex) + " OF " + juce::String(stepTotal));
    g.drawText(stepTag, header.removeFromRight(130.0f), juce::Justification::centredRight, true);

    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(testTitle, header, juce::Justification::centredLeft, true);

    if (isCollapsed)
        return;

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawHorizontalLine(30, card.getX() + 10.0f, card.getRight() - 10.0f);

    if (parameterSteps.empty())
    {
        auto renderArea = card.removeFromTop(130.0f).reduced(20.0f, 10.0f);
        g.setFont(juce::FontOptions(13.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(promptMessage.isEmpty() ? "Adjust controls to target position and press Accept [Space]." : promptMessage,
                   renderArea, juce::Justification::centred, true);
    }
}

void OperatorStepModalDialog::resized()
{
    auto bounds = getLocalBounds();
    btnToggleCollapse.setBounds(bounds.getRight() - 32, 3, 26, 24);

    if (isCollapsed)
    {
        cardsViewport.setVisible(false);
        metronomeWidget.setVisible(false);
        btnAccept.setVisible(false);
        btnRepeat.setVisible(false);
        btnStepBack.setVisible(false);
        btnToggleMetronome.setVisible(false);
        btnCancel.setVisible(false);
        btnCloseInspector.setVisible(false);
        lblAutoStatus.setVisible(false);
        return;
    }

    bounds.removeFromTop(32); // Skip header

    auto bottomBar = bounds.removeFromBottom(38).reduced(12, 4);
    auto ctrlRect = bounds.reduced(6, 2);

    if (isMetronomeMode)
    {
        cardsViewport.setVisible(false);
        metronomeWidget.setVisible(true);
        metronomeWidget.setBounds(ctrlRect);
    }
    else
    {
        metronomeWidget.setVisible(false);
        cardsViewport.setVisible(true);
        int numCtrl = static_cast<int>(parameterSteps.size());
        if (numCtrl >= 3)
        {
            int totalContainerW = std::max(ctrlRect.getWidth(), numCtrl * 96 + 20);
            int totalContainerH = std::max(ctrlRect.getHeight(), 184);
            if (cardsContainer != nullptr)
                cardsContainer->setBounds(0, 0, totalContainerW, totalContainerH);
        }
        else
        {
            int totalContainerW = std::max(ctrlRect.getWidth(), numCtrl * 160);
            if (cardsContainer != nullptr)
                cardsContainer->setBounds(0, 0, totalContainerW, ctrlRect.getHeight());
        }
        cardsViewport.setBounds(ctrlRect);
    }

    if (isInspectorMode)
    {
        btnToggleMetronome.setVisible(false);
        btnCloseInspector.setVisible(true);
        btnCloseInspector.setBounds(bottomBar.removeFromRight(130));
        lblAutoStatus.setVisible(true);
        lblAutoStatus.setBounds(bottomBar);
    }
    else if (isAutomatedMode)
    {
        btnToggleMetronome.setVisible(false);
        btnCancel.setVisible(true);
        btnCancel.setBounds(bottomBar.removeFromRight(120));
        lblAutoStatus.setVisible(true);
        lblAutoStatus.setBounds(bottomBar);
    }
    else
    {
        btnAccept.setVisible(true);
        btnAccept.setBounds(bottomBar.removeFromRight(170));
        bottomBar.removeFromRight(8);
        btnRepeat.setVisible(true);
        btnRepeat.setBounds(bottomBar.removeFromRight(100));
        bottomBar.removeFromRight(8);
        btnStepBack.setVisible(stepIndex > 1);
        btnStepBack.setBounds(bottomBar.removeFromRight(95));
        bottomBar.removeFromRight(8);
        btnToggleMetronome.setVisible(true);
        btnToggleMetronome.setBounds(bottomBar.removeFromRight(135));
        btnCancel.setVisible(true);
        btnCancel.setBounds(bottomBar.removeFromLeft(110));
    }
}

juce::Rectangle<float> OperatorStepModalDialog::getCardBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    float numCtrl = static_cast<float>(parameterSteps.size());
    float targetW = numCtrl > 2 ? std::max(560.0f, numCtrl * 150.0f + 40.0f) : 540.0f;
    float maxW = bounds.getWidth() > 10.0f ? bounds.getWidth() * 0.95f : 800.0f;
    float cardW = std::min(targetW, maxW);
    float cardH = 340.0f;
    return bounds.withSizeKeepingCentre(cardW, cardH);
}

} // namespace abdaudiolab::gui
