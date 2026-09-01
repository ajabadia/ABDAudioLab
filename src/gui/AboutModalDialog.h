#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

/**
 * @brief Elegant Light-Themed Modal Dialog for ABDAudioLab information.
 */
class AboutModalDialog : public juce::Component
{
public:
    AboutModalDialog();
    ~AboutModalDialog() override = default;

    void showDialog(juce::Component* parent);
    void hideDialog();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class CardComponent : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        juce::String appVersion;
        int buildNumber { 120 };
    };

    CardComponent card;
    juce::TextButton btnClose { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutModalDialog)
};

} // namespace abdaudiolab::gui
