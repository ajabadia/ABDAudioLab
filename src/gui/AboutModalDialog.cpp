#include "AboutModalDialog.h"
#include "../BuildVersion.h"

namespace abdaudiolab::gui
{

void AboutModalDialog::CardComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Drop shadow & Card background
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(bounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = bounds.reduced(28.0f, 24.0f);

    // 2. Header: Title + Version Badge
    auto headerRow = content.removeFromTop(28.0f);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("ABDAudioLab", headerRow.removeFromLeft(145.0f), juce::Justification::centredLeft, true);

    auto badgeRect = headerRow.removeFromLeft(120.0f).reduced(0.0f, 3.0f);
    g.setColour(juce::Colour(0xfff3f4f6));
    g.fillRoundedRectangle(badgeRect, 4.0f);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(juce::Colour(0xff374151));
    g.drawText(juce::String::fromUTF8(u8"v") + juce::String(version::kAppVersion) + juce::String::fromUTF8(u8" • #") + juce::String(version::kBuildNumber), badgeRect, juce::Justification::centred, true);

    // Subtitle
    g.setFont(juce::FontOptions(12.0f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("Hardware Profiling & LUT Generation Lab from ABDSynths", content.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    content.removeFromTop(14.0f);

    // Separator line
    g.setColour(juce::Colour(0xffe5e7eb));
    g.fillRect(content.removeFromTop(1.0f));
    content.removeFromTop(14.0f);

    // Key Architectural Features
    auto drawBullet = [&](const juce::String& boldPrefix, const juce::String& text) {
        auto row = content.removeFromTop(20.0f);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(boldPrefix, row.removeFromLeft(165.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions(11.0f));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(text, row, juce::Justification::centredLeft, true);
    };

    drawBullet(juce::String::fromUTF8(u8"• Real-Time DSP Safety:"), "Lock-free FIFO & zero-allocation audio threads (JUCE 8).");
    drawBullet(juce::String::fromUTF8(u8"• Farina Deconvolution:"), "Harmonic distortion profiling (H2-H5) & impulse responses.");
    drawBullet(juce::String::fromUTF8(u8"• Automated Profiling:"), "SysEx, NRPN, MIDI CC & manual step-by-step matrix.");
    drawBullet(juce::String::fromUTF8(u8"• Export Formats:"), "High-precision C++ Look-Up Tables & session_manifest.json.");

    content.removeFromTop(12.0f);

    // Copyright
    g.setFont(juce::FontOptions(10.5f));
    g.setColour(juce::Colour(0xff9ca3af));
    g.drawText(juce::String::fromUTF8(u8"© 2026 ABDSynths. All rights reserved."), content.removeFromTop(16.0f), juce::Justification::centred, true);
}

AboutModalDialog::AboutModalDialog()
{
    addAndMakeVisible(card);

    btnClose.setButtonText("Close");
    btnClose.setTooltip("Close dialog");
    btnClose.onClick = [this] { hideDialog(); };
    btnClose.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillBlackBg);
    btnClose.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    card.addAndMakeVisible(btnClose);

    setAlwaysOnTop(true);
    setVisible(false);
    setWantsKeyboardFocus(true);
}

void AboutModalDialog::showDialog(juce::Component* parent)
{
    if (parent == nullptr) return;
    parent->addAndMakeVisible(this);
    setBounds(parent->getLocalBounds());
    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
}

void AboutModalDialog::hideDialog()
{
    setVisible(false);
    if (auto* parent = getParentComponent())
    {
        parent->removeChildComponent(this);
    }
}

void AboutModalDialog::paint(juce::Graphics& g)
{
    // Semi-transparent dark overlay
    g.setColour(juce::Colours::black.withAlpha(0.40f));
    g.fillRect(getLocalBounds());
}

void AboutModalDialog::resized()
{
    int cardWidth = std::min(520, getWidth() - 40);
    int cardHeight = 310;
    card.setBounds((getWidth() - cardWidth) / 2, (getHeight() - cardHeight) / 2, cardWidth, cardHeight);

    auto cardBounds = card.getLocalBounds();
    btnClose.setBounds(cardBounds.getRight() - 120, cardBounds.getBottom() - 44, 96, 28);
}

void AboutModalDialog::mouseDown(const juce::MouseEvent& e)
{
    if (!card.getBounds().contains(e.getPosition()))
    {
        hideDialog();
    }
}

bool AboutModalDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hideDialog();
        return true;
    }
    return false;
}

} // namespace abdaudiolab::gui
