#pragma once

#include "SoundIdTheme.h"
#include "HardwareControlRenderer.h"
#include "../core/ProfilingSession.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cmath>

namespace abdaudiolab::gui
{

/**
 * @brief Graphical Operator Step Modal for ABDAudioLab.
 * Displays vector rendered Knobs, Sliders, and Female Jack Ports
 * showing exact target positions for manual hardware alignment.
 */
class OperatorStepModalDialog : public juce::Component,
                                public juce::KeyListener
{
public:
    OperatorStepModalDialog()
    {
        setWantsKeyboardFocus(true);
        addKeyListener(this);

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

        btnCancel.setButtonText("Cancel Session");
        btnCancel.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btnCancel.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
        addAndMakeVisible(btnCancel);

        btnAccept.onClick = [this] {
            setVisible(false);
            if (onAccept) onAccept();
        };
        btnRepeat.onClick = [this] {
            setVisible(false);
            if (onRepeat) onRepeat();
        };
        btnStepBack.onClick = [this] {
            setVisible(false);
            if (onStepBack) onStepBack();
        };
        btnCancel.onClick = [this] {
            setVisible(false);
            if (onCancel) onCancel();
        };

        addAndMakeVisible(cardsViewport);
        cardsViewport.setScrollBarsShown(false, true);
        cardsViewport.setViewedComponent(&cardsContainer, false);

        setVisible(false);
    }

    ~OperatorStepModalDialog() override
    {
        removeKeyListener(this);
    }

    std::function<void()> onAccept;
    std::function<void()> onRepeat;
    std::function<void()> onStepBack;
    std::function<void()> onCancel;

    void setStepInfo(const juce::String& sessionTitle,
                     int currentStep,
                     int totalSteps,
                     const std::vector<core::ParameterStep>& steps,
                     const juce::String& message = {})
    {
        showStepPrompt(getParentComponent(), sessionTitle, currentStep, totalSteps, steps, message);
    }

    void showStepPrompt(juce::Component* parent,
                        const juce::String& sessionTitle,
                        int currentStep,
                        int totalSteps,
                        const std::vector<core::ParameterStep>& steps,
                        const juce::String& message = {})
    {
        testTitle = sessionTitle.isNotEmpty() ? sessionTitle : "Manual Alignment Step";
        stepIndex = currentStep;
        stepTotal = totalSteps;
        parameterSteps = steps;
        promptMessage = message;

        if (parent != nullptr)
        {
            parent->addAndMakeVisible(this);
            setBounds(parent->getLocalBounds());
            toFront(true);
        }

        setVisible(true);
        resized();
        repaint();
        grabKeyboardFocus();
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) override
    {
        if (!isVisible()) return false;

        if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
        {
            btnAccept.triggerClick();
            return true;
        }
        else if (key == juce::KeyPress::escapeKey)
        {
            btnCancel.triggerClick();
            return true;
        }
        else if (key == juce::KeyPress::backspaceKey)
        {
            btnStepBack.triggerClick();
            return true;
        }
        else if (key == juce::KeyPress('r', juce::ModifierKeys::noModifiers, 0))
        {
            btnRepeat.triggerClick();
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        if (!isVisible()) return;

        g.fillAll(juce::Colours::black.withAlpha(0.55f));

        auto card = getCardBounds();
        g.setColour(SoundIdTheme::bgCard);
        g.fillRoundedRectangle(card, 12.0f);
        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(card, 12.0f, 1.5f);

        auto header = card.removeFromTop(44.0f).reduced(20.0f, 10.0f);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);

        juce::String stepTag = "STEP " + juce::String(stepIndex) + " OF " + juce::String(stepTotal);
        g.drawText(stepTag, header.removeFromRight(120.0f), juce::Justification::right, true);

        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(testTitle, header, juce::Justification::left, true);

        g.setColour(SoundIdTheme::borderSubtle);
        g.drawHorizontalLine(static_cast<int>(card.getY()), card.getX() + 16.0f, card.getRight() - 16.0f);

        if (parameterSteps.empty())
        {
            auto renderArea = card.removeFromTop(200.0f).reduced(20.0f, 10.0f);
            g.setFont(juce::FontOptions(14.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(promptMessage.isEmpty() ? "Adjust controls to target position and press Accept [Space]." : promptMessage,
                       renderArea, juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        auto card = getCardBounds();
        auto renderArea = card;
        renderArea.removeFromTop(48.0f);
        auto ctrlRect = renderArea.removeFromTop(200.0f).reduced(16.0f, 10.0f);

        int numCtrl = static_cast<int>(parameterSteps.size());
        int totalContainerW = std::max(static_cast<int>(ctrlRect.getWidth()), numCtrl * 150);
        cardsContainer.setBounds(0, 0, totalContainerW, static_cast<int>(ctrlRect.getHeight()));
        cardsViewport.setBounds(ctrlRect.toNearestInt());

        auto bottomBar = card.removeFromBottom(52.0f).reduced(16.0f, 10.0f);

        btnAccept.setBounds(bottomBar.removeFromRight(150.0f).toNearestInt());
        bottomBar.removeFromRight(8.0f);
        btnRepeat.setBounds(bottomBar.removeFromRight(100.0f).toNearestInt());
        bottomBar.removeFromRight(8.0f);
        btnStepBack.setBounds(bottomBar.removeFromRight(95.0f).toNearestInt());

        btnCancel.setBounds(bottomBar.removeFromLeft(110.0f).toNearestInt());
    }

private:
    class CardsContainerComponent : public juce::Component
    {
    public:
        CardsContainerComponent(OperatorStepModalDialog& ownerRef) : owner(ownerRef)
        {
            setInterceptsMouseClicks(false, false);
        }

        void paint(juce::Graphics& g) override
        {
            const auto& steps = owner.parameterSteps;
            if (steps.empty()) return;

            float itemW = 150.0f;
            float areaH = static_cast<float>(getHeight());

            for (size_t i = 0; i < steps.size(); ++i)
            {
                auto ctrlArea = juce::Rectangle<float>(static_cast<float>(i) * itemW, 0.0f, itemW, areaH);
                const auto& ps = steps[i];

                if (ps.controlType == "JackPort" || ps.controlType == "Jack" || ps.controlType == "Port")
                {
                    HardwareControlRenderer::drawJackPort(g, ctrlArea, ps);
                }
                else if (ps.controlType == "Button" || ps.controlType == "Push" || ps.controlType == "Toggle")
                {
                    HardwareControlRenderer::drawButton(g, ctrlArea, ps);
                }
                else if (ps.controlType == "Switch" || ps.controlType == "RotarySwitch" || ps.controlType == "Selector")
                {
                    HardwareControlRenderer::drawSwitch(g, ctrlArea, ps);
                }
                else if (ps.controlType == "Slider")
                {
                    HardwareControlRenderer::drawSlider(g, ctrlArea, ps);
                }
                else
                {
                    HardwareControlRenderer::drawKnob(g, ctrlArea, ps);
                }
            }
        }

    private:
        OperatorStepModalDialog& owner;
    };

    CardsContainerComponent cardsContainer { *this };
    juce::Viewport cardsViewport;

    juce::TextButton btnAccept;
    juce::TextButton btnRepeat;
    juce::TextButton btnStepBack;
    juce::TextButton btnCancel;

    juce::String testTitle { "Manual Calibration Step" };
    juce::String promptMessage;
    int stepIndex { 1 };
    int stepTotal { 1 };
    std::vector<core::ParameterStep> parameterSteps;

    juce::Rectangle<float> getCardBounds() const
    {
        auto bounds = getLocalBounds().toFloat();
        float numCtrl = static_cast<float>(parameterSteps.size());
        float targetW = numCtrl > 2 ? std::max(560.0f, numCtrl * 150.0f + 40.0f) : 540.0f;
        float maxW = bounds.getWidth() > 10.0f ? bounds.getWidth() * 0.95f : 800.0f;
        float cardW = std::min(targetW, maxW);
        float cardH = 340.0f;
        return bounds.withSizeKeepingCentre(cardW, cardH);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OperatorStepModalDialog)
};

} // namespace abdaudiolab::gui
