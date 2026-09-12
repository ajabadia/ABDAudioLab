#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

class HardwareWiringDiagramComponent : public juce::Component
{
public:
    HardwareWiringDiagramComponent();
    ~HardwareWiringDiagramComponent() override = default;

    void paint(juce::Graphics& g) override;

    void setRouting(const juce::String& stimulus,
                    const juce::String& response,
                    const juce::String& notes,
                    bool isMidiAutonomous);

    void setPluginRouting(const juce::String& stimulusFrom,
                          const juce::String& stimulusTo,
                          const juce::String& responseFrom,
                          const juce::String& responseTo,
                          const juce::String& notes);

    void clear();

    [[nodiscard]] const juce::String& getStimulusText() const noexcept { return routingStimulusText; }
    [[nodiscard]] const juce::String& getResponseText() const noexcept { return routingResponseText; }
    [[nodiscard]] const juce::String& getNotesText() const noexcept { return routingNotesText; }
    [[nodiscard]] bool getIsMidiAutonomous() const noexcept { return isMidiAutonomous; }
    [[nodiscard]] bool getIsPluginVirtual() const noexcept { return isPluginVirtual; }
    [[nodiscard]] bool isEmpty() const noexcept { return isCleared; }

private:
    juce::String routingStimulusText;
    juce::String routingResponseText;
    juce::String routingNotesText;
    juce::String plugStimFrom;
    juce::String plugStimTo;
    juce::String plugRespFrom;
    juce::String plugRespTo;
    bool isMidiAutonomous { false };
    bool isPluginVirtual { false };
    bool isCleared { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwareWiringDiagramComponent)
};

} // namespace abdaudiolab::gui
