#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"
#include "controllers/CanonicalWorkflowTypes.h"
#include <map>
#include <vector>
#include <optional>
#include <functional>

namespace abdaudiolab::gui
{

class WorkflowStepperBar : public juce::Component
{
public:
    using Step = CanonicalStep;
    using StepStatus = CanonicalStepStatus;

    WorkflowStepperBar() : currentStep(Step::SystemInfo)
    {
        stepStatuses[Step::SystemInfo]        = StepStatus::Current;
        stepStatuses[Step::HardwareRouting]   = StepStatus::Pending;
        stepStatuses[Step::CalibrateLoopback] = StepStatus::Pending;
        stepStatuses[Step::RunSession]        = StepStatus::Pending;
        stepStatuses[Step::ExportReport]      = StepStatus::Pending;

        stepNames[Step::SystemInfo]        = "0. Studio Environment";
        stepNames[Step::HardwareRouting]   = "1. Target & Routing";
        stepNames[Step::CalibrateLoopback] = "2. Calibration & Setup";
        stepNames[Step::RunSession]        = "3. Run Session";
        stepNames[Step::ExportReport]      = "4. Export & Report";
    }

    ~WorkflowStepperBar() override = default;

    // --- State Machine Logic & Query ---
    
    [[nodiscard]] Step getCurrentStep() const noexcept { return currentStep; }

    void setCurrentStep(Step targetStep)
    {
        if (currentStep == targetStep) return;

        if (stepStatuses[currentStep] == StepStatus::Current)
            stepStatuses[currentStep] = StepStatus::Completed;

        currentStep = targetStep;
        stepStatuses[currentStep] = StepStatus::Current;
        repaint();
    }

    void setStepStatus(Step step, StepStatus status)
    {
        stepStatuses[step] = status;
        repaint();
    }

    [[nodiscard]] StepStatus getStepStatus(Step step) const
    {
        auto it = stepStatuses.find(step);
        return it != stepStatuses.end() ? it->second : StepStatus::Pending;
    }

    void setStepLocked(Step step, bool locked)
    {
        lockedSteps[step] = locked;
        repaint();
    }

    [[nodiscard]] bool isStepLocked(Step step) const
    {
        auto it = lockedSteps.find(step);
        return it != lockedSteps.end() && it->second;
    }

    [[nodiscard]] bool canNavigateTo(Step step) const
    {
        if (isStepLocked(step))
            return false;
        return true;
    }

    std::function<void(Step targetStep)> onStepSelected;

protected:
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

        const int numSteps = 5;
        const float segmentWidth = bounds.getWidth() / static_cast<float>(numSteps);
        const float centerY = bounds.getCentreY();
        const float nodeRadius = 10.0f;

        const juce::Colour accentGreen  = SoundIdTheme::accentGreen;
        const juce::Colour accentAmber  = SoundIdTheme::accentAmber;
        const juce::Colour textPrimary  = getLookAndFeel().findColour(juce::Label::textColourId);
        const juce::Colour textMuted    = textPrimary.withAlpha(0.45f);
        const juce::Colour borderSubtle = textPrimary.withAlpha(0.15f);

        // Pre-compute node center positions
        std::vector<juce::Point<float>> nodeCenters;
        nodeCenters.reserve(numSteps);
        for (int i = 0; i < numSteps; ++i)
        {
            nodeCenters.push_back({ (static_cast<float>(i) * segmentWidth) + (segmentWidth * 0.5f), centerY });
        }

        static constexpr Step visualOrder[5] = {
            Step::SystemInfo,
            Step::HardwareRouting,
            Step::CalibrateLoopback,
            Step::RunSession,
            Step::ExportReport
        };

        // 1. Draw connecting gradient lines respecting text width
        for (size_t i = 0; i < nodeCenters.size() - 1; ++i)
        {
            auto p1 = nodeCenters[i];
            auto p2 = nodeCenters[i + 1];

            Step stepCurrent = visualOrder[i];
            Step stepNext    = visualOrder[i + 1];

            // Measure actual text width of current step
            g.setFont(juce::FontOptions(12.0f, (stepCurrent == currentStep) ? juce::Font::bold : juce::Font::plain));
            float textWidth = static_cast<float>(g.getCurrentFont().getStringWidth(stepNames[stepCurrent]));
            float startX = p1.getX() + nodeRadius + 12.0f + textWidth + 8.0f;
            float endX = p2.getX() - (nodeRadius + 6.0f);

            if (startX < endX)
            {
                juce::Colour c1 = borderSubtle;
                if (getStepStatus(stepCurrent) == StepStatus::Completed)
                    c1 = accentGreen;
                else if (getStepStatus(stepCurrent) == StepStatus::Skipped)
                    c1 = accentAmber;
                else if (getStepStatus(stepCurrent) == StepStatus::Current)
                    c1 = accentGreen.withAlpha(0.5f);

                juce::Colour c2 = borderSubtle;
                if (getStepStatus(stepNext) == StepStatus::Current || getStepStatus(stepNext) == StepStatus::Completed)
                    c2 = accentGreen.withAlpha(0.35f);
                else if (getStepStatus(stepNext) == StepStatus::Skipped)
                    c2 = accentAmber.withAlpha(0.35f);

                juce::ColourGradient grad(c1, { startX, centerY }, c2, { endX, centerY }, false);
                g.setGradientFill(grad);
                g.drawLine(startX, centerY, endX, centerY, 2.0f);
            }
        }

