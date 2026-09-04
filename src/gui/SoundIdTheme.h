#pragma once

#include "AppTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief Sonarworks SoundID Reference inspired Light LookAndFeel theme ("Técnica Refinada").
 * 
 * Clean, high-contrast, modern Scandinavian design language using AppTheme tokens.
 */
class SoundIdTheme : public juce::LookAndFeel_V4
{
public:
    // Aliases to AppTheme for backward compatibility and direct convenience
    static inline juce::Colour bgLight          = AppTheme::BackgroundApp;
    static inline juce::Colour bgCard           = AppTheme::SurfaceCard;
    static inline juce::Colour bgCardHover      = AppTheme::SurfaceHover;
    static inline juce::Colour surfaceSubtle    = AppTheme::SurfaceSubtle;
    static inline juce::Colour borderSubtle     = AppTheme::BorderSubtle;
    static inline juce::Colour borderCard       = AppTheme::BorderCard;
    
    static inline juce::Colour textPrimary      = AppTheme::TextPrimary;
    static inline juce::Colour textSecondary    = AppTheme::TextSecondary;
    static inline juce::Colour textMuted        = AppTheme::TextMuted;

    static inline juce::Colour accentGreen      = AppTheme::AccentActive;
    static inline juce::Colour accentPurple     { 0xff8b5cf6 };
    static inline juce::Colour accentPurpleFill { 0x288b5cf6 }; // 16% opacity lilac
    static inline juce::Colour accentAmber      = AppTheme::AccentWarning;
    static inline juce::Colour accentRed        = AppTheme::AccentError;

    static inline juce::Colour pillBlackBg      = AppTheme::PillBlackBg;
    static inline juce::Colour pillWhiteBg      = AppTheme::PillWhiteBg;

    SoundIdTheme()
    {
        updateColours();
    }

    void updateColours()
    {
        setColour(juce::ResizableWindow::backgroundColourId, AppTheme::BackgroundApp);
        setColour(juce::DocumentWindow::backgroundColourId, AppTheme::BackgroundApp);
        setColour(juce::DialogWindow::backgroundColourId, AppTheme::BackgroundApp);
        setColour(juce::AlertWindow::backgroundColourId, AppTheme::SurfaceCard);
        setColour(juce::AlertWindow::textColourId, AppTheme::TextPrimary);
        setColour(juce::AlertWindow::outlineColourId, AppTheme::BorderCard);

        setColour(juce::Label::textColourId, AppTheme::TextPrimary);
        setColour(juce::TextButton::textColourOffId, AppTheme::TextPrimary);
        setColour(juce::TextButton::textColourOnId, AppTheme::TextPrimary);
        setColour(juce::TextButton::buttonColourId, AppTheme::SurfaceCard);
        setColour(juce::TextButton::buttonOnColourId, AppTheme::AccentActive);
        
        setColour(juce::ToggleButton::textColourId, AppTheme::TextPrimary);
        setColour(juce::ToggleButton::tickColourId, AppTheme::AccentActive);

        setColour(juce::ListBox::backgroundColourId, AppTheme::SurfaceCard);
        setColour(juce::ListBox::outlineColourId, AppTheme::BorderSubtle);
        setColour(juce::ListBox::textColourId, AppTheme::TextPrimary);
        setColour(juce::PropertyComponent::labelTextColourId, AppTheme::TextPrimary);

        setColour(juce::ComboBox::backgroundColourId, AppTheme::SurfaceCard);
        setColour(juce::ComboBox::textColourId, AppTheme::TextPrimary);
        setColour(juce::ComboBox::outlineColourId, AppTheme::BorderSubtle);
        setColour(juce::ComboBox::arrowColourId, AppTheme::TextSecondary);

        setColour(juce::PopupMenu::backgroundColourId, AppTheme::SurfaceCard);
        setColour(juce::PopupMenu::textColourId, AppTheme::TextPrimary);
        setColour(juce::PopupMenu::headerTextColourId, AppTheme::TextSecondary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, AppTheme::SurfaceSubtle);
        setColour(juce::PopupMenu::highlightedTextColourId, AppTheme::TextPrimary);

        setColour(juce::ProgressBar::backgroundColourId, AppTheme::BorderSubtle);
        setColour(juce::ProgressBar::foregroundColourId, AppTheme::AccentActive);

        setColour(juce::TooltipWindow::backgroundColourId, AppTheme::PillBlackBg);
        setColour(juce::TooltipWindow::textColourId, juce::Colours::white);
        setColour(juce::TooltipWindow::outlineColourId, AppTheme::BorderSubtle);

        // TextEditor colours
        setColour(juce::TextEditor::backgroundColourId, AppTheme::SurfaceCard);
        setColour(juce::TextEditor::textColourId, AppTheme::TextPrimary);
        setColour(juce::TextEditor::highlightColourId, AppTheme::AccentActive.withAlpha(0.20f));
        setColour(juce::TextEditor::highlightedTextColourId, AppTheme::TextPrimary);
        setColour(juce::TextEditor::outlineColourId, AppTheme::BorderSubtle);
        setColour(juce::TextEditor::focusedOutlineColourId, AppTheme::AccentActive);
        setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

        // Slider colours
        setColour(juce::Slider::trackColourId, AppTheme::SurfaceSubtle);
        setColour(juce::Slider::thumbColourId, AppTheme::SurfaceCard);
        setColour(juce::Slider::backgroundColourId, AppTheme::BackgroundApp);
    }

