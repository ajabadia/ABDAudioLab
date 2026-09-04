#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief Sonarworks SoundID Reference inspired Light LookAndFeel theme.
 * 
 * Clean, high-contrast, modern Scandinavian design language.
 */
class SoundIdTheme : public juce::LookAndFeel_V4
{
public:
    // Color Palette
    static inline const juce::Colour bgLight          { 0xfff5f5f7 }; // Clean Nordic light background (Apple style)
    static inline const juce::Colour bgCard           { 0xfff3f4f6 };
    static inline const juce::Colour bgCardHover      { 0xffe5e7eb };
    static inline const juce::Colour borderSubtle     { 0xffe5e7eb };
    static inline const juce::Colour borderCard       { 0xffd1d5db };
    
    static inline const juce::Colour textPrimary      { 0xff111827 };
    static inline const juce::Colour textSecondary    { 0xff4b5563 };
    static inline const juce::Colour textMuted        { 0xff6b7280 };

    static inline const juce::Colour accentGreen      { 0xff1db954 }; // Spotify / Pro-Audio emerald green
    static inline const juce::Colour accentPurple     { 0xff8b5cf6 };
    static inline const juce::Colour accentPurpleFill { 0x288b5cf6 }; // 16% opacity lilac
    static inline const juce::Colour accentAmber      { 0xfff5a623 }; // Technical amber for warnings & THD%
    static inline const juce::Colour accentRed        { 0xffd0021b }; // High-contrast warning/critical red

    static inline const juce::Colour pillBlackBg      { 0xff111827 };
    static inline const juce::Colour pillWhiteBg      { 0xffffffff };

    SoundIdTheme()
    {
        setColour(juce::ResizableWindow::backgroundColourId, bgLight);
        setColour(juce::DocumentWindow::backgroundColourId, bgLight);
        setColour(juce::DialogWindow::backgroundColourId, bgLight);
        setColour(juce::AlertWindow::backgroundColourId, pillWhiteBg);
        setColour(juce::AlertWindow::textColourId, textPrimary);
        setColour(juce::AlertWindow::outlineColourId, borderCard);

        setColour(juce::Label::textColourId, textPrimary);
        setColour(juce::TextButton::textColourOffId, textPrimary);
        setColour(juce::TextButton::textColourOnId, textPrimary);
        setColour(juce::TextButton::buttonColourId, pillWhiteBg);
        
        setColour(juce::ToggleButton::textColourId, textPrimary);
        setColour(juce::ToggleButton::tickColourId, accentGreen);

        setColour(juce::ListBox::backgroundColourId, pillWhiteBg);
        setColour(juce::ListBox::outlineColourId, borderSubtle);
        setColour(juce::ListBox::textColourId, textPrimary);
        setColour(juce::PropertyComponent::labelTextColourId, textPrimary);

        setColour(juce::ComboBox::backgroundColourId, pillWhiteBg);
        setColour(juce::ComboBox::textColourId, textPrimary);
        setColour(juce::ComboBox::outlineColourId, borderCard);
        setColour(juce::ComboBox::arrowColourId, textSecondary);

        setColour(juce::PopupMenu::backgroundColourId, pillWhiteBg);
        setColour(juce::PopupMenu::textColourId, textPrimary);
        setColour(juce::PopupMenu::headerTextColourId, textSecondary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, bgCardHover);
        setColour(juce::PopupMenu::highlightedTextColourId, textPrimary);

        setColour(juce::ProgressBar::backgroundColourId, bgCard);
        setColour(juce::ProgressBar::foregroundColourId, accentGreen);

        setColour(juce::TooltipWindow::backgroundColourId, pillBlackBg);
        setColour(juce::TooltipWindow::textColourId, juce::Colours::white);
        setColour(juce::TooltipWindow::outlineColourId, borderCard);

        // TextEditor colours (Sonarworks Nordic Light style: off-white background, dark text, subtle border)
        setColour(juce::TextEditor::backgroundColourId, pillWhiteBg);
        setColour(juce::TextEditor::textColourId, textPrimary);
        setColour(juce::TextEditor::highlightColourId, accentPurpleFill);
        setColour(juce::TextEditor::highlightedTextColourId, textPrimary);
        setColour(juce::TextEditor::outlineColourId, borderCard);
        setColour(juce::TextEditor::focusedOutlineColourId, accentGreen);
        setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

        // Slider colours
        setColour(juce::Slider::trackColourId, bgCardHover);
        setColour(juce::Slider::thumbColourId, pillWhiteBg);
        setColour(juce::Slider::backgroundColourId, bgCard);
    }

