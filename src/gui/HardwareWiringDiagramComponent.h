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

    [[nodiscard]] const juce::String& getStimulusText() const noexcept { return routingStimulusText; }
    [[nodiscard]] const juce::String& getResponseText() const noexcept { return routingResponseText; }
    [[nodiscard]] const juce::String& getNotesText() const noexcept { return routingNotesText; }
    [[nodiscard]] bool getIsMidiAutonomous() const noexcept { return isMidiAutonomous; }

private:
    juce::String routingStimulusText;
    juce::String routingResponseText;
    juce::String routingNotesText;
    bool isMidiAutonomous { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwareWiringDiagramComponent)
};

} // namespace abdaudiolab::gui
