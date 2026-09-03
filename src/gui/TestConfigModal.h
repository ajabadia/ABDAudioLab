#pragma once

#include "SoundIdTheme.h"
#include "TestConfiguration.h"
#include "TestEditorPanel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace abdaudiolab::gui
{

/**
 * @brief Spacious Nordic light-themed modal wrapping TestEditorPanel for standalone test configuration.
 */
class TestConfigModal : public juce::Component
{
public:
    TestConfigModal();
    ~TestConfigModal() override = default;

    void showDialog(juce::Component* parent, const TestConfiguration& initialConfig);
    void dismissDialog();

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;

    [[nodiscard]] const TestConfiguration& getConfiguration() const noexcept { return editorPanel.getConfiguration(); }

    std::function<void(const TestConfiguration&)> onConfigurationConfirmed;

private:
    juce::Component panel;
    juce::TextButton btnClose { "X" };
    juce::Label lblTitle;

    TestEditorPanel editorPanel;
    juce::Viewport editorViewport;

    juce::TextButton btnCancel { "Cancel" };
    juce::TextButton btnApply { "Apply Configuration" };

    juce::Rectangle<float> getPanelBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestConfigModal)
};

} // namespace abdaudiolab::gui