    static void applyThemeMode(AppTheme::ThemeMode mode, SoundIdTheme* instance = nullptr)
    {
        AppTheme::setMode(mode);

        bgLight          = AppTheme::BackgroundApp;
        bgCard           = AppTheme::SurfaceCard;
        bgCardHover      = AppTheme::SurfaceHover;
        surfaceSubtle    = AppTheme::SurfaceSubtle;
        borderSubtle     = AppTheme::BorderSubtle;
        borderCard       = AppTheme::BorderCard;
        textPrimary      = AppTheme::TextPrimary;
        textSecondary    = AppTheme::TextSecondary;
        textMuted        = AppTheme::TextMuted;
        accentGreen      = AppTheme::AccentActive;
        accentAmber      = AppTheme::AccentWarning;
        accentRed        = AppTheme::AccentError;
        pillBlackBg      = AppTheme::PillBlackBg;
        pillWhiteBg      = AppTheme::PillWhiteBg;

        if (instance != nullptr)
        {
            instance->updateColours();
        }
    }

    int getDefaultScrollbarWidth() override
    {
        return 6;
    }

    juce::Font getTextButtonFont(juce::TextButton& /*button*/, int buttonHeight) override
    {
        return juce::FontOptions("Inter", std::clamp(static_cast<float>(buttonHeight) * 0.42f, 11.0f, 14.0f), juce::Font::bold);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (label.getName() == "MainTitle")
            return juce::FontOptions("Inter", 22.0f, juce::Font::bold);
        if (label.getName() == "SectionHeader")
            return juce::FontOptions("Inter", 14.5f, juce::Font::bold);
        if (label.getName() == "TechnicalData")
            return juce::FontOptions("Roboto Mono", 12.0f, juce::Font::plain);
        return juce::FontOptions("Inter", 12.5f, juce::Font::plain);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(backgroundColour);
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        auto baseColour = button.findColour(juce::TextButton::buttonColourId);

        // 1. Completely Borderless / Icon Button Mode
        if (baseColour.isTransparent() || baseColour == juce::Colours::transparentBlack || baseColour == juce::Colours::transparentWhite)
        {
            float r = std::clamp(bounds.getHeight() * 0.35f, 4.0f, 8.0f);
            if (shouldDrawButtonAsDown)
            {
                g.setColour(AppTheme::SurfaceHover.darker(0.10f));
                g.fillRoundedRectangle(bounds, r);
            }
            else if (shouldDrawButtonAsHighlighted)
            {
                g.setColour(AppTheme::SurfaceHover);
                g.fillRoundedRectangle(bounds, r);
            }
            return;
        }

        // Hero Pill vs Standard Action Button
        bool isPill = (button.getHeight() <= 40 && button.getWidth() > button.getHeight() * 1.8f) ||
                      (button.getComponentID().containsIgnoreCase("pill") || button.getComponentID().containsIgnoreCase("run"));
        float cornerSize = isPill ? (bounds.getHeight() * 0.5f) : AppTheme::cornerControl;

        // 2. Standard White / Neutral Card Button
        if (baseColour == AppTheme::SurfaceCard || baseColour == AppTheme::SurfaceSubtle ||
            baseColour == AppTheme::PillWhiteBg || baseColour == juce::Colours::white)
        {
            juce::Colour fillCol = shouldDrawButtonAsDown ? AppTheme::SurfaceHover.darker(0.08f)
                                 : (shouldDrawButtonAsHighlighted ? AppTheme::SurfaceHover : baseColour);
            g.setColour(fillCol);
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(AppTheme::BorderCard);
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
        else if (baseColour == AppTheme::PillBlackBg)
        {
            juce::Colour fillCol = shouldDrawButtonAsDown ? juce::Colour(0xff030712)
                                 : (shouldDrawButtonAsHighlighted ? juce::Colour(0xff2d3748) : AppTheme::PillBlackBg);
            g.setColour(fillCol);
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(AppTheme::BorderCard);
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
        else
        {
            // 3. Custom Accent Color Button (e.g. AccentActive, AccentWarning, AccentError)
            juce::Colour fillCol = shouldDrawButtonAsDown ? baseColour.darker(0.12f)
                                 : (shouldDrawButtonAsHighlighted ? baseColour.brighter(0.06f) : baseColour);
            g.setColour(fillCol);
            g.fillRoundedRectangle(bounds, cornerSize);
        }
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        g.setColour(AppTheme::SurfaceCard);
        g.fillRoundedRectangle(bounds, AppTheme::cornerControl);

        g.setColour(box.hasKeyboardFocus(true) ? AppTheme::AccentActive : AppTheme::BorderSubtle);
        g.drawRoundedRectangle(bounds, AppTheme::cornerControl, 1.0f);

        auto arrowArea = bounds.removeFromRight(24.0f);
        juce::Path arrow;
        float cx = arrowArea.getCentreX();
        float cy = arrowArea.getCentreY();
        arrow.startNewSubPath(cx - 3.5f, cy - 2.0f);
        arrow.lineTo(cx, cy + 2.5f);
        arrow.lineTo(cx + 3.5f, cy - 2.0f);
        arrow.closeSubPath();

        g.setColour(AppTheme::TextSecondary);
        g.fillPath(arrow);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        if (textEditor.isEnabled())
        {
            auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
            g.setColour(textEditor.hasKeyboardFocus(true) ? AppTheme::AccentActive : AppTheme::BorderSubtle);
            g.drawRoundedRectangle(bounds, AppTheme::cornerControl, 1.0f);
        }
    }

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical,
                       int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override
    {
        juce::ignoreUnused(scrollbar);
        if (thumbSize <= 0) return;

        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        float thumbAlpha = isMouseDown ? 0.65f : (isMouseOver ? 0.45f : 0.28f);
        juce::Colour thumbCol = AppTheme::TextSecondary.withAlpha(thumbAlpha);

        if (isScrollbarVertical)
        {
            float tw = bounds.getWidth() - 2.0f;
            float tx = bounds.getX() + 1.0f;
            float ty = static_cast<float>(thumbStartPosition);
            float th = static_cast<float>(thumbSize);
            g.setColour(thumbCol);
            g.fillRoundedRectangle(tx, ty, tw, th, tw * 0.5f);
        }
        else
        {
            float th = bounds.getHeight() - 2.0f;
            float ty = bounds.getY() + 1.0f;
            float tx = static_cast<float>(thumbStartPosition);
            float tw = static_cast<float>(thumbSize);
            g.setColour(thumbCol);
            g.fillRoundedRectangle(tx, ty, tw, th, th * 0.5f);
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));

        float trackWidth = 3.0f;
        float trackX = bounds.getCentreX() - trackWidth * 0.5f;

        // Track
        g.setColour(AppTheme::SurfaceSubtle);
        g.fillRoundedRectangle(trackX, bounds.getY(), trackWidth, bounds.getHeight(), 1.5f);

        // Thumb
        float thumbDiameter = 14.0f;
        float thumbX = bounds.getCentreX() - thumbDiameter * 0.5f;
        float thumbY = sliderPos - thumbDiameter * 0.5f;

        g.setColour(AppTheme::BorderCard);
        g.fillEllipse(thumbX - 1.0f, thumbY - 1.0f, thumbDiameter + 2.0f, thumbDiameter + 2.0f);

        g.setColour(AppTheme::SurfaceCard);
        g.fillEllipse(thumbX, thumbY, thumbDiameter, thumbDiameter);

        g.setColour(AppTheme::AccentActive);
        g.fillEllipse(thumbX + 4.5f, thumbY + 4.5f, 5.0f, 5.0f);
    }

    void drawAlertBox(juce::Graphics& g,
                      juce::AlertWindow& alert,
                      const juce::Rectangle<int>& bounds,
                      juce::TextLayout& textLayout) override
    {
        auto b = bounds.toFloat();

        g.setColour(AppTheme::SurfaceCard);
        g.fillRoundedRectangle(b, AppTheme::cornerModal);

        g.setColour(AppTheme::BorderSubtle);
        g.drawRoundedRectangle(b.reduced(0.5f), AppTheme::cornerModal, 1.0f);

        textLayout.draw(g, alert.getLocalBounds().toFloat());
    }

    void drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header) override
    {
        auto b = header.getLocalBounds().toFloat();
        g.setColour(AppTheme::SurfaceSubtle);
        g.fillRect(b);
        g.setColour(AppTheme::BorderSubtle);
        g.drawHorizontalLine(header.getHeight() - 1, 0.0f, static_cast<float>(header.getWidth()));
    }

    void drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& /*header*/,
                               const juce::String& columnName, int /*columnId*/,
                               int width, int height, bool isMouseOver, bool /*isMouseDown*/,
                               int /*columnFlags*/) override
    {
        auto b = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        if (isMouseOver)
        {
            g.setColour(AppTheme::SurfaceHover.withAlpha(0.5f));
            g.fillRect(b);
        }

        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
        g.setColour(AppTheme::TextSecondary);
        g.drawText(columnName, b.reduced(6.0f, 0.0f), juce::Justification::centredLeft, true);

        g.setColour(AppTheme::BorderSubtle);
        g.drawVerticalLine(width - 1, 4.0f, static_cast<float>(height - 4));
    }
};

} // namespace abdaudiolab::gui
