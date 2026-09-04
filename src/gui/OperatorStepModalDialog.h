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
            cardsViewport.setVisible(!isCollapsed);
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
                btnCancel.setVisible(!isCollapsed);
            }
            if (onCollapseToggled) onCollapseToggled(isCollapsed);
            resized();
            repaint();
        };
        addAndMakeVisible(btnToggleCollapse);

        btnAccept.onClick = [this] {
            setMeasuringState(true);
            if (onAccept) onAccept();
        };
        btnRepeat.onClick = [this] {
            setMeasuringState(true);
            if (onRepeat) onRepeat();
        };
        btnStepBack.onClick = [this] {
            setMeasuringState(true);
            if (onStepBack) onStepBack();
        };
        btnCancel.onClick = [this] {
            dismiss();
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
    std::function<void(bool isCollapsed)> onCollapseToggled;
    std::function<void()> onCloseInspector;

    bool isAutomatedMode { false };
    bool isMeasuring { false };
    bool isCollapsed { false };
    bool isInspectorMode { false };

    void updateTheme()
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

        cardsContainer.repaint();
        repaint();
    }

    void setAutomatedMode(bool autoMode)
    {
        isAutomatedMode = autoMode;
        isInspectorMode = false;
        updateTheme();
        btnCloseInspector.setVisible(false);
        btnAccept.setVisible(!autoMode && !isCollapsed);
        btnRepeat.setVisible(!autoMode && !isCollapsed);
        btnStepBack.setVisible(!autoMode && !isCollapsed);
        lblAutoStatus.setVisible(autoMode);
        btnCancel.setVisible(!isCollapsed);
        btnCancel.setButtonText(autoMode ? "Stop Session" : "Cancel Session");
        resized();
        repaint();
    }

    void setMeasuringState(bool measuring)
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
            btnAccept.setButtonText(measuring ? "Measuring Audio..." : juce::String::fromUTF8(u8"Accept Step [Space]"));
            btnRepeat.setEnabled(!measuring);
            btnStepBack.setEnabled(!measuring && stepIndex > 1);
        }
        cardsContainer.repaint();
        repaint();
    }

    void dismiss()
    {
        setVisible(false);
    }

    void setStepInfo(const juce::String& sessionTitle,
                     int currentStep,
                     int totalSteps,
                     const std::vector<core::ParameterStep>& steps,
                     const juce::String& message = {})
    {
        updateTheme();
        testTitle = sessionTitle.isNotEmpty() ? sessionTitle : (isAutomatedMode ? "Automated Hardware Sweep" : "Manual Alignment Step");
        stepIndex = currentStep;
        stepTotal = totalSteps;
        parameterSteps = steps;
        promptMessage = message;
        isMeasuring = false;
        cardsContainer.selectedParamIndex = 0;

        if (!isAutomatedMode)
        {
            btnAccept.setEnabled(true);
            btnAccept.setButtonText(juce::String::fromUTF8(u8"Accept Step [Space]"));
            btnRepeat.setEnabled(true);
            btnStepBack.setEnabled(stepIndex > 1);
        }

        setVisible(true);
        resized();
        cardsContainer.repaint();
        repaint();
    }

    void showInspector(const juce::String& sessionTitle,
                       int currentStep,
                       int totalSteps,
                       const std::vector<core::ParameterStep>& steps,
                       const juce::String& metricsInfo = {})
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
        cardsContainer.selectedParamIndex = 0;

        setVisible(true);
        resized();
        cardsContainer.repaint();
        repaint();
    }

    void showStepPrompt(juce::Component* parent,
                        const juce::String& sessionTitle,
                        int currentStep,
                        int totalSteps,
                        const std::vector<core::ParameterStep>& steps,
                        const juce::String& message = {})
    {
        testTitle = sessionTitle.isNotEmpty() ? sessionTitle : (isAutomatedMode ? "Automated Hardware Sweep" : "Manual Alignment Step");
        stepIndex = currentStep;
        stepTotal = totalSteps;
        parameterSteps = steps;
        promptMessage = message;
        isMeasuring = false;

        if (parent != nullptr && getParentComponent() == nullptr)
        {
            parent->addChildComponent(this);
        }

        setVisible(true);
        resized();
        cardsContainer.repaint();
        repaint();
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

    void resized() override
    {
        auto bounds = getLocalBounds();
        btnToggleCollapse.setBounds(bounds.getRight() - 32, 3, 26, 24);

        if (isCollapsed)
        {
            cardsViewport.setVisible(false);
            btnAccept.setVisible(false);
            btnRepeat.setVisible(false);
            btnStepBack.setVisible(false);
            btnCancel.setVisible(false);
            btnCloseInspector.setVisible(false);
            lblAutoStatus.setVisible(false);
            return;
        }

        cardsViewport.setVisible(true);
        bounds.removeFromTop(32); // Skip header

        auto bottomBar = bounds.removeFromBottom(38).reduced(12, 4);

        auto ctrlRect = bounds.reduced(6, 2);
        int numCtrl = static_cast<int>(parameterSteps.size());
        if (numCtrl >= 3)
        {
            int totalContainerW = std::max(ctrlRect.getWidth(), numCtrl * 96 + 20);
            int totalContainerH = std::max(ctrlRect.getHeight(), 184);
            cardsContainer.setBounds(0, 0, totalContainerW, totalContainerH);
        }
        else
        {
            int totalContainerW = std::max(ctrlRect.getWidth(), numCtrl * 160);
            cardsContainer.setBounds(0, 0, totalContainerW, ctrlRect.getHeight());
        }
        cardsViewport.setBounds(ctrlRect);

        if (isInspectorMode)
        {
            btnCloseInspector.setVisible(true);
            btnCloseInspector.setBounds(bottomBar.removeFromRight(130));
            lblAutoStatus.setVisible(true);
            lblAutoStatus.setBounds(bottomBar);
        }
        else if (isAutomatedMode)
        {
            btnCancel.setVisible(true);
            btnCancel.setBounds(bottomBar.removeFromRight(120));
            lblAutoStatus.setVisible(true);
            lblAutoStatus.setBounds(bottomBar);
        }
        else
        {
            btnAccept.setVisible(true);
            btnAccept.setBounds(bottomBar.removeFromRight(150));
            bottomBar.removeFromRight(8);
            btnRepeat.setVisible(true);
            btnRepeat.setBounds(bottomBar.removeFromRight(100));
            bottomBar.removeFromRight(8);
            btnStepBack.setVisible(stepIndex > 1);
            btnStepBack.setBounds(bottomBar.removeFromRight(95));
            btnCancel.setVisible(true);
            btnCancel.setBounds(bottomBar.removeFromLeft(110));
        }
    }

private:
    class CardsContainerComponent : public juce::Component
    {
    public:
        CardsContainerComponent(OperatorStepModalDialog& ownerRef) : owner(ownerRef)
        {
            setInterceptsMouseClicks(true, false);
        }

        int selectedParamIndex { 0 };

        void mouseDown(const juce::MouseEvent& e) override
        {
            const auto& steps = owner.parameterSteps;
            if (steps.size() >= 3 && e.position.y <= 102.0f)
            {
                int clickedIdx = static_cast<int>((e.position.x - 6.0f) / 96.0f);
                if (clickedIdx >= 0 && clickedIdx < static_cast<int>(steps.size()))
                {
                    selectedParamIndex = clickedIdx;
                    repaint();
                }
            }
        }

        void paint(juce::Graphics& g) override
        {
            const auto& steps = owner.parameterSteps;
            if (steps.empty()) return;

            float w = static_cast<float>(getWidth());
            float areaH = static_cast<float>(getHeight());

            if (steps.size() <= 2)
            {
                layoutDualCardView(g, w, areaH);
            }
            else
            {
                layoutMultiControlGridView(g, w, areaH);
            }
        }

    private:
        void layoutDualCardView(juce::Graphics& g, float w, float areaH)
        {
            const auto& steps = owner.parameterSteps;
            if (steps.size() == 1)
            {
                const auto& ps = steps[0];
                if (w >= 400.0f)
                {
                    float ctrlW = 160.0f;
                    auto ctrlArea = juce::Rectangle<float>(12.0f, 0.0f, ctrlW, areaH);
                    drawControl(g, ctrlArea, ps);

                    // Right Telemetry Card
                    auto infoArea = juce::Rectangle<float>(ctrlW + 20.0f, 4.0f, w - (ctrlW + 32.0f), areaH - 8.0f);
                    g.setColour(SoundIdTheme::bgCardHover.withAlpha(0.6f));
                    g.fillRoundedRectangle(infoArea, 6.0f);
                    g.setColour(SoundIdTheme::borderSubtle);
                    g.drawRoundedRectangle(infoArea, 6.0f, 1.0f);

                    auto contentArea = infoArea.reduced(14.0f, 8.0f);
                    auto headerRow = contentArea.removeFromTop(18.0f);
                    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                    g.setColour(SoundIdTheme::accentGreen);
                    g.drawText("PARAMETER TELEMETRY & SPECIFICATION", headerRow, juce::Justification::centredLeft, true);

                    auto titleRow = contentArea.removeFromTop(20.0f);
                    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
                    g.setColour(SoundIdTheme::textPrimary);
                    juce::String descName = juce::String(ps.paramName).toUpperCase() + " [" + juce::String(ps.controlType) + "]";
                    g.drawText(descName, titleRow, juce::Justification::centredLeft, true);

                    if (contentArea.getHeight() >= 24.0f)
                    {
                        auto readoutRow = contentArea.removeFromTop(22.0f);
                        int pct = static_cast<int>(std::round(ps.normalizedValue * 100.0f));
                        juce::String normStr = "Target: " + juce::String(pct) + "% (" + juce::String(ps.normalizedValue, 3) + " norm)";
                        juce::String rangeStr = "Range: " + juce::String(static_cast<int>(ps.minNormalized * 100.0f)) + "% - " +
                                                juce::String(static_cast<int>(ps.maxNormalized * 100.0f)) + "%";

                        g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
                        g.setColour(SoundIdTheme::textSecondary);
                        g.drawText(normStr + "   |   " + rangeStr, readoutRow, juce::Justification::centredLeft, true);
                    }

                    if (owner.promptMessage.isNotEmpty() && contentArea.getHeight() >= 16.0f)
                    {
                        auto promptRow = contentArea.removeFromTop(18.0f);
                        g.setFont(juce::FontOptions(10.0f, juce::Font::italic));
                        g.setColour(SoundIdTheme::textMuted);
                        g.drawText(owner.promptMessage, promptRow, juce::Justification::centredLeft, true);
                    }
                    return;
                }
                else
                {
                    float itemW = 150.0f;
                    float startX = std::max(0.0f, (w - itemW) * 0.5f);
                    auto ctrlArea = juce::Rectangle<float>(startX, 0.0f, itemW, areaH);
                    drawControl(g, ctrlArea, ps);
                    return;
                }
            }

            float itemW = 150.0f;
            float totalCardsW = static_cast<float>(steps.size()) * itemW;
            float startX = (totalCardsW < w) ? (w - totalCardsW) * 0.5f : 0.0f;

            for (size_t i = 0; i < steps.size(); ++i)
            {
                auto ctrlArea = juce::Rectangle<float>(startX + static_cast<float>(i) * itemW, 0.0f, itemW, areaH);
                drawControl(g, ctrlArea, steps[i]);
            }
        }

        void layoutMultiControlGridView(juce::Graphics& g, float w, float /*areaH*/)
        {
            const auto& steps = owner.parameterSteps;

            // 1. Horizontal row of compact micro-cards
            for (size_t i = 0; i < steps.size(); ++i)
            {
                auto cardArea = juce::Rectangle<float>(6.0f + static_cast<float>(i) * 96.0f, 2.0f, 90.0f, 98.0f);
                bool isSelected = (static_cast<int>(i) == selectedParamIndex);

                g.setColour(SoundIdTheme::bgCardHover.withAlpha(isSelected ? 0.85f : 0.35f));
                g.fillRoundedRectangle(cardArea, 6.0f);

                g.setColour(isSelected ? SoundIdTheme::accentGreen : SoundIdTheme::borderSubtle);
                g.drawRoundedRectangle(cardArea, 6.0f, isSelected ? 1.5f : 1.0f);

                drawControl(g, cardArea.reduced(2.0f), steps[i]);
            }

            // 2. Full-width Telemetry & Specification card for selected parameter
            int selIdx = std::clamp(selectedParamIndex, 0, static_cast<int>(steps.size()) - 1);
            const auto& selPs = steps[static_cast<size_t>(selIdx)];

            float infoY = 104.0f;
            float infoH = 74.0f;
            auto infoArea = juce::Rectangle<float>(6.0f, infoY, std::max(w - 12.0f, static_cast<float>(steps.size()) * 96.0f), infoH);
            g.setColour(SoundIdTheme::bgCardHover.withAlpha(0.6f));
            g.fillRoundedRectangle(infoArea, 6.0f);
            g.setColour(SoundIdTheme::borderSubtle);
            g.drawRoundedRectangle(infoArea, 6.0f, 1.0f);

            auto contentArea = infoArea.reduced(12.0f, 6.0f);
            auto headerRow = contentArea.removeFromTop(16.0f);
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText("SELECTED PARAMETER TELEMETRY & SPECIFICATION [" + juce::String(selIdx + 1) + "/" + juce::String(steps.size()) + "]",
                       headerRow, juce::Justification::centredLeft, true);

            auto titleRow = contentArea.removeFromTop(18.0f);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            juce::String descName = juce::String(selPs.paramName).toUpperCase() + " [" + juce::String(selPs.controlType) + "]";
            g.drawText(descName, titleRow, juce::Justification::centredLeft, true);

            auto readoutRow = contentArea.removeFromTop(16.0f);
            int pct = static_cast<int>(std::round(selPs.normalizedValue * 100.0f));
            juce::String normStr = "Target: " + juce::String(pct) + "% (" + juce::String(selPs.normalizedValue, 3) + " norm)";
            juce::String rangeStr = "Range: " + juce::String(static_cast<int>(selPs.minNormalized * 100.0f)) + "% - " +
                                    juce::String(static_cast<int>(selPs.maxNormalized * 100.0f)) + "%";

            g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(normStr + "   |   " + rangeStr, readoutRow, juce::Justification::centredLeft, true);

            if (owner.promptMessage.isNotEmpty() && contentArea.getHeight() >= 14.0f)
            {
                auto promptRow = contentArea.removeFromTop(16.0f);
                g.setFont(juce::FontOptions(10.0f, juce::Font::italic));
                g.setColour(SoundIdTheme::textMuted);
                g.drawText(owner.promptMessage, promptRow, juce::Justification::centredLeft, true);
            }
        }

        static void drawControl(juce::Graphics& g, juce::Rectangle<float> ctrlArea, const core::ParameterStep& ps)
        {
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

    private:
        OperatorStepModalDialog& owner;
    };

    CardsContainerComponent cardsContainer { *this };
    juce::Viewport cardsViewport;

    juce::TextButton btnAccept;
    juce::TextButton btnRepeat;
    juce::TextButton btnStepBack;
    juce::TextButton btnCancel;
    juce::TextButton btnCloseInspector;
    juce::TextButton btnToggleCollapse;
    juce::Label lblAutoStatus;

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