        // 2. Draw nodes and label indications
        for (int i = 0; i < numSteps; ++i)
        {
            Step step = visualOrder[i];
            auto center = nodeCenters[static_cast<size_t>(i)];
            auto status = getStepStatus(step);

            juce::Colour bubbleColour = borderSubtle;
            juce::Colour textColour = textMuted;
            bool isCurrent = (step == currentStep);

            if (status == StepStatus::Completed)
            {
                bubbleColour = accentGreen;
                textColour = accentGreen;
            }
            else if (status == StepStatus::Skipped)
            {
                bubbleColour = accentAmber;
                textColour = accentAmber;
            }
            else if (status == StepStatus::Current)
            {
                bubbleColour = textPrimary;
                textColour = textPrimary;
            }
            else if (status == StepStatus::Warning)
            {
                bubbleColour = accentAmber;
                textColour = accentAmber;
            }

            bool isLocked = isStepLocked(step);
            if (isLocked)
            {
                bubbleColour = bubbleColour.withAlpha(0.38f);
                textColour = textColour.withAlpha(0.38f);
            }

            // Draw bubble with subtle outer halo for active step
            if (isCurrent)
            {
                g.setColour(bubbleColour.withAlpha(0.12f));
                g.fillEllipse(center.getX() - (nodeRadius + 5.0f), center.getY() - (nodeRadius + 5.0f), (nodeRadius + 5.0f) * 2.0f, (nodeRadius + 5.0f) * 2.0f);
                g.setColour(bubbleColour);
                g.drawEllipse(center.getX() - nodeRadius, center.getY() - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 2.0f);
            }
            else if (status == StepStatus::Completed)
            {
                g.setColour(bubbleColour);
                g.fillEllipse(center.getX() - nodeRadius, center.getY() - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
            }
            else if (status == StepStatus::Skipped)
            {
                g.setColour(bubbleColour.withAlpha(0.20f));
                g.fillEllipse(center.getX() - nodeRadius, center.getY() - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
                g.setColour(bubbleColour);
                g.drawEllipse(center.getX() - nodeRadius, center.getY() - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 1.5f);
            }
            else
            {
                g.setColour(bubbleColour);
                g.drawEllipse(center.getX() - nodeRadius, center.getY() - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 1.5f);
            }

            // Draw inner glyph (Checkmark for completed, >> for skipped, number for current/pending)
            auto nodeBounds = juce::Rectangle<float>(center.getX() - nodeRadius, center.getY() - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            if (status == StepStatus::Completed)
            {
                g.setColour(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
                g.drawText(juce::String::fromUTF8(u8"\u2713"), nodeBounds, juce::Justification::centred, false);
            }
            else if (status == StepStatus::Skipped)
            {
                g.setColour(bubbleColour);
                g.drawText(juce::String::fromUTF8(u8"\u23ED"), nodeBounds, juce::Justification::centred, false);
            }
            else
            {
                g.setColour(isCurrent ? textPrimary : textMuted);
                g.drawText(juce::String(i), nodeBounds, juce::Justification::centred, false);
            }

            // Draw textual label next to bubble
            g.setFont(juce::FontOptions(12.0f, isCurrent ? juce::Font::bold : juce::Font::plain));
            g.setColour(textColour);

            float labelX = center.getX() + nodeRadius + 8.0f;
            float labelWidth = segmentWidth - (nodeRadius * 2.0f) - 12.0f;
            auto labelBounds = juce::Rectangle<float>(labelX, center.getY() - 10.0f, labelWidth, 20.0f);
            juce::String stepLabel = stepNames[step];
            if (isLocked)
                stepLabel += " [Bloqueado]";
            g.drawText(stepLabel, labelBounds, juce::Justification::centredLeft, true);

            // Hover highlight for accessible steps
            if (hoveredStep.has_value() && *hoveredStep == step && canNavigateTo(step))
            {
                g.setColour(accentGreen.withAlpha(0.12f));
                g.fillRoundedRectangle(center.getX() - 14.0f, center.getY() - 14.0f, segmentWidth * 0.72f, 28.0f, 6.0f);
            }
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        static constexpr Step visualOrder[5] = {
            Step::SystemInfo,
            Step::HardwareRouting,
            Step::CalibrateLoopback,
            Step::RunSession,
            Step::ExportReport
        };

        auto bounds = getLocalBounds().toFloat();
        float segmentWidth = bounds.getWidth() / 5.0f;
        int stepIdx = juce::jlimit(0, 4, static_cast<int>(event.position.getX() / segmentWidth));
        Step stepUnderMouse = visualOrder[stepIdx];

        if (hoveredStep != stepUnderMouse)
        {
            hoveredStep = stepUnderMouse;
            setMouseCursor(canNavigateTo(stepUnderMouse)
                           ? juce::MouseCursor::PointingHandCursor
                           : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoveredStep.reset();
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        static constexpr Step visualOrder[5] = {
            Step::SystemInfo,
            Step::HardwareRouting,
            Step::CalibrateLoopback,
            Step::RunSession,
            Step::ExportReport
        };

        auto bounds = getLocalBounds().toFloat();
        float segmentWidth = bounds.getWidth() / 5.0f;
        int stepIdx = juce::jlimit(0, 4, static_cast<int>(event.position.getX() / segmentWidth));
        Step targetStep = visualOrder[stepIdx];

        if (canNavigateTo(targetStep))
        {
            setCurrentStep(targetStep);
            if (onStepSelected != nullptr)
                onStepSelected(targetStep);
        }
    }

private:
    Step currentStep;
    std::optional<Step> hoveredStep;
    std::map<Step, StepStatus> stepStatuses;
    std::map<Step, juce::String> stepNames;
    std::map<Step, bool> lockedSteps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkflowStepperBar)
};

} // namespace abdaudiolab::gui
