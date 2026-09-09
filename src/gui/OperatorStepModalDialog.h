#pragma once

#include "RhythmicMetronomeComponent.h"
#include "../core/ProfilingSession.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <functional>

namespace abdaudiolab::gui
{

class OperatorCardsContainerComponent;

/**
 * @brief Graphical Operator Step Modal for ABDAudioLab.
 * Displays vector rendered Knobs, Sliders, and Female Jack Ports
 * showing exact target positions for manual hardware alignment.
 */
class OperatorStepModalDialog : public juce::Component,
                                public juce::KeyListener
{
public:
    OperatorStepModalDialog();
    ~OperatorStepModalDialog() override;

    std::function<void()> onAccept;
    std::function<void()> onRepeat;
    std::function<void()> onStepBack;
    std::function<void()> onCancel;
    std::function<void(bool isCollapsed)> onCollapseToggled;
    std::function<void()> onCloseInspector;
    std::function<void(int secondTick)> onMetronomeTick;

    bool isAutomatedMode { false };
    bool isMeasuring { false };
    bool isCollapsed { false };
    bool isInspectorMode { false };
    bool isMetronomeMode { false };

    void setMetronomeMode(bool active);
    void updateTheme();
    void setAutomatedMode(bool autoMode);
    void setMeasuringState(bool measuring);
    void dismiss();

    void setStepInfo(const juce::String& sessionTitle,
                     int currentStep,
                     int totalSteps,
                     const std::vector<core::ParameterStep>& steps,
                     const juce::String& message = {});

    void showInspector(const juce::String& sessionTitle,
                       int currentStep,
                       int totalSteps,
                       const std::vector<core::ParameterStep>& steps,
                       const juce::String& metricsInfo = {});

    void showStepPrompt(juce::Component* parent,
                        const juce::String& sessionTitle,
                        int currentStep,
                        int totalSteps,
                        const std::vector<core::ParameterStep>& steps,
                        const juce::String& message = {});

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    std::unique_ptr<OperatorCardsContainerComponent> cardsContainer;
    juce::Viewport cardsViewport;
    RhythmicMetronomeComponent metronomeWidget;

    juce::TextButton btnAccept;
    juce::TextButton btnRepeat;
    juce::TextButton btnStepBack;
    juce::TextButton btnToggleMetronome;
    juce::TextButton btnCancel;
    juce::TextButton btnCloseInspector;
    juce::TextButton btnToggleCollapse;
    juce::Label lblAutoStatus;

    juce::String testTitle { "Manual Calibration Step" };
    juce::String promptMessage;
    int stepIndex { 1 };
    int stepTotal { 1 };
    std::vector<core::ParameterStep> parameterSteps;

    juce::Rectangle<float> getCardBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OperatorStepModalDialog)
};

} // namespace abdaudiolab::gui