    juce::Font getTextButtonFont(juce::TextButton& /*button*/, int buttonHeight) override
    {
        return juce::FontOptions("Inter", std::clamp(static_cast<float>(buttonHeight) * 0.42f, 11.0f, 14.0f), juce::Font::bold);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (label.getName() == "MainTitle")
            return juce::FontOptions("Inter", 20.0f, juce::Font::bold);
        if (label.getName() == "SectionHeader")
            return juce::FontOptions("Inter", 14.5f, juce::Font::bold);
        if (label.getName() == "TechnicalData")
            return juce::FontOptions("Consolas", 12.0f, juce::Font::plain);
        return juce::FontOptions("Inter", 12.5f, juce::Font::plain);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(backgroundColour);
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        constexpr float cornerSize = 6.0f; // Standardized pro-audio corner radius

        auto baseColour = button.findColour(juce::TextButton::buttonColourId);

        if (baseColour == pillWhiteBg || baseColour == juce::Colours::white)
        {
            juce::Colour fillCol = shouldDrawButtonAsDown ? bgCardHover.darker(0.12f)
                                 : (shouldDrawButtonAsHighlighted ? bgCardHover : pillWhiteBg);
            g.setColour(fillCol);
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(borderCard.withAlpha(0.6f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
        else if (baseColour == pillBlackBg)
        {
            juce::Colour fillCol = shouldDrawButtonAsDown ? juce::Colour(0xff030712)
                                 : (shouldDrawButtonAsHighlighted ? juce::Colour(0xff1f2937) : pillBlackBg);
            g.setColour(fillCol);
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
        else
        {
            // Custom accent color button (e.g., accentGreen, accentRed, accentAmber)
            juce::Colour fillCol = shouldDrawButtonAsDown ? baseColour.darker(0.20f)
                                 : (shouldDrawButtonAsHighlighted ? baseColour.brighter(0.10f) : baseColour);
            g.setColour(fillCol);
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(baseColour.darker(0.25f).withAlpha(0.4f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        g.setColour(pillWhiteBg);
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(box.hasKeyboardFocus(true) ? accentGreen : borderCard);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        auto arrowArea = bounds.removeFromRight(24.0f);
        juce::Path arrow;
        float cx = arrowArea.getCentreX();
        float cy = arrowArea.getCentreY();
        arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
        arrow.lineTo(cx, cy + 2.5f);
        arrow.lineTo(cx + 4.0f, cy - 2.0f);

        g.setColour(textSecondary);
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        if (textEditor.isEnabled())
        {
            auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
            g.setColour(textEditor.hasKeyboardFocus(true) ? accentGreen : borderCard);
            g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));

        float trackWidth = 4.0f;
        float trackX = bounds.getCentreX() - trackWidth * 0.5f;

        // Groove Track
        g.setColour(bgCardHover);
        g.fillRoundedRectangle(trackX, bounds.getY(), trackWidth, bounds.getHeight(), 2.0f);

        // Thumb
        float thumbDiameter = 16.0f;
        float thumbX = bounds.getCentreX() - thumbDiameter * 0.5f;
        float thumbY = sliderPos - thumbDiameter * 0.5f;

        g.setColour(borderCard);
        g.fillEllipse(thumbX - 1.0f, thumbY - 1.0f, thumbDiameter + 2.0f, thumbDiameter + 2.0f);

        g.setColour(pillWhiteBg);
        g.fillEllipse(thumbX, thumbY, thumbDiameter, thumbDiameter);

        g.setColour(textSecondary);
        g.fillEllipse(thumbX + 5.0f, thumbY + 5.0f, 6.0f, 6.0f);
    }

    void drawAlertBox(juce::Graphics& g,
                      juce::AlertWindow& alert,
                      const juce::Rectangle<int>& bounds,
                      juce::TextLayout& textLayout) override
    {
        auto b = bounds.toFloat();

        g.setColour(pillWhiteBg);
        g.fillRoundedRectangle(b, 12.0f);

        g.setColour(borderCard);
        g.drawRoundedRectangle(b.reduced(0.5f), 12.0f, 1.2f);

        textLayout.draw(g, alert.getLocalBounds().toFloat());
    }
};

} // namespace abdaudiolab::gui
