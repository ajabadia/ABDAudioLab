#pragma once

#include "SoundIdTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace abdaudiolab::gui
{

/**
 * @brief Professional Nordic Light Confirmation & Alert Modal for ABDAudioLab.
 * Replaces default dark AlertWindow with seamless SoundID styled dialogs.
 */
class ConfirmationModalDialog : public juce::Component,
                                 public juce::KeyListener
{
public:
    enum class Result
    {
        Primary,   // e.g. Save
        Secondary, // e.g. Don't Save
        Cancel     // e.g. Cancel
    };

    ConfirmationModalDialog()
    {
        setWantsKeyboardFocus(true);
        addKeyListener(this);

        btnPrimary.setButtonText("Save");
        btnPrimary.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillBlackBg);
        btnPrimary.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(btnPrimary);

        btnSecondary.setButtonText("Don't Save");
        btnSecondary.setColour(juce::TextButton::buttonColourId, SoundIdTheme::pillWhiteBg);
        btnSecondary.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
        addAndMakeVisible(btnSecondary);

        btnCancel.setButtonText("Cancel");
        btnCancel.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff3f4f6));
        btnCancel.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
        addAndMakeVisible(btnCancel);

        btnPrimary.onClick = [this] { finish(Result::Primary); };
        btnSecondary.onClick = [this] { finish(Result::Secondary); };
        btnCancel.onClick = [this] { finish(Result::Cancel); };
    }

    ~ConfirmationModalDialog() override
    {
        removeKeyListener(this);
    }

    void show(juce::Component* parent,
              const juce::String& titleText,
              const juce::String& bodyMessageText,
              const juce::String& primaryButtonText,
              const juce::String& secondaryButtonText,
              const juce::String& cancelButtonText,
              std::function<void(Result)> callback)
    {
        title = titleText;
        message = bodyMessageText;
        onResult = std::move(callback);

        btnPrimary.setButtonText(primaryButtonText);

        if (secondaryButtonText.isNotEmpty())
        {
            btnSecondary.setButtonText(secondaryButtonText);
            btnSecondary.setVisible(true);
        }
        else
        {
            btnSecondary.setVisible(false);
        }

        if (cancelButtonText.isNotEmpty())
        {
            btnCancel.setButtonText(cancelButtonText);
            btnCancel.setVisible(true);
        }
        else
        {
            btnCancel.setVisible(false);
        }

        if (parent != nullptr)
        {
            parent->addChildComponent(this);
            setBounds(parent->getLocalBounds());
            setVisible(true);
            toFront(true);
            grabKeyboardFocus();
        }
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (isVisible())
        {
            if (key.isKeyCode(juce::KeyPress::returnKey))
            {
                finish(Result::Primary);
                return true;
            }
            else if (key.isKeyCode(juce::KeyPress::escapeKey))
            {
                finish(Result::Cancel);
                return true;
            }
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Semi-transparent backdrop overlay
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillRect(bounds);

        // 2. Central Modal Card (Pure White SoundID style)
        float cardW = 540.0f;
        float cardH = 230.0f;
        auto card = bounds.withSizeKeepingCentre(cardW, cardH);

        // Drop shadow
        g.setColour(juce::Colour(0x20000000));
        g.fillRoundedRectangle(card.expanded(4.0f), 14.0f);

        // Card background & border
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(card, 12.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(card, 12.0f, 1.0f);

        // 3. Header Badge & Title
        auto inner = card.reduced(24.0f, 20.0f);
        auto header = inner.removeFromTop(44.0f);

        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText("ACTION CONFIRMATION REQUIRED", header.removeFromTop(16.0f), juce::Justification::left, true);

        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(title, header, juce::Justification::left, true);

        // 4. Message Body
        g.setFont(juce::FontOptions(12.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawFittedText(message, inner.removeFromTop(75.0f).toNearestInt(), juce::Justification::topLeft, 4);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().toFloat();
        float cardW = 540.0f;
        float cardH = 230.0f;
        auto card = bounds.withSizeKeepingCentre(cardW, cardH);

        auto bottomBar = card.removeFromBottom(52.0f).reduced(24.0f, 10.0f);

        btnPrimary.setBounds(bottomBar.removeFromRight(140.0f).toNearestInt());
        if (btnSecondary.isVisible())
        {
            bottomBar.removeFromRight(10.0f);
            btnSecondary.setBounds(bottomBar.removeFromRight(165.0f).toNearestInt());
        }
        if (btnCancel.isVisible())
        {
            btnCancel.setBounds(bottomBar.removeFromLeft(95.0f).toNearestInt());
        }
    }

private:
    juce::TextButton btnPrimary;
    juce::TextButton btnSecondary;
    juce::TextButton btnCancel;

    juce::String title { "Unsaved Changes" };
    juce::String message;
    std::function<void(Result)> onResult;

    void finish(Result r)
    {
        setVisible(false);
        if (onResult)
            onResult(r);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfirmationModalDialog)
};

} // namespace abdaudiolab::gui
