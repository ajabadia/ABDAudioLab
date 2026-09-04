#include "TestConfigModal.h"

namespace abdaudiolab::gui
{

TestConfigModal::TestConfigModal()
{
    setWantsKeyboardFocus(true);

    lblTitle.setText("TEST CONFIGURATION & MATRIX RESOLUTION", juce::dontSendNotification);
    lblTitle.setFont(AppTheme::fontBold(12.0f));
    lblTitle.setColour(juce::Label::textColourId, AppTheme::TextPrimary);
    panel.addAndMakeVisible(lblTitle);

    btnClose.setTooltip("Close configuration dialog");
    btnClose.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnClose.setColour(juce::TextButton::textColourOffId, AppTheme::TextSecondary);
    btnClose.onClick = [this] { dismissDialog(); };
    panel.addAndMakeVisible(btnClose);

    editorViewport.setScrollBarsShown(true, false);
    editorViewport.setViewedComponent(&editorPanel, false);
    panel.addAndMakeVisible(editorViewport);

    btnCancel.setTooltip("Cancel - Discard parameter changes and close");
    btnCancel.setColour(juce::TextButton::buttonColourId, AppTheme::SurfaceCard);
    btnCancel.setColour(juce::TextButton::textColourOffId, AppTheme::TextPrimary);
    btnCancel.onClick = [this] { dismissDialog(); };
    panel.addAndMakeVisible(btnCancel);

    btnApply.setTooltip("Apply & Save - Update test plan with modified parameters and sweep resolution");
    btnApply.setColour(juce::TextButton::buttonColourId, AppTheme::AccentActive);
    btnApply.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnApply.onClick = [this] {
        if (onConfigurationConfirmed)
            onConfigurationConfirmed(editorPanel.getConfiguration());
        dismissDialog();
    };
    panel.addAndMakeVisible(btnApply);

    addAndMakeVisible(panel);
    setVisible(false);
}

void TestConfigModal::showDialog(juce::Component* parent, const TestConfiguration& initialConfig)
{
    editorPanel.setConfiguration(initialConfig);

    if (parent != nullptr)
    {
        setBounds(parent->getLocalBounds());
        parent->addAndMakeVisible(this);
    }

    setVisible(true);
    resized();
    editorPanel.resized();
    editorViewport.setViewedComponent(&editorPanel, false);
}

void TestConfigModal::dismissDialog()
{
    setVisible(false);
}

juce::Rectangle<float> TestConfigModal::getPanelBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    float w = std::min(680.0f, bounds.getWidth() * 0.94f);
    float h = std::min(640.0f, bounds.getHeight() * 0.94f);
    return bounds.withSizeKeepingCentre(w, h);
}

void TestConfigModal::paint(juce::Graphics& g)
{
    if (!isVisible()) return;

    g.fillAll(juce::Colours::black.withAlpha(0.60f));

    auto card = getPanelBounds();
    g.setColour(AppTheme::SurfaceCard);
    g.fillRoundedRectangle(card, AppTheme::CardCornerRadius);
    g.setColour(AppTheme::BorderSubtle);
    g.drawRoundedRectangle(card, AppTheme::CardCornerRadius, 1.5f);
}

void TestConfigModal::resized()
{
    auto card = getPanelBounds();
    panel.setBounds(card.toNearestInt());

    auto pBounds = panel.getLocalBounds();

    auto header = pBounds.removeFromTop(44).reduced(16, 8);
    btnClose.setBounds(header.removeFromRight(24).withSizeKeepingCentre(24, 24));
    lblTitle.setBounds(header);

    auto bottomBar = pBounds.removeFromBottom(56).reduced(16, 8);
    btnApply.setBounds(bottomBar.removeFromRight(180).withHeight(38));
    bottomBar.removeFromRight(10);
    btnCancel.setBounds(bottomBar.removeFromRight(100).withHeight(34));

    editorViewport.setBounds(pBounds.reduced(16, 4));

    int contentW = editorViewport.getWidth() - (editorViewport.isVerticalScrollBarShown() ? 10 : 0);
    editorPanel.setSize(contentW, std::max(editorViewport.getHeight(), editorPanel.getPreferredHeight()));
}

bool TestConfigModal::keyPressed(const juce::KeyPress& key)
{
    if (!isVisible()) return false;
    if (key.isKeyCode(juce::KeyPress::escapeKey))
    {
        dismissDialog();
        return true;
    }
    return false;
}

void TestConfigModal::mouseDown(const juce::MouseEvent& e)
{
    if (!getPanelBounds().contains(e.position))
    {
        dismissDialog();
    }
}

} // namespace abdaudiolab::gui
