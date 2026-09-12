#include "OperatorCardsContainerComponent.h"
#include "SoundIdTheme.h"
#include "HardwareControlRenderer.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::gui
{

OperatorCardsContainerComponent::OperatorCardsContainerComponent()
{
    setInterceptsMouseClicks(true, false);
}

void OperatorCardsContainerComponent::setStepData(const std::vector<core::ParameterStep>& steps, const juce::String& promptMsg)
{
    parameterSteps = steps;
    promptMessage = promptMsg;
    selectedParamIndex = 0;
    repaint();
}

void OperatorCardsContainerComponent::setSelectedParamIndex(int index)
{
    int clamped = parameterSteps.empty() ? 0 : std::clamp(index, 0, static_cast<int>(parameterSteps.size()) - 1);
    if (selectedParamIndex != clamped)
    {
        selectedParamIndex = clamped;
        if (onSelectionChanged)
            onSelectionChanged(selectedParamIndex);
        repaint();
    }
}

void OperatorCardsContainerComponent::mouseDown(const juce::MouseEvent& e)
{
    if (parameterSteps.size() >= 3 && e.position.y <= 102.0f)
    {
        int clickedIdx = static_cast<int>((e.position.x - 6.0f) / 96.0f);
        if (clickedIdx >= 0 && clickedIdx < static_cast<int>(parameterSteps.size()))
        {
            setSelectedParamIndex(clickedIdx);
        }
    }
}

void OperatorCardsContainerComponent::paint(juce::Graphics& g)
{
    if (parameterSteps.empty())
        return;

    float w = static_cast<float>(getWidth());
    float areaH = static_cast<float>(getHeight());

    if (parameterSteps.size() <= 2)
    {
        layoutDualCardView(g, w, areaH);
    }
    else
    {
        layoutMultiControlGridView(g, w, areaH);
    }
}

void OperatorCardsContainerComponent::layoutDualCardView(juce::Graphics& g, float w, float areaH)
{
    if (parameterSteps.size() == 1)
    {
        const auto& ps = parameterSteps[0];
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

            if (promptMessage.isNotEmpty() && contentArea.getHeight() >= 16.0f)
            {
                auto promptRow = contentArea.removeFromTop(18.0f);
                g.setFont(juce::FontOptions(10.0f, juce::Font::italic));
                g.setColour(SoundIdTheme::textMuted);
                g.drawText(promptMessage, promptRow, juce::Justification::centredLeft, true);
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
    float totalCardsW = static_cast<float>(parameterSteps.size()) * itemW;
    float startX = (totalCardsW < w) ? (w - totalCardsW) * 0.5f : 0.0f;

    for (size_t i = 0; i < parameterSteps.size(); ++i)
    {
        auto ctrlArea = juce::Rectangle<float>(startX + static_cast<float>(i) * itemW, 0.0f, itemW, areaH);
        drawControl(g, ctrlArea, parameterSteps[i]);
    }
}

void OperatorCardsContainerComponent::layoutMultiControlGridView(juce::Graphics& g, float w, float /*areaH*/)
{
    // 1. Horizontal row of compact micro-cards
    for (size_t i = 0; i < parameterSteps.size(); ++i)
    {
        auto cardArea = juce::Rectangle<float>(6.0f + static_cast<float>(i) * 96.0f, 2.0f, 90.0f, 98.0f);
        bool isSelected = (static_cast<int>(i) == selectedParamIndex);

        g.setColour(SoundIdTheme::bgCardHover.withAlpha(isSelected ? 0.85f : 0.35f));
        g.fillRoundedRectangle(cardArea, 6.0f);

        g.setColour(isSelected ? SoundIdTheme::accentGreen : SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(cardArea, 6.0f, isSelected ? 1.5f : 1.0f);

        drawControl(g, cardArea.reduced(2.0f), parameterSteps[i]);
    }

    // 2. Full-width Telemetry & Specification card for selected parameter
    int selIdx = std::clamp(selectedParamIndex, 0, static_cast<int>(parameterSteps.size()) - 1);
    const auto& selPs = parameterSteps[static_cast<size_t>(selIdx)];

    float infoY = 104.0f;
    float infoH = 74.0f;
    auto infoArea = juce::Rectangle<float>(6.0f, infoY, std::max(w - 12.0f, static_cast<float>(parameterSteps.size()) * 96.0f), infoH);
    g.setColour(SoundIdTheme::bgCardHover.withAlpha(0.6f));
    g.fillRoundedRectangle(infoArea, 6.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(infoArea, 6.0f, 1.0f);

    auto contentArea = infoArea.reduced(12.0f, 6.0f);
    auto headerRow = contentArea.removeFromTop(16.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText("SELECTED PARAMETER TELEMETRY & SPECIFICATIONS [" + juce::String(selIdx + 1) + "/" + juce::String(parameterSteps.size()) + "]",
               headerRow, juce::Justification::centredLeft, true);

    auto titleRow = contentArea.removeFromTop(18.0f);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    juce::String descName = juce::String(selPs.paramName).toUpperCase() + " [" + juce::String(selPs.controlType) + "]";
    g.drawText(descName, titleRow, juce::Justification::centredLeft, true);

    auto readoutRow = contentArea.removeFromTop(16.0f);
    int pct = static_cast<int>(std::round(selPs.normalizedValue * 100.0f));
    juce::String normStr = "Target Value: " + juce::String(pct) + "% (" + juce::String(selPs.normalizedValue, 3) + " norm)";
    juce::String rangeStr = "Target Range: " + juce::String(static_cast<int>(selPs.minNormalized * 100.0f)) + "% - " +
                            juce::String(static_cast<int>(selPs.maxNormalized * 100.0f)) + "%";

    g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(normStr + "   |   " + rangeStr, readoutRow, juce::Justification::centredLeft, true);

    if (promptMessage.isNotEmpty() && contentArea.getHeight() >= 14.0f)
    {
        auto promptRow = contentArea.removeFromTop(16.0f);
        g.setFont(juce::FontOptions(10.0f, juce::Font::italic));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText(promptMessage, promptRow, juce::Justification::centredLeft, true);
    }
}

void OperatorCardsContainerComponent::drawControl(juce::Graphics& g, juce::Rectangle<float> ctrlArea, const core::ParameterStep& ps)
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

} // namespace abdaudiolab::gui
